/*
 * Copyright (c) 2021 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/*
* \brief Parser and validation of serializations according to a schema
* \file schema.cpp
*/
#include "papuga/schema.h"
#include "papuga/errors.h"
#include "papuga/allocator.h"
#include "papuga/serialization.h"
#include "papuga/requestParser.h"
#include "papuga/valueVariant.h"
#include "textwolf.hpp"
#include "textwolf/xmlpathautomatonparse.hpp"
#include <string>
#include <map>
#include <bitset>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <inttypes.h>

#if defined __GNUC__ || defined __clang__
#define UNUSED __attribute__((unused)) 
#endif

typedef textwolf::XMLPathSelectAutomatonParser<> Automaton;
typedef textwolf::XMLPathSelectAutomaton<>::PathElement PathElement;
typedef textwolf::XMLPathSelect<textwolf::charset::UTF8> AutomatonState;

enum {MaxNofNodes = 64};

struct BitSet
{
	BitSet() :val(0){}
	BitSet( uint64_t val_) :val(val_){}
	BitSet( const BitSet& o) :val(o.val){}

	void set( int bit)
	{
		val |= (1 << (bit-1));
	}
	struct const_iterator
	{
		uint64_t val;
		int idx;

		const_iterator( uint64_t val_) :val(val_),idx(::ffsl(val_)){}
		const_iterator( const const_iterator& o) :val(o.val),idx(o.idx){}

		int operator*() const
		{
			return idx;
		}
		const_iterator& operator++()
		{
			skip(); return *this;
		}
		const_iterator operator++(int)
		{
			const_iterator rt( val); skip(); return rt;
		}
		bool operator == (const const_iterator& o) const
		{
			return val == o.val;
		}
		bool operator != (const const_iterator& o) const
		{
			return val != o.val;
		}
		void skip()
		{
			val &= ((1 << (idx-1)) - 1);
			idx = ::ffsl( val);
		}
	};

	const_iterator begin() const	{return const_iterator(val);}
	const_iterator end() const	{return const_iterator(0);}

	uint64_t val;
};

struct SchemaOperation
{
	enum Id {
		NameAtomic,
		NameArray,
		ValueInteger,
		ValueFloat,
		ValueBool,
		ValueString,
		AttributeInteger,
		AttributeFloat,
		AttributeBool,
		AttributeString,
		OpenNamedStructure,
		CloseNamedStructure,
		OpenNamedStructureArray,
		CloseNamedStructureArray,
		OpenStructure,
		CloseStructure,
		OpenStructureArray,
		CloseStructureArray
	};
	static const char* idName( Id id) noexcept
	{
		static const char* ar[] = {
			"NameAtomic",
			"NameArray",
			"ValueInteger",
			"ValueFloat",
			"ValueBool",
			"ValueString",
			"AttributeInteger",
			"AttributeFloat",
			"AttributeBool",
			"AttributeString",
			"OpenNamedStructure",
			"CloseNamedStructure",
			"OpenNamedStructureArray",
			"CloseNamedStructureArray",
			"OpenStructure",
			"CloseStructure",
			"OpenStructureArray",
			"CloseStructureArray",
			0
		};
		return ar[ id];
	}

	SchemaOperation( Id id_, uint64_t set_, uint64_t mask_) :id(id_),set(set_),mask(mask_) {}

	Id id;
	uint64_t set;
	uint64_t mask;
};

bool SchemaError( papuga_SchemaError* err, papuga_ErrorCode errcode, int line=0, const char* itm=0, int itmlen=0)
{
	err->code = errcode;
	err->line = line;
	if (itm)
	{
		char itmbuf[ 256];
		if (itmlen)
		{
			if (itmlen >= (int)sizeof(itmbuf)) itmlen = sizeof( itmbuf)-1;
			std::memcpy( itmbuf, itm, itmlen);
			itmbuf[ itmlen] = 0;
			itm = itmbuf;
		}
		if ((int)sizeof(err->item) <= std::snprintf( err->item, sizeof(err->item), "%s", itm))
		{
			err->item[ sizeof(err->item)-1] = 0;
		}
	}
	return false;
}

static int errorLine( char const* start, char const* ptr)
{
	char const* pi = start;
	int rt = 0;
	for (; pi != ptr; ++pi)
	{
		if (*pi == '\n') ++rt;
	}
	return rt+1;
}

bool RequestParserError( papuga_SchemaError* err, papuga_ErrorCode errcode, papuga_RequestParser* parser, char* contentstr)
{
	char errlocation[ 1024];
	int pos = papuga_RequestParser_get_position( parser, errlocation, sizeof(errlocation));
	int line = pos == -1 ? 0 : errorLine( contentstr, contentstr + pos);
	return SchemaError( err, errcode, line, errlocation);
}

struct papuga_Schema
{
	char const* name;
	Automaton atm;
	std::vector<SchemaOperation> ops;

	papuga_Schema( char const* name_) :name(name_),atm(),ops(){}
	~papuga_Schema(){}
};

struct papuga_SchemaMap
{
	papuga_Schema* ar;
	int arsize;
	papuga_Allocator allocator;

	papuga_SchemaMap() :ar(0),arsize(0),allocator(){}
	~papuga_SchemaMap(){}
};

struct papuga_SchemaList
{
	papuga_SchemaSource* ar;
	int arsize;
	papuga_Allocator allocator;

	papuga_SchemaList() :ar(0),arsize(0),allocator(){}
	~papuga_SchemaList(){}
};

struct SchemaNode
{
	const char* typnam;
	const char* name;
	SchemaNode* next;
	SchemaNode* chld;
	bool array;
	bool attribute;

	SchemaNode( const char* name_)
		:typnam(0),name(name_),next(0),chld(0),array(false),attribute(false){}
};

struct StringView
{
	const char* str;

	StringView( const char* str_) :str(str_){}

	bool operator < (const StringView& o) const {return compare(o) < 0;}
	bool operator == (const StringView& o) const {return compare(o) == 0;}
	bool operator == (const char* o) const {return std::strcmp( str, o) == 0;}

	int compare( const StringView& o) const
	{
		return std::strcmp( str, o.str);
	}
};

struct SchemaTree
{
	std::map<StringView,BitSet> nodemap;
	SchemaNode* nodear[ MaxNofNodes];
	std::size_t nodearsize;
	papuga_Allocator allocator;
	int allocator_buffer[ 8192];

	SchemaTree() :nodemap(),nodearsize(0)
	{
		papuga_init_Allocator( &allocator, allocator_buffer, sizeof(allocator_buffer));
	}
	~SchemaTree()
	{
		papuga_destroy_Allocator( &allocator);
	}
};

struct Lexeme
{
	const char* name;
	int namelen;
	enum Type {EndOfSource,Error,Open,Close,Equal,Attribute,Identifier,Comma};
	Type type;

	Lexeme( const char* name_, int namelen_, Type type_)
		:name(name_),namelen(namelen_),type(type_){}

	bool operator == (const char* str) const
	{
		return (0==std::memcmp( name, str, namelen) && str[ namelen] == '\0');
	}
};

static bool skipToEoln( char const*& src)
{
	for (; *src && *src != '\n'; ++src){}
	if (*src)
	{
		++src;
		return true;
	}
	else
	{
		return false;
	}
}
static bool skipSpaces( char const*& src)
{
AGAIN:
	for (; *src && (unsigned char)*src <= 32; ++src){}
	if (*src == '#' || (src[0] == '-' && src[1] == '-')) {skipToEoln( src); goto AGAIN;}
	return *src != 0;
}

static bool isAlpha( unsigned char ch)
{
	return ((ch|32) >= 'a' && (ch|32) <= 'z') || ch == '_';
}
static bool isDigit( unsigned char ch)
{
	return (ch >= '0' && ch <= '9');
}
static bool isAlphaNum( unsigned char ch)
{
	return isAlpha( ch) || isDigit( ch);
}
static bool stringToInt( int64_t& res,  char const* si)
{
	res = 0;
	int64_t res_prev = 0;
	bool sign = false;
	if (*si == '-')
	{
		sign = true;
		++si;
	}
	for (; isDigit(*si); ++si)
	{
		res_prev = res;
		res = res * 10 + (*si - '0');
		if (res < res_prev) return false;
	}
	return !*si;
}
static bool stringToDouble( double& res, char const* si)
{	
	char const* pi = si;
	if (*si == '-') ++si;
	for (; isDigit(*si); ++si){}
	if (*si == '.')
	{
		++si;
		for (; isDigit(*si); ++si){}
	}
	if ((*si | 32) == 'e')
	{
		++si;
		if (*si == '-') ++si;
		for (; isDigit(*si); ++si){}
	}
	return (!*si && 1==std::sscanf( pi, "%lf", &res));
}
static bool stringToBool( bool& res, char const* si)
{
	if (*si == 't' && 0==std::strcmp( si, "true")) {res = true; return true;}
	if (*si == 'f' && 0==std::strcmp( si, "false")) {res = false; return true;}
	if (*si == 'y' && 0==std::strcmp( si, "yes")) {res = true; return true;}
	if (*si == 'n' && 0==std::strcmp( si, "no")) {res = false; return true;}
	if (*si == '1' && !si[1]) {res = true; return true;}
	if (*si == '0' && !si[1]) {res = false; return true;}
	return false;
}

static Lexeme parseIdentifier( char const*& src)
{
	Lexeme rt( src, 1, Lexeme::Identifier);
	char const* start = src++;
	for (; isAlphaNum( *src); ++src){}
	rt.namelen = src-start;
	return rt;
}
static Lexeme parseNextLexeme( char const*& src)
{
	if (!skipSpaces( src)) return Lexeme( nullptr, 0, Lexeme::EndOfSource);
	if (*src == '{') {++src; return Lexeme( "{", 1, Lexeme::Open);}
	if (*src == '}') {++src; return Lexeme( "}", 1, Lexeme::Close);}
	if (*src == '=') {++src; return Lexeme( "=", 1, Lexeme::Equal);}
	if (*src == ',') {++src; return Lexeme( ",", 1, Lexeme::Comma);}
	if (*src == '@') {++src; return Lexeme( ",", 1, Lexeme::Attribute);}
	if (isAlpha( *src)) return parseIdentifier( src);
	return Lexeme( "", 0, Lexeme::Error);
}
static bool expectComma( char const*& src)
{
	char const* si = src;
	Lexeme lx = parseNextLexeme( si);
	if (lx.type == Lexeme::Close)
	{
		return true;
	}
	else if (lx.type == Lexeme::Comma)
	{
		src = si;
		return true;
	}
	return false;
}
static bool skipToEndBlock( char const*& src)
{
	int cnt = 0;
	Lexeme lx = parseNextLexeme( src);
	for (; lx.type != Lexeme::EndOfSource; lx = parseNextLexeme( src))
	{
		if (lx.type == Lexeme::Open) ++cnt;
		if (lx.type == Lexeme::Close) --cnt;
		if (cnt == 0) return true;
	}
	return false;
}

static bool parseSchemaSource( papuga_SchemaSource& res, papuga_Allocator& allocator, char const* src, char const*& si, papuga_SchemaError* err)
{
	Lexeme lx = parseNextLexeme( si);
	char const* start = si;
	if (lx.type == Lexeme::Error)
	{
		return SchemaError( err, papuga_SyntaxError, errorLine( src, si));
	}
	if (lx.type != Lexeme::Identifier)
	{
		return SchemaError( err, papuga_SyntaxError, errorLine( src, si), lx.name, lx.namelen);
	}
	res.name = papuga_Allocator_copy_string( &allocator, lx.name, lx.namelen);
	if (!res.name)
	{
		return SchemaError( err, papuga_NoMemError, errorLine( src, si));
	}
	lx = parseNextLexeme( si);
	if (lx.type == Lexeme::Error)
	{
		return SchemaError( err, papuga_SyntaxError, errorLine( src, si));
	}
	if (lx.type != Lexeme::Equal)
	{
		return SchemaError( err, papuga_SyntaxError, errorLine( src, si), lx.name, lx.namelen);
	}
	if (!skipToEndBlock( si))
	{
		return SchemaError( err, papuga_SyntaxError, errorLine( src, si));
	}
	for (; *si && (unsigned char)*si <= 32; ++si){}
	res.source = papuga_Allocator_copy_string( &allocator, start, si-start+1);
	if (!res.source)
	{
		return SchemaError( err, papuga_NoMemError, errorLine( src, si));
	}
	res.lines = errorLine( start, si);
	return true;
}

static bool parseSchemaTree( SchemaTree& tree, char const* src, papuga_SchemaError* err)
{
	char const* si = src;
	struct StackElem
	{
		SchemaNode* node;
		SchemaNode* tail;

		StackElem( SchemaNode* node_, SchemaNode* tail_) :node(node_),tail(tail_){}
	};
	struct Stack
	{
		std::vector<StackElem> ar;

		void push( SchemaNode* node)
		{
			ar.push_back( StackElem( node, 0));
		}
		bool link( SchemaNode* node, SchemaTree& tree, papuga_SchemaError* err)
		{
			if (ar.empty())
			{
				if (tree.nodearsize >= MaxNofNodes)
				{
					return SchemaError( err, papuga_ComplexityOfProblem);
				}
				tree.nodear[ tree.nodearsize++] = node;
				tree.nodemap[ StringView(node->name)].set( tree.nodearsize);
			}
			else if (ar.back().tail)
			{
				ar.back().tail->next = node;
				ar.back().tail = node;
			}
			else if (ar.back().node->chld)
			{
				return SchemaError( err, papuga_LogicError);
			}
			else
			{
				ar.back().node->chld = node;
				ar.back().tail = node;
			}
			return true;
		}
		bool empty() const
		{
			return ar.empty();
		}
	};
	Stack stack;
	Lexeme lx = parseNextLexeme( si);
	for (; lx.type != Lexeme::EndOfSource; lx = parseNextLexeme( si))
	{
		if (lx.type == Lexeme::Error)
		{
			return SchemaError( err, papuga_SyntaxError, errorLine( src, si));
		}
		bool attribute = false;
		if (lx.type == Lexeme::Attribute)
		{
			attribute = true;
			lx = parseNextLexeme( si);
			if (lx.type != Lexeme::Identifier)
			{
				return SchemaError( err, papuga_SyntaxError, errorLine( src, si), lx.name, lx.namelen);
			}
		}
		if (lx.type == Lexeme::Identifier)
		{
			const char* lxnam = papuga_Allocator_copy_string( &tree.allocator, lx.name, lx.namelen);
			void* nodemem = papuga_Allocator_alloc( &tree.allocator, sizeof(SchemaNode), sizeof(char*));
			if (!lxnam || !nodemem) throw std::bad_alloc();
			SchemaNode* node = new (nodemem) SchemaNode( lxnam);

			lx = parseNextLexeme( si);
			if (lx.type != Lexeme::Equal)
			{
				return SchemaError( err, papuga_SyntaxError, errorLine( src, si));
			}
			lx = parseNextLexeme( si);

			if (lx.type == Lexeme::Open)
			{
				if (attribute)
				{
					return SchemaError( err, papuga_SyntaxError, errorLine( src, si), lx.name);
				}
				// Test case of array:
				char const* si_follow = si;
				Lexeme lx_follow = parseNextLexeme( si_follow);
				if (lx_follow.type == Lexeme::Open)
				{
					lx = lx_follow;
					si = si_follow;
					node->array = true;
				}
				else if (lx_follow.type == Lexeme::Identifier)
				{
					Lexeme lx_follow2 = parseNextLexeme( si_follow);
					if (lx_follow2.type == Lexeme::Close)
					{
						lx = lx_follow;
						si = si_follow;
						node->array = true;
					}
				}
			}
			if (lx.type == Lexeme::Identifier)
			{
				node->attribute = attribute;
				node->typnam = papuga_Allocator_copy_string( &tree.allocator, lx.name, lx.namelen);
				if (!stack.link( node, tree, err))
				{
					return false;
				}
				if (!stack.empty() && !expectComma( si))
				{
					return SchemaError( err, papuga_SyntaxError, errorLine( src, si), ",");
				}
			}
			else if (lx.type == Lexeme::Open)
			{
				stack.push( node);
			}
			else
			{
				return SchemaError( err, papuga_SyntaxError, errorLine( src, si), lxnam);
			}
		}
		else if (lx.type == Lexeme::Close)
		{
			if (stack.empty())
			{
				return SchemaError( err, papuga_SyntaxError, errorLine( src, si), "}");
			}
			StackElem elem = stack.ar.back();
			stack.ar.pop_back();
			if (elem.node->array)
			{
				lx = parseNextLexeme( si);
				if (lx.type != Lexeme::Close)
				{
					return SchemaError( err, papuga_SyntaxError, errorLine( src, si), "}");
				}
			}
			if (!stack.link( elem.node, tree, err))
			{
				return false;
			}
			if (!stack.empty() && !expectComma( si))
			{
				return SchemaError( err, papuga_SyntaxError, errorLine( src, si), ",");
			}
		}
		else
		{
			return SchemaError( err, papuga_SyntaxError, errorLine( src, si));
		}
	}
	return true;
}

static bool addOperation(
		std::map<std::string,SchemaOperation>& opmap,
		const std::string& key, SchemaOperation::Id id,
		uint64_t set, int select, papuga_SchemaError* err)
{
	uint64_t mask = select == 0 ? 0 : (1 << (select-1));
	auto ins = opmap.insert( std::pair<std::string,SchemaOperation>( key, SchemaOperation( id, set, mask)));
	if (!ins.second /*not insert*/)
	{
		if (ins.first->second.id != id || ins.first->second.set != set)
		{
			return SchemaError( err, papuga_DuplicateDefinition);
		}
		ins.first->second.mask |= mask;
	}
	return true;
}

static bool buildSelectExpressionMap(
		std::map<std::string,SchemaOperation>& opmap,
		std::string path,
		SchemaNode const* node,
		const SchemaTree& tree,
		int select,
		papuga_SchemaError* err)
{
	SchemaNode const* nd = node;
	for (; nd; nd=nd->next)
	{
		const char* sep = nd->attribute ? "@":"/";
		std::string key = path + sep + nd->name;
		if (nd->typnam)
		{
			if (0==std::strcmp( nd->typnam, "integer"))
			{
				if (nd->attribute)
				{
					if (!addOperation( opmap, key, SchemaOperation::AttributeInteger, 0, select, err)) return false;
				}
				else
				{
					const SchemaOperation::Id vid = nd->array
						? SchemaOperation::NameArray
						: SchemaOperation::NameAtomic;
					if (!addOperation( opmap, key, vid, 0, select, err)
					||  !addOperation( opmap, key + "()", SchemaOperation::ValueInteger, 0, select, err))
					{
						return false;
					}
				}
			}
			else if (0==std::strcmp( nd->typnam, "number") || 0==std::strcmp( nd->typnam, "float"))
			{
				if (nd->attribute)
				{
					if (!addOperation( opmap, key, SchemaOperation::AttributeFloat, 0, select, err)) return false;
				}
				else
				{
					const SchemaOperation::Id vid = nd->array
						? SchemaOperation::NameArray
						: SchemaOperation::NameAtomic;
					if (!addOperation( opmap, key, vid, 0, select, err)
					||  !addOperation( opmap, key + "()", SchemaOperation::ValueFloat, 0, select, err))
					{
						return false;
					}
				}
			}
			else if (0==std::strcmp( nd->typnam, "bool"))
			{
				if (nd->attribute)
				{
					if (!addOperation( opmap, key, SchemaOperation::AttributeBool, 0, select, err)) return false;
				}
				else
				{
					const SchemaOperation::Id vid = nd->array
						? SchemaOperation::NameArray
						: SchemaOperation::NameAtomic;
					if (!addOperation( opmap, key, vid, 0, select, err)
					||  !addOperation( opmap, key + "()", SchemaOperation::ValueBool, 0, select, err))
					{
						return false;
					}
				}
			}
			else if (0==std::strcmp( nd->typnam, "string"))
			{
				if (nd->attribute)
				{
					if (!addOperation( opmap, key, SchemaOperation::AttributeString, 0, select, err)) return false;
				}
				else
				{
					const SchemaOperation::Id vid = nd->array
						? SchemaOperation::NameArray
						: SchemaOperation::NameAtomic;
					if (!addOperation( opmap, key, vid, 0, select, err)
					||  !addOperation( opmap, key + "()", SchemaOperation::ValueString, 0, select, err))
					{
						return false;
					}
				}
			}
			else if (nd->attribute)
			{
				return SchemaError( err, papuga_AttributeNotAtomic, 0/*no line info*/, nd->name);
			}
			else
			{
				std::map<StringView,BitSet>::const_iterator itr = tree.nodemap.find( nd->typnam);
				if (itr == tree.nodemap.end())
				{
					return SchemaError( err, papuga_AddressedItemNotFound, 0/*no line info*/, nd->typnam);
				}
				else
				{					
					SchemaOperation::Id vopen,vclose;
					if (nd->array)
					{
						vopen = SchemaOperation::OpenNamedStructureArray;
						vclose = SchemaOperation::CloseNamedStructureArray;
					}
					else
					{
						vopen = SchemaOperation::OpenNamedStructure;
						vclose = SchemaOperation::CloseNamedStructure;
					}
					if (!addOperation( opmap, key, vopen, itr->second.val, select, err)) return false;

					BitSet::const_iterator bi = itr->second.begin(), be = itr->second.end();
					for (; bi != be; ++bi)
					{
						SchemaNode const* chld = tree.nodear[ *bi-1]->chld;
						if (!chld)
						{
							return SchemaError( err, papuga_StructureExpected, 0, key.c_str());
						}
						if (!buildSelectExpressionMap( opmap, path + "//" + nd->name, chld, tree, *bi, err))
						{
							return false;
						}
					}
					if (!addOperation( opmap, key + "~", vclose, 0, select, err)) return false;
				}
			}
		}
		else if (nd->chld)
		{
			SchemaOperation::Id vopen,vclose;
			if (nd->array)
			{
				vopen = SchemaOperation::OpenStructureArray;
				vclose = SchemaOperation::CloseStructureArray;
			}
			else
			{
				vopen = SchemaOperation::OpenStructure;
				vclose = SchemaOperation::CloseStructure;
			}
			if (!addOperation( opmap, key, vopen, 0, select, err)
			||  !buildSelectExpressionMap( opmap, key, nd->chld, tree, select, err)
			||  !addOperation( opmap, key + "~", vclose, 0, select, err))
			{
				return false;
			}
		}
	}
	return true;
}

static void UNUSED printNode( std::ostream& out, SchemaNode const* node, const std::string& indent = "\n")
{
	for (; node; node=node->next)
	{
		out << indent << node->name;
		if (node->typnam)
		{
			out << " = " << node->typnam;
		}
		else if (node->chld)
		{
			out << " = {";
			printNode( out, node->chld, indent + "  ");
			out << "}";
		}
		if (node->next)
		{
			out << ",";
		}
	}
}

static bool buildAutomaton(
		PathElement& path,
		papuga_Schema* schema,
		SchemaNode const* node,
		const SchemaTree& tree,
		papuga_SchemaError* err)
{
	std::map<std::string,SchemaOperation> opmap;
	if (!buildSelectExpressionMap( opmap, "", node, tree, 0/*select*/, err))
	{
		return false;
	}
	auto oi = opmap.begin(), oe = opmap.end();
	for (; oi != oe; ++oi)
	{
		schema->ops.push_back( oi->second);
		int errorpos = schema->atm.addExpression( schema->ops.size(), oi->first.c_str(), oi->first.size());
		if (errorpos)
		{
			return SchemaError( err, papuga_LogicError, 0, oi->first.c_str(), oi->first.size());
		}
	}
	return true;
}

extern "C" void papuga_destroy_schemamap( papuga_SchemaMap* map)
{
	for (; map->arsize > 0; --map->arsize)
	{
		map->ar[ map->arsize-1].~papuga_Schema();
	}
	papuga_destroy_Allocator( &map->allocator);
}

extern "C" const char* papuga_print_schema_automaton( papuga_Allocator* allocator, const char* source, const char* schema, papuga_SchemaError* err)
{
	const char* rt = 0;
	try
	{
		SchemaTree tree;
		if (!parseSchemaTree( tree, source, err))
		{
			return 0;
		}
		std::map<StringView,BitSet>::const_iterator ni = tree.nodemap.find( schema);
		if (ni == tree.nodemap.end())
		{
			SchemaError( err, papuga_AddressedItemNotFound, 0, schema);
			return 0;
		}
		BitSet::const_iterator itr = ni->second.begin(), end = ni->second.end();
		int rootidx = *itr++;
		if (itr != end)
		{
			SchemaError( err, papuga_AmbiguousReference, 0, schema);
			return 0;
		}
		SchemaNode const* rootnode = tree.nodear[ rootidx-1];

		std::map<std::string,SchemaOperation> opmap;
		if (!buildSelectExpressionMap( opmap, "", rootnode, tree, 0/*select*/, err))
		{
			return 0;
		}
		std::string res;
		auto oi = opmap.begin(), oe = opmap.end();
		for (; oi != oe; ++oi)
		{
			char buf[ 2048];
			std::snprintf( buf, sizeof(buf), "%s %s %" PRIu64 " %" PRIu64 "\n",
						oi->first.c_str(), SchemaOperation::idName( oi->second.id),
						oi->second.set, oi->second.mask);
			buf[ sizeof( buf)-1] = 0;
			res.append( buf);
		}
		rt = papuga_Allocator_copy_string( allocator, res.c_str(), res.size());
		if (!rt)
		{
			SchemaError( err, papuga_NoMemError);
			return 0;
		}
	}
	catch (const std::bad_alloc&)
	{
		SchemaError( err, papuga_NoMemError);
		return 0;
	}
	return rt;
}

extern "C" papuga_SchemaMap* papuga_create_schemamap( const char* source, papuga_SchemaError* err)
{
	papuga_Allocator allocator;
	papuga_init_Allocator( &allocator, 0, 0);
	papuga_SchemaMap* rt = (papuga_SchemaMap*) papuga_Allocator_alloc( &allocator, sizeof( papuga_SchemaMap), 0);
	if (!rt)
	{
		SchemaError( err, papuga_NoMemError);
		return 0;
	}
	std::memcpy( &rt->allocator, &allocator, sizeof(allocator));
	rt->arsize = 0;
	rt->ar = 0;
	try
	{
		SchemaTree tree;
		if (!parseSchemaTree( tree, source, err))
		{
			return 0;
		}
		if (tree.nodemap.empty())
		{
			return rt;
		}
		std::size_t arsize = tree.nodemap.size();
		rt->ar = (papuga_Schema*) papuga_Allocator_alloc( &rt->allocator, arsize*sizeof( papuga_Schema), 0);
		if (!rt->ar)
		{
			papuga_destroy_Allocator( &rt->allocator);
			SchemaError( err, papuga_NoMemError);
			return 0;
		}
		std::map<StringView,BitSet>::const_iterator ni = tree.nodemap.begin(), ne = tree.nodemap.end();
		for (; ni != ne; ++ni)
		{
			BitSet::const_iterator itr = ni->second.begin(), end = ni->second.end();
			int rootidx = *itr++;
			if (itr != end)
			{
				continue;
			}
			char const* schemaName = papuga_Allocator_copy_charp( &rt->allocator, ni->first.str);
			if (!schemaName)
			{
				papuga_destroy_schemamap( rt);
				SchemaError( err, papuga_NoMemError);
				return 0;
			}
			papuga_Schema* schema = new (rt->ar + rt->arsize) papuga_Schema( schemaName);
			SchemaNode const* rootnode = tree.nodear[ rootidx-1];
			PathElement rootpath = (*schema->atm)[ rootnode->name];
			if (!buildAutomaton( rootpath, schema, rootnode, tree, err))
			{
				papuga_destroy_schemamap( rt);
				return 0;
			}
			rt->arsize += 1;
		}
	}
	catch (const std::bad_alloc&)
	{
		SchemaError( err, papuga_NoMemError);
		papuga_destroy_schemamap( rt);
		return 0;
	}
	return rt;
}

extern "C" papuga_Schema const* papuga_schemamap_get( const papuga_SchemaMap* map, const char* schemaname)
{
	int si = 0, se = map->arsize;
	for (; si != se; ++si)
	{
		if (0==std::strcmp( schemaname, map->ar[ si].name))
		{
			return map->ar+si;
		}
	}
	return 0;
}

extern "C" void papuga_destroy_schemalist( papuga_SchemaList* list)
{
	papuga_destroy_Allocator( &list->allocator);
}

extern "C" papuga_SchemaList* papuga_create_schemalist( const char* source, papuga_SchemaError* err)
{
	papuga_Allocator allocator;
	papuga_init_Allocator( &allocator, 0, 0);
	papuga_SchemaList* rt = (papuga_SchemaList*) papuga_Allocator_alloc( &allocator, sizeof( papuga_SchemaList), 0);
	if (!rt)
	{
		SchemaError( err, papuga_NoMemError);
		return 0;
	}
	std::memcpy( &rt->allocator, &allocator, sizeof(allocator));
	rt->arsize = 0;
	rt->ar = 0;
	try
	{
		int count = 0;
		char const* si = source;
		char const* si_next = source;
		Lexeme lx = parseNextLexeme( si_next);
		for (; lx.type == Lexeme::Identifier; count++, si_next = si, lx = parseNextLexeme( si_next))
		{
			papuga_SchemaSource res;
			if (!parseSchemaSource( res, rt->allocator, source, si, err))
			{
				papuga_destroy_Allocator( &rt->allocator);
				return 0;
			}
		}
		if (count)
		{
			si = source;
			rt->ar = (papuga_SchemaSource*) papuga_Allocator_alloc( &rt->allocator, count*sizeof( papuga_SchemaSource), 0);
			if (!rt->ar)
			{
				papuga_destroy_Allocator( &rt->allocator);
				SchemaError( err, papuga_NoMemError);
				return 0;
			}
			rt->arsize = count;
			for (int ci=0; ci<count; ++ci)
			{
				if (!parseSchemaSource( rt->ar[ ci], rt->allocator, source, si, err))
				{
					papuga_destroy_Allocator( &rt->allocator);
					return 0;
				}
			}
		}
	}
	catch (const std::bad_alloc&)
	{
		SchemaError( err, papuga_NoMemError);
		papuga_destroy_Allocator( &rt->allocator);
		return 0;
	}
	return rt;
}

extern "C" papuga_SchemaSource const* papuga_schemalist_get( const papuga_SchemaList* list, const char* schemaname)
{
	papuga_SchemaSource const* si = list->ar;
	papuga_SchemaSource const* se = list->ar + list->arsize;
	for (; si != se; ++si)
	{
		if (si->name[0] == schemaname[0] && 0==std::strcmp(si->name, schemaname))
		{
			return si;
		}
	}
	return 0;
}

struct RequestElement
{
	textwolf::XMLScannerBase::ElementType type;
	const char* valuestr;
	std::size_t valuelen;

	RequestElement( textwolf::XMLScannerBase::ElementType type_, const char* valuestr_, std::size_t valuelen_)
		:type(type_),valuestr(valuestr_),valuelen(valuelen_){}
	RequestElement( const RequestElement& o)
		:type(o.type),valuestr(o.valuestr),valuelen(o.valuelen){}

	bool operator==( const RequestElement& o) const
	{
		if (type != o.type) return false;
		if (valuelen != o.valuelen) return false;
		return (0==std::strcmp( valuestr, o.valuestr));
	}
};

static inline textwolf::XMLScannerBase::ElementType requestElementType( papuga_RequestElementType elemtype)
{
	static const textwolf::XMLScannerBase::ElementType ar[] = {
		textwolf::XMLScannerBase::Exit,
		textwolf::XMLScannerBase::OpenTag,
		textwolf::XMLScannerBase::CloseTag,
		textwolf::XMLScannerBase::TagAttribName,
		textwolf::XMLScannerBase::TagAttribValue,
		textwolf::XMLScannerBase::Content
	};
	return ar[ elemtype];
}

static bool parseRequest( std::vector<RequestElement>& res, papuga_Allocator* allocator, papuga_ContentType doctype, papuga_StringEncoding encoding, const char* contentstr, size_t contentlen, papuga_SchemaError* err)
{
	papuga_RequestParser* parser = 0;
	papuga_ErrorCode errcode = papuga_Ok;
	const char* valuestr;
	std::size_t valuelen;
	
	switch (doctype)
	{
		case papuga_ContentType_Unknown:
			errcode = papuga_InvalidRequest;
		case papuga_ContentType_XML:
			parser = papuga_create_RequestParser_xml( allocator, encoding, contentstr, contentlen, &errcode);
			break;
		case papuga_ContentType_JSON:
			parser = papuga_create_RequestParser_json( allocator, encoding, contentstr, contentlen, &errcode);
			break;
		default:
			errcode = papuga_InvalidRequest;
			break;
	}
	if (!parser)
	{
		return SchemaError( err, errcode);
	}
	papuga_ValueVariant elemvalue;
	papuga_RequestElementType elemtype = papuga_RequestParser_next( parser, &elemvalue);
	for (; elemtype != papuga_RequestElementType_None; elemtype = papuga_RequestParser_next( parser, &elemvalue))
	{
		if (papuga_ValueVariant_defined( &elemvalue))
		{
			valuestr = papuga_ValueVariant_tostring( &elemvalue, allocator, &valuelen, &errcode);
			if (!valuestr)
			{
				return SchemaError( err, errcode);
			}
			if (valuelen > 0)
			{
				valuestr = papuga_Allocator_copy_string( allocator, valuestr, valuelen);
				if (!valuestr)
				{
					return SchemaError( err, papuga_NoMemError);
				}
			}
			else
			{
				valuestr = "";
			}
		}
		else
		{
			valuestr = "";
			valuelen = 0;
		}		
		try
		{
			res.push_back( RequestElement( requestElementType( elemtype), valuestr, valuelen));
		}
		catch(...)
		{
			return SchemaError( err, papuga_NoMemError);
		}
	}
	return true;
}

static bool RequestProcessError( papuga_SchemaError* err, papuga_ErrorCode errcode, const std::vector<RequestElement>& request, std::size_t idx)
{
	char errlocation[ 1024];
	auto ri = request.begin(), re = (idx >= request.size()) ? request.end() : (request.begin() + idx);
	std::vector<RequestElement> pt;
	for (; ri != re; ++ri)
	{
		switch (ri->type)
		{
			case textwolf::XMLScannerBase::Exit: break;
			case textwolf::XMLScannerBase::OpenTag:
				pt.push_back( *ri);
				break;
			case textwolf::XMLScannerBase::CloseTag:
				while (!pt.empty() && pt.back().type != textwolf::XMLScannerBase::OpenTag) pt.pop_back();
				pt.pop_back();
				break;
			case textwolf::XMLScannerBase::TagAttribName:
				pt.push_back( *ri);
				break;
			case textwolf::XMLScannerBase::TagAttribValue:
				pt.push_back( *ri);
				break;
			case textwolf::XMLScannerBase::Content:
				break;
			default:
				break;
		}
	}
	std::string res;
	auto pi = pt.begin(), pe = pt.end();
	for (; pi != pe; ++pi)
	{
		switch (pi->type)
		{
			case textwolf::XMLScannerBase::Exit: break;
			case textwolf::XMLScannerBase::OpenTag:
				res.append( "/");
				res.append( pi->valuestr, pi->valuelen);
				break;
			case textwolf::XMLScannerBase::CloseTag:
				break;
			case textwolf::XMLScannerBase::TagAttribName:
				res.append( " ");
				res.append( pi->valuestr, pi->valuelen);
				break;
			case textwolf::XMLScannerBase::TagAttribValue:
				res.append( "=");
				res.append( pi->valuestr, pi->valuelen);
				break;
			case textwolf::XMLScannerBase::Content:
				break;
			default:
				break;
		}
	}
	std::snprintf( errlocation, sizeof(errlocation), "%s", res.c_str());
	errlocation[ sizeof(errlocation)-1] = 0;
	return SchemaError( err, errcode, 0, errlocation);
}

static inline void pushName( papuga_Serialization* output, char const* namestr, size_t namelen)
{
	namestr = papuga_Allocator_copy_string( output->allocator, namestr, namelen);
	if (!namestr) throw std::bad_alloc();
	if (!papuga_Serialization_pushName_string( output, namestr, namelen))  throw std::bad_alloc();
}

static inline void pushValue( papuga_Serialization* output, char const* namestr, size_t namelen)
{
	namestr = papuga_Allocator_copy_string( output->allocator, namestr, namelen);
	if (!namestr) throw std::bad_alloc();
	if (!papuga_Serialization_pushValue_string( output, namestr, namelen))  throw std::bad_alloc();
}

static inline void pushOpen( papuga_Serialization* output)
{
	if (!papuga_Serialization_pushOpen( output))  throw std::bad_alloc();
}

static inline void pushClose( papuga_Serialization* output)
{
	if (!papuga_Serialization_pushClose( output))  throw std::bad_alloc();
}

static bool serializeRequest( papuga_Serialization* output, papuga_Schema const* schema, const std::vector<RequestElement>& request, papuga_SchemaError* err)
{
	AutomatonState atmstate( &schema->atm);
	int64_t val_int;
	double val_double;
	bool val_bool;
	const char* arraytag = 0;
	std::vector<int64_t> setStack;	//... Stack to determine ambiguous structure definitions
	std::vector<size_t> arrayStack;
	
	size_t ridx = 0;
	size_t rsize = request.size();
	for (; ridx != rsize; ++ridx)
	{
		const RequestElement& relem = request[ ridx];
		AutomatonState::iterator itr = atmstate.push( relem.type, relem.valuestr, relem.valuelen);
		int nofEvents = 0;
		for (*itr; *itr; ++itr,++nofEvents)
		{
			const SchemaOperation& op = schema->ops[ *itr-1];
			if (op.mask)
			{
				if (setStack.empty())
				{
					return SchemaError( err, papuga_LogicError);
				}
				if ((setStack.back() & op.mask) == 0)
				{
					return RequestProcessError( err, papuga_SyntaxError, request, ridx);
				}
			}
			if (arraytag)
			{
				if (op.id != SchemaOperation::ValueInteger
				&&  op.id != SchemaOperation::ValueFloat
				&&  op.id != SchemaOperation::ValueBool
				&&  op.id != SchemaOperation::ValueString)
				{}
				else if (op.id == SchemaOperation::NameArray)
				{
					if (0!=std::memcmp( arraytag, relem.valuestr, relem.valuelen))
					{
						arraytag = 0;
						pushClose( output);
					}

				}
				else
				{
					arraytag = 0;
					pushClose( output);
				}
			}
			switch (op.id)
			{
				case SchemaOperation::NameAtomic:
					pushName( output, relem.valuestr, relem.valuelen);
					break;
				case SchemaOperation::NameArray:
					if (!arraytag)
					{
						pushName( output, relem.valuestr, relem.valuelen);
						pushOpen( output);
						arraytag = relem.valuestr;
					}
					break;
				case SchemaOperation::AttributeInteger:
					if (ridx == 0) return SchemaError( err, papuga_LogicError);
					pushName( output, request[ ridx-1].valuestr, request[ ridx-1].valuelen);
					/* no break here! */
				case SchemaOperation::ValueInteger:
					if (!stringToInt( val_int, relem.valuestr))
					{
						return RequestProcessError( err, papuga_SyntaxError, request, ridx);
					}
					if (!papuga_Serialization_pushValue_int( output, val_int))
					{
						return SchemaError( err, papuga_NoMemError);
					}
					break;
				case SchemaOperation::AttributeFloat:
					if (ridx == 0) return SchemaError( err, papuga_LogicError);
					pushName( output, request[ ridx-1].valuestr, request[ ridx-1].valuelen);
					/* no break here! */
				case SchemaOperation::ValueFloat:
					if (!stringToDouble( val_double, relem.valuestr))
					{
						return RequestProcessError( err, papuga_SyntaxError, request, ridx);
					}
					if (!papuga_Serialization_pushValue_double( output, val_double))
					{
						return SchemaError( err, papuga_NoMemError);
					}
					break;
				case SchemaOperation::AttributeBool:
					if (ridx == 0) return SchemaError( err, papuga_LogicError);
					pushName( output, request[ ridx-1].valuestr, request[ ridx-1].valuelen);
					/* no break here! */
				case SchemaOperation::ValueBool:
					if (!stringToBool( val_bool, relem.valuestr))
					{
						return RequestProcessError( err, papuga_SyntaxError, request, ridx);
					}
					if (!papuga_Serialization_pushValue_double( output, val_bool))
					{
						return SchemaError( err, papuga_NoMemError);
					}
					break;
				case SchemaOperation::AttributeString:
					if (ridx == 0) return SchemaError( err, papuga_LogicError);
					pushName( output, request[ ridx-1].valuestr, request[ ridx-1].valuelen);
					/* no break here! */
				case SchemaOperation::ValueString:
					pushValue( output, relem.valuestr, relem.valuelen);
					break;

				case SchemaOperation::OpenNamedStructure:
					setStack.push_back( op.set);

					pushName( output, relem.valuestr, relem.valuelen);
					pushOpen( output);
					break;

				case SchemaOperation::CloseNamedStructure:
					if (setStack.empty()) return SchemaError( err, papuga_LogicError);
					setStack.pop_back();

					pushClose( output);
					break;

				case SchemaOperation::OpenNamedStructureArray:
					setStack.push_back( op.set);
					arrayStack.push_back( ridx);

					pushName( output, relem.valuestr, relem.valuelen);
					pushOpen( output);
					pushOpen( output);
					break;

				case SchemaOperation::CloseNamedStructureArray:
					if (setStack.empty() || arrayStack.empty())
					{
						return SchemaError( err, papuga_LogicError);
					}
					setStack.pop_back();

					if (ridx+1 < request.size() && request[ arrayStack.back()] == request[ ridx])
					{
						pushClose( output);
						pushOpen( output);
						++ridx;
					}
					else
					{
						pushClose( output);
						pushClose( output);
						arrayStack.pop_back();
					}
					break;

				case SchemaOperation::OpenStructure:
					pushName( output, relem.valuestr, relem.valuelen);
					pushOpen( output);
					break;

				case SchemaOperation::CloseStructure:
					pushClose( output);
					break;

				case SchemaOperation::OpenStructureArray:
					arrayStack.push_back( ridx);

					pushName( output, relem.valuestr, relem.valuelen);
					pushOpen( output);
					pushOpen( output);
					break;

				case SchemaOperation::CloseStructureArray:
					if (arrayStack.empty())
					{
						return SchemaError( err, papuga_LogicError);
					}

					if (ridx+1 < request.size() && request[ arrayStack.back()] == request[ ridx])
					{
						pushClose( output);
						pushOpen( output);
						++ridx;
					}
					else
					{
						pushClose( output);
						pushClose( output);
						arrayStack.pop_back();
					}
					break;
			}
		}
		if (nofEvents == 0)
		{
			if (relem.type != textwolf::XMLScannerBase::CloseTag && relem.type != textwolf::XMLScannerBase::TagAttribName)
			{
				if (relem.type == textwolf::XMLScannerBase::Content)
				{
					char const* ci = relem.valuestr;
					char const* ce = relem.valuestr + relem.valuelen;
					for (; ci != ce; ++ci)
					{
						if ((unsigned char)*ci > 32) break;
					}
					if (ci != ce)
					{
						return RequestProcessError( err, papuga_SyntaxError, request, ridx);
					}
				}
				else
				{
					return RequestProcessError( err, papuga_SyntaxError, request, ridx);
				}
			}
		}
		else if (nofEvents > 1)
		{
			return RequestProcessError( err, papuga_SyntaxError, request, ridx);
		}
	}
	return true;
}

extern "C" bool papuga_schema_parse( papuga_Serialization* dest, papuga_Schema const* schema, papuga_ContentType doctype, papuga_StringEncoding encoding, const char* contentstr, size_t contentlen, papuga_SchemaError* err)
{
	papuga_Allocator allocator;
	int allocatormem[ 2048];
	papuga_init_Allocator( &allocator, allocatormem, sizeof(allocatormem));

	try
	{
		std::vector<RequestElement> request;
		if (!parseRequest( request, &allocator, doctype, encoding, contentstr, contentlen, err)
		||  !serializeRequest( dest, schema, request, err))
		{
			papuga_destroy_Allocator( &allocator);
			return false;
		}
	}
	catch (const std::bad_alloc&)
	{
		papuga_destroy_Allocator( &allocator);
		return SchemaError( err, papuga_NoMemError);
	}
	return true;
}



