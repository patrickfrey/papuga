/*
 * Copyright (c) 2019 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/*
* \brief Automaton to describe and build papuga XML and JSON requests
* \file schemeDescription.cpp
*/
#include "papuga/schemeDescription.h"
#include <vector>
#include <utility>
#include <new>
#include <stdexcept>
#include <cstring>
#include <cstdlib>

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
	else throw std::runtime_error("syntax error");

	for (; *src && *src != eb; ++src)
	{
		if (*src == sb) throw std::runtime_error("syntax error");
	}
	if (*src != eb) throw std::runtime_error("syntax error");
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
				case '\0': throw std::runtime_error("syntax error");
				default: rt.push_back( *src); break;
			}
		}
		else
		{
			rt.push_back( *src);
		}
	}
	if (*src != eb) throw std::runtime_error("syntax error");
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
	papuga_Type valueType;
	papuga_ResolveType resolveType;
	std::vector<std::string> examples;
	std::vector<TreeNode> chld;

	TreeNode()
		:name(),valueType(papuga_TypeVoid),resolveType(papuga_ResolveTypeRequired),examples(),chld(){}
	TreeNode( const TreeNode& o)
		:name(o.name),valueType(o.valueType),resolveType(o.resolveType),examples(o.examples),chld(o.chld){}
	TreeNode& operator = (const TreeNode& o)
		{name=o.name; valueType=o.valueType; resolveType=o.resolveType; examples=o.examples; chld=o.chld; return *this;}
#if __cplusplus >= 201103L
	TreeNode( TreeNode&& o)
		:name(std::move(o.name)),valueType(o.valueType),resolveType(o.resolveType),examples(std::move(o.examples)),chld(std::move(o.chld)){}
	TreeNode& operator = (TreeNode&& o)
		{name=std::move(o.name); valueType=o.valueType; resolveType=o.resolveType; examples=std::move(o.examples); chld=std::move(o.chld); return *this;}
#endif
	TreeNode( const std::string& name_, papuga_Type valueType_, papuga_ResolveType resolveType_, const std::vector<std::string>& examples_=std::vector<std::string>())
		:name(name_),valueType(valueType_),resolveType(resolveType_),examples(examples_),chld()
	{}
	TreeNode( const std::string& name_, papuga_Type valueType_, papuga_ResolveType resolveType_, const char** examples_)
		:name(name_),valueType(valueType_),resolveType(resolveType_),examples(),chld()
	{
		addExamples( examples_);
	}
	void addExamples( const char** examples_)
	{
		if (examples_)
		{
			char const* const* ei = examples_;
			for (; *ei; ++ei)
			{
				examples.push_back( *ei);
			}
		}
	}

	void addChild( const TreeNode& o)
	{
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
				char const* start = src;
				if (*src == '@') ++src;
				src = skipElement( ++src);
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
					chld.push_back( TreeNode( condAttrName, papuga_TypeVoid, papuga_ResolveTypeRequired, condExamples));
				}
				else
				{
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

	bool addFollow( const char* expression_, papuga_Type valueType_, papuga_ResolveType resolveType_, const char** examples_)
	{
		bool isAttribute = !name.empty() && name[0] == '@';
		char const* ei = expression_;
		if (*ei == '[')
		{
			addConditionElements( ei);
			ei = skipBrackets( ei);
		}
		if (*ei == '/')
		{
			if (isAttribute) throw std::runtime_error("syntax error");
			++ei;
			if (*ei == '/')
			{
				return false;
			}
			return addElement( ei, valueType_, resolveType_, examples_);
		}
		else if (*ei == '@')
		{
			if (isAttribute) throw std::runtime_error("syntax error");
			return addElement( ei, valueType_, resolveType_, examples_);
		}
		else if (*ei == '(' || *ei == '\0')
		{
			valueType = valueType_;
			resolveType = resolveType_;
			addExamples( examples_);
			return true;
		}
		else
		{
			throw std::runtime_error("syntax error");
		}
	}

	bool addElement( const char* expression_, papuga_Type valueType_, papuga_ResolveType resolveType_, const char** examples_)
	{
		char const* ei = expression_;
		char const* start = ei;
		bool isAttribute = false;
		if (*ei == '@')
		{
			++ei;
			isAttribute = true;
		}
		ei = skipElement( ++ei);
		
		std::string nodename( start, ei-start);
		std::vector<TreeNode>::iterator ni = findNode( nodename);
		if (ni == chld.end())
		{
			chld.push_back( TreeNode( name, papuga_TypeVoid, papuga_ResolveTypeRequired));
			ni = chld.end();
			--ni;
		}
		return ni->addFollow( ei, valueType_, resolveType_, examples_);
	}
};

class SchemeDescription
{
public:
	papuga_ErrorCode lasterr;
	TreeNode tree;
	std::vector<TreeNode> unresolved;
	std::string text;
	std::string example;
	
	SchemeDescription()
		:lasterr(papuga_Ok),tree(),unresolved(),text(),example(){}

	void addElement( const char* expression, papuga_Type valueType, papuga_ResolveType resolveType, const char** examples)
	{
		if (expression[0] == '/')
		{
			if (expression[1] == '/')
			{
				unresolved.push_back( TreeNode( expression, valueType, resolveType, examples));
			}
			else
			{
				if (isDelimiter( expression[ 1]))
				{
					tree.addFollow( expression+1, valueType, resolveType, examples);
				}
				else
				{
					tree.addElement( expression+1, valueType, resolveType, examples);
				}
			}
		}
		else
		{
			tree.addFollow( expression, valueType, resolveType, examples);
		}
	}
};

typedef struct papuga_SchemeDescription
{
	SchemeDescription impl;
} papuga_SchemeDescription;


extern "C" papuga_SchemeDescription* papuga_create_SchemeDescription( const char* name)
{
	try
	{
		papuga_SchemeDescription* rt = new papuga_SchemeDescription();
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

extern "C" void papuga_destroy_SchemeDescription( papuga_SchemeDescription* self)
{
	delete self;
}

extern "C" papuga_ErrorCode papuga_SchemeDescription_last_error( const papuga_SchemeDescription* self)
{
	return self->impl.lasterr;
}

extern "C" bool papuga_SchemeDescription_add_element( papuga_SchemeDescription* self, const char* expression, papuga_Type valueType, papuga_ResolveType resolveType, const char** examples)
{
	try
	{
		self->impl.addElement( expression, valueType, resolveType, examples);
		self->impl.lasterr = papuga_Ok;
		return true;
	}
	catch (const std::bad_alloc&)
	{
		self->impl.lasterr = papuga_NoMemError;
		return false;
	}
	catch (const std::runtime_error&)
	{
		self->impl.lasterr = papuga_SyntaxError;
		return false;
	}
}

extern "C" bool papuga_SchemeDescription_finish( papuga_SchemeDescription* self)
{
	try
	{
		self->impl.lasterr = papuga_Ok;
		return true;
	}
	catch (const std::bad_alloc&)
	{
		self->impl.lasterr = papuga_NoMemError;
		return false;
	}
	catch (const std::runtime_error&)
	{
		self->impl.lasterr = papuga_SyntaxError;
		return false;
	}
}

extern "C" const char* papuga_SchemeDescription_get_text( const papuga_SchemeDescription* self)
{
	return self->impl.text.c_str();
}

extern "C" const char* papuga_SchemeDescription_get_example( const papuga_SchemeDescription* self)
{
	return self->impl.example.c_str();
}

