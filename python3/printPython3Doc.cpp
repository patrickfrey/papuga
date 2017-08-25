/*
 * Copyright (c) 2017 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/// \brief Module for printing the Python (v3) documentation
/// \file printPython3Doc.cpp
#include "printPython3Doc.hpp"
#include "private/sourceDoc.hpp"
#include <sstream>
#include <string>
#include <vector>
#include <cstring>
#include <stdexcept>
#include <algorithm>
#include <cstdio>

using namespace papuga;

class Python3LanguageDescription
	:public SourceDocLanguageDescription
{
public:
	explicit Python3LanguageDescription( const papuga_InterfaceDescription* descr_)
		:m_descr(descr_){}

	virtual const char* eolncomment() const
	{
		return "#";
	}

	virtual std::string mapCodeExample( const std::string& example) const
	{
		std::ostringstream out;
		printCodeSnippet( out, example);
		return out.str();
	}

	virtual std::string classStartDeclaration( const papuga_ClassDescription* classdef) const
	{
		std::ostringstream out;
		out << "class " << classdef->name << ":" << std::endl;
		return out.str();
	}

	virtual std::string classEndDeclaration( const papuga_ClassDescription*) const
	{
		std::ostringstream out;
		out << "" << std::endl;
		return out.str();
	}

	virtual std::string constructorDeclaration( const std::string& classname, const papuga_ConstructorDescription* cdef) const
	{
		std::ostringstream out;
		out << "\t" << "def __init__( self, ";
		printParameterList( out, cdef->parameter);
		out << "):" << std::endl << "\t\t" << "pass" << std::endl;
		return out.str();
	}

	virtual std::string methodDeclaration( const std::string& classname, const papuga_MethodDescription* mdef) const
	{
		std::ostringstream out;
		out << "\t" << "def " << mdef->name << "(";
		if (mdef->nonstatic) out << " self, ";
		printParameterList( out, mdef->parameter);
		out << "):" << std::endl << "\t\t" << "pass" << std::endl;
		return out.str();
	}

private:
	std::string fullclassname( const std::string& classname) const
	{
		std::string rt = m_descr->name;
		std::transform( rt.begin(), rt.end(), rt.begin(), ::tolower);
		rt.append( "_");
		rt.append( classname);
		return rt;
	}

	static void printParameterList(
			std::ostream& out,
			const papuga_ParameterDescription* parameter)
	{
		if (!parameter) return;
		papuga_ParameterDescription const* pi = parameter;
		for (int pidx=0; pi->name; ++pi,++pidx)
		{
			if (pidx) out << ", ";
			out << pi->name;
		}
	}

	static const char* skipSpaces( char const* ei)
	{
		for (; *ei && *ei != '\n' && (unsigned char)*ei <= 32; ++ei){}
		return ei;
	}

	static void printCodeSnippetSeparator( std::ostream& out, char const* ei)
	{
		ei = skipSpaces( ei);
		if (*ei && *ei != '(' && *ei != ')' && *ei != ']' && *ei != ',' && *ei != '\n')
		{
			out << ", ";
		}
	}

	static void printCodeSnippet( std::ostream& out, const std::string& examples)
	{
		if (!examples[0]) return;
		unsigned int bcnt = 0;
		unsigned int ocnt = 0;
		char const* ei = examples.c_str();
		while (*ei)
		{
			ei = skipSpaces( ei);
			if (!*ei) break;
			if (*ei == '\n')
			{
				++ei;
				out << std::endl;
			}
			else if (*ei == '(')
			{
				ei = skipSpaces( ei+1);
				++ocnt;
				out << "(";
			}
			else if (*ei == ')')
			{
				ei = skipSpaces( ei+1);
				--ocnt;
				out << ")";
				printCodeSnippetSeparator( out, ei);
			}
			else if (*ei == '[')
			{
				ei = skipSpaces( ei+1);
				++bcnt;
				out << "{";
			}
			else if (*ei == ']')
			{
				ei = skipSpaces( ei+1);
				--bcnt;
				out << "}";
				printCodeSnippetSeparator( out, ei);
			}
			else if (*ei == '\"' || *ei == '\'')
			{
				const char* start = ei;
				char eb = *ei;
				++ei;
				for (; *ei && *ei != eb && *ei != '\n'; ++ei)
				{
					if (*ei == '\\' && *(ei+1)) ++ei;
				}
				out << std::string( start, ei-start) << (char)eb;
				if (*ei != eb) throw std::runtime_error("string not correctly terminated");
				++ei;
				printCodeSnippetSeparator( out, ei);
			}
			else if (*ei == '-' || (*ei >= '0' && *ei <= '9'))
			{
				out << *ei++;
				for (; *ei >= '0' && *ei <= '9'; ++ei) out << *ei;
				if (*ei == '.') out << *ei++;
				for (; *ei >= '0' && *ei <= '9'; ++ei) out << *ei;
				if (*ei == 'E') out << *ei++;
				if (*ei == '-') out << *ei++;
				for (; *ei >= '0' && *ei <= '9'; ++ei) out << *ei;
				printCodeSnippetSeparator( out, ei);
			}
			else if (*ei == '_' || ((*ei|32) >= 'a' && (*ei|32) <= 'z'))
			{
				out << *ei++;
				for (; (*ei >= '0' && *ei <= '9') || *ei == '_' || ((*ei|32) >= 'a' && (*ei|32) <= 'z') ; ++ei) out << *ei;
				for (; *ei && *ei <= 32; ++ei){}
				if (*ei == ':' || *ei == '=')
				{
					out << '=';
					++ei;
				}
				else
				{
					printCodeSnippetSeparator( out, ei);
				}
			}
			else if (*ei == ',')
			{
				out << ", ";
				ei = skipSpaces( ei+1);
			}
			else
			{
				char buf[ 1024];
				std::snprintf( buf, sizeof(buf), "syntax error in example, unexpected token '%c'", *ei);
				throw std::runtime_error( buf);
			}
		}
		if (ocnt || bcnt)
		{
			std::string expr(
				examples.c_str(), 
				std::min( examples.size(), (std::size_t)60));
			char buf[ 1024];
			if (ocnt) std::snprintf( buf, sizeof(buf), "oval brackets ( ) are not balanced in expression at '%s...'", expr.c_str());
			if (bcnt) std::snprintf( buf, sizeof(buf), "square brackets [ ] are not balanced in expression at '%s...'", expr.c_str());
			throw std::runtime_error( buf);
		}
	}
private:
	const papuga_InterfaceDescription* m_descr;
};


void papuga::printPython3Doc(
		std::ostream& out,
		const papuga_InterfaceDescription& descr)
{
	Python3LanguageDescription lang( &descr);
	printSourceDoc( out, &lang, descr);
}

