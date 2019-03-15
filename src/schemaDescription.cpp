/*
 * Copyright (c) 2019 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/*
* \brief Automaton to describe and build papuga XML and JSON requests
* \file schemaDescription.cpp
*/
#include "papuga/schemaDescription.h"
#include "papuga/allocator.h"
#include <vector>
#include <utility>
#include <new>
#include <stdexcept>
#include <cstring>
#include <cstdlib>
#include <iostream>
#include <sstream>

class SyntaxErrorException
	:public std::runtime_error
{
public:
	SyntaxErrorException() :std::runtime_error("syntax error"){}
};

class LogicErrorException
	:public std::runtime_error
{
public:
	LogicErrorException() :std::runtime_error("logic error"){}
};


static bool isDelimiter( char ch)
{
	static const char* brk = "/@[](){}#=";
	if (ch == '\0') return true;
	if ((unsigned char)ch < 32) return true;
	return 0!=std::strchr(brk,ch);
}

static char const* skipBrackets( char const* src)
{
	char sb = *src++;
	char eb = '\0';
	if (sb == '(') eb = ')';
	else if (sb == '[') eb = ']';
	else if (sb == '{') eb = '}';
	else throw SyntaxErrorException();

	for (; *src && *src != eb; ++src)
	{
		if (*src == sb) throw SyntaxErrorException();
	}
	if (*src != eb) throw SyntaxErrorException();
	return ++src;
}

std::string parseToken( char const*& src)
{
	const char* start = src;
	for (;*src && !isDelimiter(*src); ++src){};
	return std::string( start, src-start);
}

std::string parseString( char const*& src)
{
	std::string rt;
	char eb = *src++;
	for (; *src && *src != eb; ++src)
	{
		if (*src == '\\')
		{
			++src;
			switch (*src)
			{
				case 'n': rt.push_back( '\n'); break;
				case 't': rt.push_back( '\t'); break;
				case 'b': rt.push_back( '\b'); break;
				case 'r': rt.push_back( '\r'); break;
				case '\0': throw SyntaxErrorException();
				default: rt.push_back( *src); break;
			}
		}
		else
		{
			rt.push_back( *src);
		}
	}
	if (*src != eb) throw SyntaxErrorException();
	return rt;
}

static char const* skipElement( char const* src)
{
	for (; !isDelimiter( *src); ++src){}
	return src;
}


class TreeNode
{
public:
	std::string name;
	int id;
	bool isAttribute;
	papuga_Type valueType;
	papuga_ResolveType resolveType;
	std::vector<std::string> examples;
	std::vector<TreeNode> chld;

	TreeNode()
		:name(),id(-1),isAttribute(false),valueType(papuga_TypeVoid),resolveType(papuga_ResolveTypeRequired),examples(),chld(){}
	TreeNode( const TreeNode& o)
		:name(o.name),id(o.id),isAttribute(o.isAttribute),valueType(o.valueType),resolveType(o.resolveType),examples(o.examples),chld(o.chld){}
	TreeNode& operator = (const TreeNode& o)
		{name=o.name; id=o.id; isAttribute=o.isAttribute; valueType=o.valueType; resolveType=o.resolveType; examples=o.examples; chld=o.chld; return *this;}
#if __cplusplus >= 201103L
	TreeNode( TreeNode&& o)
		:name(std::move(o.name)),id(o.id),isAttribute(o.isAttribute),valueType(o.valueType),resolveType(o.resolveType),examples(std::move(o.examples)),chld(std::move(o.chld)){}
	TreeNode& operator = (TreeNode&& o)
		{name=std::move(o.name); id=o.id; isAttribute=o.isAttribute; valueType=o.valueType; resolveType=o.resolveType; examples=std::move(o.examples); chld=std::move(o.chld); return *this;}
#endif
	TreeNode( const std::string& name_, int id_, bool isAttribute_, papuga_Type valueType_, const std::vector<std::string>& examples_=std::vector<std::string>())
		:name(name_),id(id_),isAttribute(isAttribute_),valueType(valueType_),resolveType(papuga_ResolveTypeRequired),examples(examples_),chld()
	{}
	TreeNode( const std::string& name_, int id_, bool isAttribute_, papuga_Type valueType_, const char* examples_)
		:name(name_),id(id_),isAttribute(isAttribute_),valueType(valueType_),resolveType(papuga_ResolveTypeRequired),examples(),chld()
	{
		addExamples( examples_);
	}
	void addExamples( const char* examples_)
	{
		if (examples_)
		{
			std::string examplebuf;
			if (examples_)
			{
				examplebuf.append( examples_);
				char* ei = const_cast<char*>( examplebuf.c_str());
				char* ni = std::strchr( ei, ';');
				for (; ni; ei=ni+1,ni = std::strchr( ei, ';'))
				{
					*ni = '\0';
					examples.push_back( std::string(ei,ni-ei));
				}
				examples.push_back( std::string(ei));
			}
		}
	}

	void addChild( const TreeNode& o)
	{
		if (isAttribute) throw SyntaxErrorException();
		chld.push_back( o);
	}
	std::vector<TreeNode>::iterator findNode( const std::string& name_)
	{
		std::vector<TreeNode>::iterator ci = chld.begin(), ce = chld.end();
		for (; ci != ce; ++ci)
		{
			if (name_ == ci->name) return ci;
		}
		return chld.end();
	}

	void addConditionElements( const char* src)
	{
		while (*src == '[')
		{
			++src;
			for (;;)
			{
				for (;*src && (unsigned char)*src <= 32; ++src){}
				bool condIsAttribute = false;
				if (*src == '@') {++src; condIsAttribute = true;}
				char const* start = src;
				src = skipElement( src);
				if (start == src) throw SyntaxErrorException();
				std::string condAttrName( start, src-start);
				std::vector<std::string> condExamples;
				for (;*src && (unsigned char)*src <= 32; ++src){}
				if (*src == '=')
				{
					++src;
					for (;*src && (unsigned char)*src <= 32; ++src){}
					if (*src == '\'' || *src == '\"')
					{
						condExamples.push_back( parseString( src));
					}
					else
					{
						condExamples.push_back( parseToken( src));
					}
					for (;*src && (unsigned char)*src <= 32; ++src){}
				}
				std::vector<TreeNode>::iterator ni = findNode( condAttrName);
				if (ni == chld.end())
				{
					chld.push_back( TreeNode( condAttrName, -1/*id*/, condIsAttribute, papuga_TypeVoid, condExamples));
				}
				else
				{
					if (ni->isAttribute != condIsAttribute) throw SyntaxErrorException();
					ni->examples.insert( ni->examples.end(), condExamples.begin(), condExamples.end());
				}
				if (*src == ',')
				{
					++src;
				}
				else
				{
					break;
				}
			}
			if (*src == ']')
			{
				++src;
				for (;*src && (unsigned char)*src <= 32; ++src){}
			}
			else
			{
				throw std::runtime_error( "syntax error");
			}
		}
	}

	bool addFollow( int id_, const char* expression_, papuga_Type valueType_, const char* examples_)
	{
		char const* ei = expression_;
		if (*ei == '[')
		{
			addConditionElements( ei);
			ei = skipBrackets( ei);
		}
		if (*ei == '/')
		{
			if (isAttribute) throw SyntaxErrorException();
			++ei;
			if (*ei == '/')
			{
				return false;
			}
			return addElement( id_, ei, valueType_, examples_);
		}
		else if (*ei == '@')
		{
			if (isAttribute) throw SyntaxErrorException();
			return addElement( id_, ei, valueType_, examples_);
		}
		else if (*ei == '(' || *ei == '\0')
		{
			valueType = valueType_;
			id = id_;
			addExamples( examples_);
			return true;
		}
		else
		{
			throw SyntaxErrorException();
		}
	}

	static std::string trim( char const* str, std::size_t strsize)
	{
		for (; strsize && (unsigned char)*str <= 32; ++str,--strsize){}
		for (; strsize && (unsigned char)str[ strsize-1] <= 32; --strsize){}
		return std::string( str, strsize);
	}

	static std::vector<std::string> getAltNodeList( const std::string& nodeliststr)
	{
		std::vector<std::string> rt;
		char const* ei = nodeliststr.c_str();
		char const* ni = std::strchr( ei, ',');
		for (; ni; ei=ni+1,ni = std::strchr( ei, ','))
		{
			rt.push_back( trim( ei, ni-ei));
			if (rt.back().empty() || rt.back()[0] == '@')
			{
				throw SyntaxErrorException();
			}
		}
		ni = std::strchr( ei, '\0');
		rt.push_back( trim( ei, ni-ei));
		return rt;
	}

	std::vector<TreeNode>::iterator getOrCreateChildNode( const std::string& nodename, bool nodeIsAttribute)
	{
		std::vector<TreeNode>::iterator ni = findNode( nodename);
		if (ni == chld.end())
		{
			chld.push_back( TreeNode( nodename, -1/*id*/, nodeIsAttribute, papuga_TypeVoid));
			ni = chld.end();
			--ni;
		}
		else
		{
			if (ni->isAttribute != nodeIsAttribute) throw SyntaxErrorException();
		}
		return ni;
	}

	bool addElement( int id_, const char* expression_, papuga_Type valueType_, const char* examples_)
	{
		char const* ei = expression_;
		bool nodeIsAttribute = false;
		if (*ei == '{')
		{
			bool rt = true;
			char const* start = ei;
			ei = skipBrackets( ei);
			std::vector<std::string> nodelist = getAltNodeList( std::string( start+1, ei-start-2));
			std::vector<std::string>::const_iterator si = nodelist.begin(), se = nodelist.end();
			for (; si != se; ++si)
			{
				std::vector<TreeNode>::iterator ni = getOrCreateChildNode( *si, false);
				rt &= ni->addFollow( id_, ei, valueType_, examples_);
			}
			return rt;
		}
		else
		{
			if (*ei == '@')
			{
				nodeIsAttribute = true;
				++ei;
			}
			char const* start = ei;
			ei = skipElement( ei);
			
			std::string nodename( start, ei-start);
			std::vector<TreeNode>::iterator ni = getOrCreateChildNode( nodename, nodeIsAttribute);
			return ni->addFollow( id_, ei, valueType_, examples_);
		}
	}

	static const char* schemaAtomTypeName( papuga_Type tp)
	{
		switch (tp)
		{
			case papuga_TypeVoid: break;
			case papuga_TypeDouble: return "xs:decimal";
			case papuga_TypeInt: return "xs:integer";
			case papuga_TypeBool: return "xs:boolean";
			case papuga_TypeString: return "xs:string";
			case papuga_TypeHostObject: break;
			case papuga_TypeSerialization: break;
			case papuga_TypeIterator: break;
		}
		return NULL;
	}

	std::string elementUseSpecifier() const
	{
		switch (resolveType)
		{
			case papuga_ResolveTypeRequired:
				return " minOccurs=\"1\"";
			case papuga_ResolveTypeOptional:
				return " minOccurs=\"0\"";
			case papuga_ResolveTypeInherited:
			case papuga_ResolveTypeArray:
				return " minOccurs=\"0\" maxOccurs=\"unbounded\"";
			case papuga_ResolveTypeArrayNonEmpty:
				return " minOccurs=\"1\" maxOccurs=\"unbounded\"";
		}
		return std::string();
	}
	std::string attributeUseSpecifier() const
	{
		switch (resolveType)
		{
			case papuga_ResolveTypeRequired:
				return " use=\"required\"";
			case papuga_ResolveTypeOptional:
				break;
			case papuga_ResolveTypeInherited:
			case papuga_ResolveTypeArray:
			case papuga_ResolveTypeArrayNonEmpty:
				break;
		}
		return std::string();
	}

	void printSchemaElements( std::ostream& out) const
	{
		switch (valueType)
		{
			case papuga_TypeVoid:
			{
				std::vector<TreeNode>::const_iterator ci = chld.begin(), ce = chld.end();
				out << "<xs:element name=\"" << name << "\"" << elementUseSpecifier() << ">\n";
				out << "<xs:complexType>\n";
				for (; ci != ce; ++ci)
				{
					if (ci->isAttribute) ci->printSchemaElements( out);
				}
				out << "<xs:any>\n";
				for (; ci != ce; ++ci)
				{
					if (!ci->isAttribute) ci->printSchemaElements( out);
				}
				out << "</xs:any>\n";
				out << "</xs:complexType>\n";
				out << "</xs:element>\n";
				break;
			}
			case papuga_TypeDouble:
			case papuga_TypeInt:
			case papuga_TypeBool:
			case papuga_TypeString:
				if (isAttribute)
				{
					out << "<xs:attribute name=\"" << name << "\" type=\"" << schemaAtomTypeName( valueType) << attributeUseSpecifier() << "\"/>\n";
				}
				else
				{
					out << "<xs:element name=\"" << name << "\" type=\"" << schemaAtomTypeName( valueType) << elementUseSpecifier() << "\"/>\n";
				}
				break;
			case papuga_TypeHostObject:
				throw LogicErrorException();
			case papuga_TypeSerialization:
				throw LogicErrorException();
			case papuga_TypeIterator:
				throw LogicErrorException();
		}
	}
};

class SchemaDescription
{
public:
	mutable papuga_ErrorCode lasterr;
	TreeNode tree;
	std::vector<TreeNode> unresolved;
	bool done;

	SchemaDescription()
		:lasterr(papuga_Ok),tree(),unresolved(),done(false){}

	void addElement( int id, const char* expression, papuga_Type valueType, const char* examples)
	{
		if (expression[0] == '/')
		{
			if (expression[1] == '/')
			{
				unresolved.push_back( TreeNode( expression, id, false/*is attribute*/, valueType, examples));
			}
			else
			{
				if (isDelimiter( expression[ 1]))
				{
					tree.addFollow( id, expression+1, valueType, examples);
				}
				else
				{
					if (!tree.addElement( id, expression+1, valueType, examples))
					{
						unresolved.push_back( TreeNode( expression, id, false/*is attribute*/, valueType, examples));
					}
				}
			}
		}
		else if (isDelimiter(*expression))
		{
			if (!tree.addFollow( id, expression, valueType, examples))
			{
				unresolved.push_back( TreeNode( expression, id, false/*is attribute*/, valueType, examples));
			}
		}
		else
		{
			if (!tree.addElement( id, expression+1, valueType, examples))
			{
				unresolved.push_back( TreeNode( expression, id, false/*is attribute*/, valueType, examples));
			}
		}
	}
	bool addRelation( int sink_id, int source_id, papuga_ResolveType resolveType_)
	{
		return true;
	}
	void finish()
	{
		done = true;
	}

	std::string buildExample() const
	{
		return std::string();
	}
	std::string buildSchemaText() const
	{
		std::ostringstream out;
		out << "<?xml version=\"1.0\"?>\n";
		out << "<xs:schema xmlns:xs=\"http://www.w3.org/2001/XMLSchema\">\n";

		out << "<xs:element name=\"" << tree.name << "\">\n";
		out << "</xs:element>\n";
		tree.printSchemaElements( out);
		out << "</xs:schema>\n";
		return out.str();
	}
};

typedef struct papuga_SchemaDescription
{
	SchemaDescription impl;
} papuga_SchemaDescription;


extern "C" papuga_SchemaDescription* papuga_create_SchemaDescription()
{
	try
	{
		papuga_SchemaDescription* rt = new papuga_SchemaDescription();
		return rt;
	}
	catch (const std::bad_alloc&)
	{
		return NULL;
	}
	catch (const std::runtime_error& err)
	{
		return NULL;
	}
}

extern "C" void papuga_destroy_SchemaDescription( papuga_SchemaDescription* self)
{
	delete self;
}

extern "C" papuga_ErrorCode papuga_SchemaDescription_last_error( const papuga_SchemaDescription* self)
{
	return self->impl.lasterr;
}

static papuga_ErrorCode getExceptionError( const char* name)
{
	if (0==std::strcmp( name, "syntax error")) return papuga_SyntaxError;
	if (0==std::strcmp( name, "logic error")) return papuga_LogicError;
	return papuga_TypeError;
}

extern "C" bool papuga_SchemaDescription_add_element( papuga_SchemaDescription* self, int id, const char* expression, papuga_Type valueType, const char* examples)
{
	try
	{
		if (self->impl.done)
		{
			self->impl.lasterr = papuga_ExecutionOrder;
			return false;
		}
		self->impl.addElement( id, expression, valueType, examples);
		self->impl.lasterr = papuga_Ok;
		return true;
	}
	catch (const std::bad_alloc&)
	{
		self->impl.lasterr = papuga_NoMemError;
		return false;
	}
	catch (const std::runtime_error& err)
	{
		self->impl.lasterr = getExceptionError( err.what());
		return false;
	}
}

extern "C" bool papuga_SchemaDescription_add_relation( papuga_SchemaDescription* self, int sink_id, int source_id, papuga_ResolveType resolveType)
{
	try
	{
		if (self->impl.done)
		{
			self->impl.lasterr = papuga_ExecutionOrder;
			return false;
		}
		self->impl.addRelation( sink_id, source_id, resolveType);
		self->impl.lasterr = papuga_Ok;
		return true;
	}
	catch (const std::bad_alloc&)
	{
		self->impl.lasterr = papuga_NoMemError;
		return false;
	}
	catch (const std::runtime_error& err)
	{
		self->impl.lasterr = getExceptionError( err.what());
		return false;
	}
}

extern "C" bool papuga_SchemaDescription_done( papuga_SchemaDescription* self)
{
	try
	{
		if (!self->impl.done) return true;
		self->impl.finish();
		self->impl.lasterr = papuga_Ok;
		return true;
	}
	catch (const std::bad_alloc&)
	{
		self->impl.lasterr = papuga_NoMemError;
		return false;
	}
	catch (const std::runtime_error& err)
	{
		self->impl.lasterr = getExceptionError( err.what());
		return false;
	}
}

static const char* copyString( papuga_Allocator* allocator, const std::string& str)
{
	char* rt = (char*)papuga_Allocator_alloc( allocator, str.size()+1, 1);
	if (!rt) throw std::bad_alloc();
	std::memcpy( rt, str.c_str(), str.size()+1);
	return rt;
}

extern "C" const char* papuga_SchemaDescription_get_text( const papuga_SchemaDescription* self, papuga_Allocator* allocator, papuga_ContentType doctype, papuga_StringEncoding enc)
{
	try
	{
		if (!self->impl.done)
		{
			self->impl.lasterr = papuga_ExecutionOrder;
			return NULL;
		}
		const char* rt = copyString( allocator, self->impl.buildSchemaText());
		self->impl.lasterr = papuga_Ok;
		return rt;
	}
	catch (const std::bad_alloc&)
	{
		self->impl.lasterr = papuga_NoMemError;
		return NULL;
	}
	catch (const std::runtime_error& err)
	{
		self->impl.lasterr = getExceptionError( err.what());
		return NULL;
	}
}

extern "C" const char* papuga_SchemaDescription_get_example( const papuga_SchemaDescription* self, papuga_Allocator* allocator, papuga_ContentType doctype, papuga_StringEncoding enc)
{
	try
	{
		if (!self->impl.done)
		{
			self->impl.lasterr = papuga_ExecutionOrder;
			return NULL;
		}
		const char* rt = copyString( allocator, self->impl.buildExample());
		self->impl.lasterr = papuga_Ok;
		return rt;
	}
	catch (const std::bad_alloc&)
	{
		self->impl.lasterr = papuga_NoMemError;
		return NULL;
	}
	catch (const std::runtime_error& err)
	{
		self->impl.lasterr = getExceptionError( err.what());
		return NULL;
	}
}


