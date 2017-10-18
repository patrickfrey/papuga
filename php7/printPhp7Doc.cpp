/*
 * Copyright (c) 2017 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/// \brief Module for printing the PHP (v7) documentation
/// \file printPhp7Doc.cpp
#include "printPhp7Doc.hpp"
#include "private/sourceDoc.hpp"
#include <sstream>
#include <string>
#include <vector>
#include <cstring>
#include <stdexcept>
#include <algorithm>
#include <cstdio>

using namespace papuga;

class Php7LanguageDescription
	:public SourceDocLanguageDescription
{
public:
	explicit Php7LanguageDescription( const papuga_InterfaceDescription* descr_)
		:m_descr(descr_){}

	virtual const char* eolncomment() const
	{
		return "//";
	}

	virtual std::string mapCodeExample( const SourceDocExampleNode* example) const
	{
		std::ostringstream out;
		printCodeSnippet( out, example);
		return out.str();
	}

	virtual std::string classStartDeclaration( const papuga_ClassDescription* classdef) const
	{
		std::ostringstream out;
		out << "class " << classdef->name << " {" << std::endl;
		return out.str();
	}

	virtual std::string classEndDeclaration( const papuga_ClassDescription*) const
	{
		std::ostringstream out;
		out << "}" << std::endl;
		return out.str();
	}

	virtual std::string constructorDeclaration( const std::string& classname, const papuga_ConstructorDescription* cdef) const
	{
		std::ostringstream out;
		out << "function " << fullclassname(classname) << "::" << "__constructor(";
		printParameterList( out, cdef->parameter);
		out << ")" << std::endl << "end" << std::endl;
		return out.str();
	}

	virtual std::string methodDeclaration( const std::string& classname, const papuga_MethodDescription* mdef) const
	{
		std::ostringstream out;
		out << "function " << fullclassname(classname) << ":" << mdef->name << "(";
		printParameterList( out, mdef->parameter);
		out << ")" << std::endl << "end" << std::endl;
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
			out << "$" << pi->name;
		}
	}

	static bool isAlpha( char ch)
	{
		if ((ch|32) >= 'a' && (ch|32) <= 'z') return true;
		if (ch == '_') return true;
		return false;
	}

	static void printCodeSnippet( std::ostream& out, const SourceDocExampleNode* example)
	{
		SourceDocExampleNode const* ei = example;
		for (; ei; ei = ei->next)
		{
			if (ei->proc)
			{
				out << ei->proc << "( ";
				printCodeSnippet( out, ei->chld);
				out << ")";
				if (ei->next)
				{
					out << ", ";
				}
				continue;
			}
			if (ei->name)
			{
				out << ei->name;
				out << "=>";
			}
			if (ei->value)
			{
				if (std::strcmp( ei->value, "false") == 0)
				{
					out << "FALSE";
				}
				else if (std::strcmp( ei->value, "true") == 0)
				{
					out << "TRUE";
				}
				else if (isAlpha( ei->value[0]))
				{
					out << '$' << ei->value;
				}
				else
				{
					out << ei->value;
				}
			}
			else
			{
				out << "[";
				printCodeSnippet( out, ei->chld);
				out << "]";
			}
			if (ei->next)
			{
				out << ", ";
			}
		}
	}

private:
	const papuga_InterfaceDescription* m_descr;
};


void papuga::printPhp7Doc(
		std::ostream& out,
		const papuga_InterfaceDescription& descr)
{
	Php7LanguageDescription lang( &descr);
	printSourceDoc( out, &lang, descr);
}


