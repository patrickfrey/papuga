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
#include "textwolf.hpp"
#include <string>
#include <map>
#include <bitset>
#include <cstdint>
#include <cstdlib>
#include <cstring>

typedef textwolf::XMLPathSelectAutomaton<textwolf::charset::UTF8> Automaton;
typedef textwolf::XMLPathSelectAutomaton<>::PathElement PathElement;

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
		ValueInteger,
		ValueFloat,
		ValueString,
		ArrayInteger,
		ArrayFloat,
		ArrayString,
		OpenSubStructure,
		OpenStructure,
		OpenSubStructureArray,
		OpenStructureArray,
		CloseStructure
	};
	SchemaOperation( Id id_, uint64_t set_, int select_) :id(id_),set(set_),select(select_){}

	Id id;
	uint64_t set;
	int select;
};

bool SchemaError( papuga_SchemaError* err, papuga_ErrorCode errcode, int line, const char* itm=0)
{
	err->code = errcode;
	err->line = line;
	if (itm)
	{
		if ((int)sizeof(err->item) <= std::snprintf( err->item, sizeof(err->item), "%s", itm))
		{
			err->item[ sizeof(err->item)-1] = 0;
		}
	}
	return false;
}

struct papuga_Schema
{
	char const* name;
	Automaton atm;
	std::vector<SchemaOperation> ops;

	papuga_Schema( char const* name_) :name(name_),atm(),ops(){}
};

struct papuga_SchemaMap
{
	papuga_Schema* ar;
	int arsize;
	papuga_Allocator allocator;
};

struct SchemaNode
{
	const char* typnam;
	const char* name;
	SchemaNode* next;
	SchemaNode* chld;
	bool array;

	SchemaNode( const char* name_)
		:typnam(0),name(name_),next(0),chld(0),array(false){}
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
	enum Type {EndOfSource,Error,Open,Close,Equal,Identifier,Comma};
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
	Lexeme lx = parseNextLexeme( src);
	char const* start = si;
	if (lx.type == Lexeme::Error)
	{
		return SchemaError( err, papuga_SyntaxError, errorLine( src, si), 0);
	}
	if (lx.type != Lexeme::Identifier)
	{
		return SchemaError( err, papuga_SyntaxError, errorLine( src, si), 0);
	}
	res.name = papuga_Allocator_copy_string( &allocator, lx.name, lx.namelen);
	if (!res.name)
	{
		return SchemaError( err, papuga_NoMemError, errorLine( src, si), 0);
	}
	if (lx.type != Lexeme::Equal)
	{
		return SchemaError( err, papuga_SyntaxError, errorLine( src, si), 0);
	}
	if (!skipToEndBlock( si))
	{
		return SchemaError( err, papuga_SyntaxError, errorLine( src, si), 0);
	}
	for (; *si && (unsigned char)*si <= 32; ++si){}
	res.source = papuga_Allocator_copy_string( &allocator, start, si-start+1);
	if (!res.source)
	{
		return SchemaError( err, papuga_NoMemError, errorLine( src, si), 0);
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
			ar.push_back( StackElem( node,node));
		}
		bool link( SchemaNode* node, SchemaTree& tree)
		{
			if (ar.empty())
			{
				if (tree.nodearsize >= MaxNofNodes) return false;
				tree.nodear[ tree.nodearsize++] = node;
				tree.nodemap[ StringView(node->name)].set( tree.nodearsize);
			}
			else
			{
				ar.back().tail->next = node;
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
			return SchemaError( err, papuga_SyntaxError, errorLine( src, si), 0);
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
				return SchemaError( err, papuga_SyntaxError, errorLine( src, si), 0);
			}
			lx = parseNextLexeme( si);

			if (lx.type == Lexeme::Open)
			{
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
				node->typnam = papuga_Allocator_copy_string( &tree.allocator, lx.name, lx.namelen);
				if (!stack.link( node, tree))
				{
					return SchemaError( err, papuga_ComplexityOfProblem, errorLine( src, si));
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
			if (!stack.link( elem.node, tree))
			{
				return SchemaError( err, papuga_ComplexityOfProblem, errorLine( src, si));
			}
			if (!stack.empty() && !expectComma( si))
			{
				return SchemaError( err, papuga_SyntaxError, errorLine( src, si), ",");
			}
		}
		else
		{
			return SchemaError( err, papuga_SyntaxError, errorLine( src, si), 0/*itm*/);
		}
	}
	return true;
}

static bool buildAutomaton( PathElement& path, papuga_Schema* schema, papuga_SchemaError* err, SchemaNode const* node, const SchemaTree& tree, int select)
{
	SchemaNode const* chld = node->chld;
	for (; chld; chld=node->next)
	{
		PathElement subpath = path;
		subpath[ chld->name];

		if (chld->typnam)
		{
			if (0==std::strcmp( chld->typnam, "integer"))
			{
				schema->ops.push_back( SchemaOperation( SchemaOperation::ValueInteger, 0, select));
				subpath.assignType( schema->ops.size());
			}
			else if (0==std::strcmp( chld->typnam, "float"))
			{
				schema->ops.push_back( SchemaOperation( SchemaOperation::ValueFloat, 0, select));
				subpath.assignType( schema->ops.size());
			}
			else if (0==std::strcmp( chld->typnam, "string"))
			{
				schema->ops.push_back( SchemaOperation( SchemaOperation::ValueString, 0, select));
				subpath.assignType( schema->ops.size());
			}
			else
			{
				std::map<StringView,BitSet>::const_iterator itr = tree.nodemap.find( chld->typnam);
				if (itr == tree.nodemap.end())
				{
					return SchemaError( err, papuga_AddressedItemNotFound, 0/*no line info*/, chld->typnam);
				}
				else
				{
					schema->ops.push_back( SchemaOperation( SchemaOperation::OpenSubStructure, itr->second.val, select));
					subpath.assignType( schema->ops.size());

					BitSet::const_iterator bi = itr->second.begin(), be = itr->second.end();
					for (; bi != be; ++bi)
					{
						PathElement seekpath = path;
						seekpath--[ chld->name]; //... seekpath :=  <path> // <name>
						if (!buildAutomaton( seekpath, schema, err, chld, tree, *bi/*select*/))
						{
							return false;
						}
					}
					subpath.selectCloseTag();
					schema->ops.push_back( SchemaOperation( SchemaOperation::CloseStructure, 0, 0/*select*/));
					subpath.assignType( schema->ops.size());
				}
			}
		}
		else if (chld->chld)
		{
			schema->ops.push_back( SchemaOperation( SchemaOperation::OpenStructure, 0, 0/*select*/));
			subpath.assignType( schema->ops.size());
			if (!buildAutomaton( subpath, schema, err, chld, tree, 0/*select*/))
			{
				return false;
			}
			subpath.selectCloseTag();
			schema->ops.push_back( SchemaOperation( SchemaOperation::CloseStructure, 0, 0/*select*/));
			subpath.assignType( schema->ops.size());
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

extern "C" papuga_SchemaMap* papuga_create_schemamap( const char* source, papuga_SchemaError* err)
{
	papuga_Allocator allocator;
	papuga_SchemaMap* rt = (papuga_SchemaMap*) papuga_Allocator_alloc( &allocator, sizeof( papuga_SchemaMap), 0);
	if (!rt)
	{
		SchemaError( err, papuga_NoMemError, 0/*line*/, 0/*item*/);
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
		rt->ar = (papuga_Schema*) papuga_Allocator_alloc( &rt->allocator, tree.nodemap.size()*sizeof( papuga_Schema), 0);
		if (!rt->ar)
		{
			papuga_destroy_Allocator( &rt->allocator);
			SchemaError( err, papuga_NoMemError, 0/*line*/, 0/*item*/);
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
				SchemaError( err, papuga_NoMemError, 0/*line*/, 0/*item*/);
				return 0;
			}
			papuga_Schema* schema = new (rt->ar + rt->arsize) papuga_Schema( schemaName);
			SchemaNode const* rootnode = tree.nodear[ rootidx-1];
			PathElement rootpath = (*schema->atm)[ rootnode->name];
			if (!buildAutomaton( rootpath, schema, err, rootnode, tree, 0))
			{
				papuga_destroy_schemamap( rt);
				return 0;
			}
			rt->arsize += 1;
		}
	}
	catch (const std::bad_alloc&)
	{
		SchemaError( err, papuga_NoMemError, 0/*line*/, 0/*item*/);
		papuga_destroy_Allocator( &rt->allocator);
		return 0;
	}
	return rt;
}

extern "C" papuga_Schema const* papuga_schema_get( const papuga_SchemaMap* map, const char* schemaname)
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

extern "C" bool papuga_schema_parse( papuga_Serialization* dest, papuga_Serialization const* src, papuga_Schema const* schema, papuga_SchemaError* err)
{
	bool rt = false;
	try
	{
		papuga_SerializationIter iter;
		papuga_init_SerializationIter( &iter, src);

		rt = true;
	}
	catch (const std::bad_alloc&)
	{
		return SchemaError( err, papuga_NoMemError, 0/*line*/, 0/*item*/);
	}
	return rt;
}

extern "C" void papuga_destroy_schemalist( papuga_SchemaList* list)
{
	papuga_destroy_Allocator( &list->allocator);
}

extern "C" papuga_SchemaList* papuga_parse_schemalist( const char* source, papuga_SchemaError* err)
{
	papuga_Allocator allocator;
	papuga_SchemaList* rt = (papuga_SchemaList*) papuga_Allocator_alloc( &allocator, sizeof( papuga_SchemaList), 0);
	if (!rt)
	{
		SchemaError( err, papuga_NoMemError, 0/*line*/, 0/*item*/);
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
				SchemaError( err, papuga_NoMemError, 0/*line*/, 0/*item*/);
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
		SchemaError( err, papuga_NoMemError, 0/*line*/, 0/*item*/);
		papuga_destroy_Allocator( &rt->allocator);
		return 0;
	}
	return rt;
}


