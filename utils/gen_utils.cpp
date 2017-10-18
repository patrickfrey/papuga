/*
* Copyright (c) 2017 Patrick P. Frey
*
* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/
/// \brief Some utility functions for generating language binding sources
/// \file utils.cpp
#include "private/gen_utils.hpp"
#include <cstdarg>
#include <iostream>
#include <sstream>
#include <cstdio>
#include <cstring>
#include <cerrno>
#include <stdint.h>

using namespace papuga;

std::string papuga::cppCodeSnippet( unsigned int idntcnt, ...)
{
	std::ostringstream rt;
	std::string indent( idntcnt, '\t');
	va_list args;
	va_start( args, idntcnt);
	unsigned int ai = 0;
	for (;; ++ai)
	{
		const char* ln = va_arg( args,const char*);
		if (!(unsigned int)(uintptr_t)ln) break;
		std::size_t nn = std::strlen( ln);
		if (nn == 0)
		{
			rt << std::endl;
		}
		else if (ln[0] == '#')
		{
			rt << ln << std::endl;
		}
		else if (ln[nn-1] == '{')
		{
			rt << indent << ln << "{" << std::endl;
			indent += "\t";
		}
		else if (ln[0] == '}')
		{
			if (indent.empty()) throw std::runtime_error( "format string error");
			indent.resize( indent.size() -1);
			rt << indent << "}" << ln << std::endl;
		}
		else
		{
			rt << indent << ln << std::endl;
		}
	}
	va_end (args);
	return rt.str();
}

std::vector<std::string> papuga::getGeneratorArguments(
	const std::multimap<std::string,std::string>& args,
	const char* name)
{
	typedef std::multimap<std::string,std::string>::const_iterator ArgIterator;
	std::pair<ArgIterator,ArgIterator> argrange = args.equal_range( name);
	ArgIterator ai = argrange.first, ae = argrange.second;
	std::vector<std::string> rt;
	for (; ai != ae; ++ai)
	{
		rt.push_back( ai->second);
	}
	return rt;
}

std::string papuga::getGeneratorArgument(
	const std::multimap<std::string,std::string>& args,
	const char* name,
	const char* defaultval)
{
	typedef std::multimap<std::string,std::string>::const_iterator ArgIterator;
	std::pair<ArgIterator,ArgIterator> argrange = args.equal_range( name);
	ArgIterator ai = argrange.first, ae = argrange.second;
	std::string rt;
	if (ai == ae)
	{
		if (defaultval)
		{
			rt = defaultval;
		}
		else
		{
			char buf[ 256];
			std::snprintf( buf, sizeof(buf), "missing definition of argument '%s'", name);
			throw std::runtime_error( buf);
		}
	}
	for (int aidx=0; ai != ae; ++ai,++aidx)
	{
		if (aidx)
		{
			char buf[ 256];
			std::snprintf( buf, sizeof(buf), "too many arguments with name '%s' defined", name);
			throw std::runtime_error( buf);
		}
		rt = ai->second;
	}
	return rt;
}

std::string papuga::readFile( const std::string& filename)
{
	int err = 0;
	std::string rt;
	FILE* fh = ::fopen( filename.c_str(), "rb");
	if (!fh)
	{
		err = errno;
		goto ERROR;
	}
	unsigned int nn;
	enum {bufsize=(1<<12)};
	char buf[ bufsize];

	while (!!(nn=::fread( buf, 1/*nmemb*/, bufsize, fh)))
	{
		rt.append( buf, nn);
	}
	if (!feof( fh))
	{
		err = ::ferror( fh);
		::fclose( fh);
		goto ERROR;
	}
	::fclose( fh);
	return rt;
ERROR:
	std::snprintf( buf, sizeof(buf), "error reading file '%s': %s", filename.c_str(), std::strerror(err));
	throw std::runtime_error( buf);
}

void papuga::writeFile( const std::string& filename, const std::string& content)
{
	unsigned char ch;
	FILE* fh = ::fopen( filename.c_str(), "wb");
	if (!fh)
	{
		throw std::runtime_error( std::strerror( errno));
	}
	std::string::const_iterator fi = content.begin(), fe = content.end();
	for (; fi != fe; ++fi)
	{
		ch = *fi;
		if (1 > ::fwrite( &ch, 1, 1, fh))
		{
			int ec = ::ferror( fh);
			if (ec)
			{
				::fclose( fh);
				throw std::runtime_error( std::strerror(ec));
			}
		}
	}
	::fclose( fh);
}

static const char* skipSpaces( char const* ei)
{
	for (; *ei && (unsigned char)*ei <= 32; ++ei){}
	return ei;
}

static bool isAlpha( char ch)
{
	if ((ch|32) >= 'a' && (ch|32) <= 'z') return true;
	if (ch == '_') return true;
	return false;
}

static bool isDigit( char ch)
{
	return (ch >= '0' && ch <= '9');
}

static bool isAlnum( char ch)
{
	return isDigit(ch) || isAlpha( ch);
}

const char* SourceDocExampleTree::parseNumber( char const*& ei)
{
	char const* rt = m_strings.c_str()+m_strings.size();
	if (*ei == '-')
	{
		m_strings.push_back( *ei);
		++ei;
	}
	while (isDigit(*ei))
	{
		m_strings.push_back( *ei);
		++ei;
	}
	if (*ei == '.')
	{
		m_strings.push_back( *ei);
		++ei;
	}
	while (isDigit(*ei))
	{
		m_strings.push_back( *ei);
		++ei;
	}
	if (*ei == 'E')
	{
		m_strings.push_back( *ei);
		++ei;
		if (*ei == '-')
		{
			m_strings.push_back( *ei);
			++ei;
		}
		while (isDigit(*ei))
		{
			m_strings.push_back( *ei);
			++ei;
		}
	}
	m_strings.push_back( '\0');
	ei = skipSpaces( ei);
	return rt;
}

const char* SourceDocExampleTree::parseIdentifier( char const*& ei)
{
	char const* rt = m_strings.c_str()+m_strings.size();
	for (; isAlnum(*ei); ++ei)
	{
		m_strings.push_back( *ei);
	}
	m_strings.push_back( '\0');
	ei = skipSpaces( ei);
	return rt;
}

const char* SourceDocExampleTree::parseString( char const*& ei)
{
	char const* rt = m_strings.c_str()+m_strings.size();
	char eb = *ei++;
	m_strings.push_back( eb);
	for (; *ei && *ei != eb; ++ei)
	{
		if (*ei == '\\')
		{
			m_strings.push_back( *ei++);
			if (!*ei) return 0;
			m_strings.push_back( *ei);
		}
		else
		{
			m_strings.push_back( *ei);
		}
	}
	if (*ei != eb)
	{
		throw std::runtime_error( "string not terminated in example");
	}
	m_strings.push_back( eb);
	m_strings.push_back( '\0');
	ei = skipSpaces( ei+1);
	return rt;
}

SourceDocExampleNode* SourceDocExampleTree::parseExpressionList( char const*& ei, char eb)
{
	SourceDocExampleNode* rt = 0;
	SourceDocExampleNode* chld = 0;
	while (*ei && *ei != eb)
	{
		if (!chld)
		{
			rt = chld = parseExpression( ei);
		}
		else
		{
			chld->next = parseExpression( ei);
			chld = chld->next;
		}
		if (*ei == ',')
		{
			ei = skipSpaces( ei+1);
		}
	}
	if (*ei == eb)
	{
		ei = skipSpaces( ei+1);
	}
	else
	{
		throw std::runtime_error( "expression list not terminated");
	}
	return rt;
}

SourceDocExampleNode* SourceDocExampleTree::parseExpression( char const*& ei)
{
	SourceDocExampleNode nd;
	ei = skipSpaces( ei);
	if (!*ei) return 0;

	if (*ei == '-' || isDigit(*ei))
	{
		nd.name = parseNumber( ei);
	}
	else if (isAlpha(*ei))
	{
		nd.name = parseIdentifier( ei);
		if (*ei == '(')
		{
			nd.proc = nd.name;
			nd.name = 0;
			ei = skipSpaces( ei+1);
			nd.chld = parseExpressionList( ei, ')');

			m_nodes.push_back( nd);
			return &m_nodes.back();
		}
	}
	else if (*ei == '\'' || *ei == '"')
	{
		nd.name = parseString( ei);
	}
	else if (*ei == '[')
	{
		ei = skipSpaces( ei+1);
		nd.chld = parseExpressionList( ei, ']');

		m_nodes.push_back( nd);
		return &m_nodes.back();
	}
	if (*ei == ':')
	{
		ei = skipSpaces( ei+1);
		if (*ei == '\'' || *ei == '"')
		{
			nd.value = parseString( ei);
		}
		else if (*ei == '-' || isDigit(*ei))
		{
			nd.value = parseNumber( ei);
		}
		else if (isAlpha(*ei))
		{
			nd.value = parseIdentifier( ei);
		}
		else if (*ei == '[')
		{
			ei = skipSpaces( ei+1);
			nd.chld = parseExpressionList( ei, ']');
		}
	}
	else if (nd.name)
	{
		nd.value = nd.name;
		nd.name = 0;
	}
	else
	{
		throw std::runtime_error("unexpected token in expression");
	}
	m_nodes.push_back( nd);
	return &m_nodes.back();
}

SourceDocExampleTree::SourceDocExampleTree( const char* source)
	:m_root(0),m_strings(),m_nodes()
{
	if (!source || !source[0]) return;
	m_strings.reserve( std::strlen(source) *4); // ... we rely on the fact that pointers into m_strings remain stable

	char const* ei = source;
	try
	{
		m_root = parseExpressionList( ei, '\0');
	}
	catch (const std::runtime_error& err)
	{
		char buf[ 2048];
		char const* start = ei;
		char const* end = ei;
		int pcnt = 50, fcnt=50;
		for (;start != source && pcnt > 0; --pcnt,--start){}
		for (;*end && fcnt > 0; --pcnt,++end){}
		std::string preerr( start, ei-start);
		std::string posterr( ei, end-ei);
		std::snprintf( buf, sizeof(buf), "error parsing example expression, %s at: [%s <!> %s]", err.what(), preerr.c_str(), posterr.c_str());
		buf[sizeof(buf)-1] = 0;
		throw std::runtime_error( buf);
	}
}

