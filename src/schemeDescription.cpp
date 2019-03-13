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
	if (ch == '/') return true;
	if (ch == '@') return true;
	if (ch == '[') return true;
	if (ch == '\0') return true;
	if (ch == ' ') return true;
	return false;
}

static char const* skipBrackets( char const* src)
{
	sb = *src++;
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

static char const* skipElement( char const* src)
{
	for (; !isDelimiter(src); ++src){}
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
	TreeNode( const std::string& name_, papuga_Type valueType_, papuga_ResolveType resolveType_, const char** examples_)
		:name(name_),valueType(valueType_),resolveType(resolveType_),examples(),chld()
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
	std::vector<TreeNode>::iterator findNode( const std::string& name)
	{
		std::vector<TreeNode>::iterator ci = chld.begin(), ce = chld.end();
		for (; ci != ce; ++ci)
		{
			if (name == ci->name) return ci;
		}
		return chld.end();
	}

	bool addElement( const char* expression, papuga_Type valueType, papuga_ResolveType resolveType, const char** examples)
	{
		char const* ei = expression;
		char const* start = ei;
		if (*ei == '@')
		{
			ei = skipElement( ++ei);
			std::string name( start, ei-start);
			if (*ei == '/')
			{
				++ei;
				if (*ei == '/')
				{
					return false;
				}
				std::vector<TreeNode>::iterator ni = findNode( name);
				if (ni == chld.end())
				{
					chld.push_back( TreeNode( name, papuga_TypeVoid, papuga_ResolveTypeRequired, NULL/*examples*/));
					ni = chld.end();
					--ni;
				}
				ni->addElement( ei, valueType, resolveType, examples);
			}
			else if (*ei == '@')
			{
				!!!!! HIE WIITER !!!!!
			}
		}
		if (expression[0] == '/' && expression[1] == '/')
		{
			unresolved = true;
			return;
		}
		if (expression[0] == '/')
		{
			
		}
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
		char const* src = expression;
		if (std::strstr( expression, "//"))
		{
			unresolved.push_back( TreeNode( expression, valueType, resolveType, examples));
		}
		else
		{
			
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

