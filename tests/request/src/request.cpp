/*
 * Copyright (c) 2017 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/// \brief Some classes and functions for building test requests in a convenient way
/// \file request.cpp
#include "request.hpp"
#include <stdexcept>
#include <cstdio>

using namespace papuga;
using namespace papuga::test;

static bool skipSpaces( char const*& si)
{
	while (*si && (unsigned char)*si <= 32) ++si;
	return *si != 0;
}

static bool isDigit( char ch)
{
	return (ch >= '0' && ch <= '9');
}

static bool isAlpha( char ch)
{
	ch |= 32;
	return (ch >= 'a' && ch <= 'z') || ch == '_';
}

static bool isAlnum( char ch)
{
	return isAlpha(ch)||isDigit(ch);
}

static std::string parse_identifier( char const*& si)
{
	std::string rt;
	skipSpaces( si);
	if (!isAlpha(*si)) throw std::runtime_error("identifier expected");
	for (; isAlnum(*si); ++si)
	{
		rt.push_back( *si);
	}
	skipSpaces(si);
	return rt;
}

void RequestAutomaton_FunctionDef::parseCall( const char* call)
{
	char const* si = 0;
	try
	{
		si = call;
		if (!skipSpaces(si)) throw std::runtime_error("call is empty");
		std::string name = parse_identifier( si);
		
		if (*si == '=')
		{
			resultvarname = name;
			++si;
			name = parse_identifier( si);
		}
		if (si[0] == '-' && si[1] == '>')
		{
			selfvarname = name;
			si += 2;
			name = parse_identifier( si);
		}
		if (*si == ':')
		{
			classname = name;
			++si;
			name = parse_identifier( si);
		}
		methodname = name;
	}
	catch (const std::runtime_error& err)
	{
		char errbuf[ 2048];
		std::snprintf( errbuf, sizeof( errbuf), "error in call call '%s' at position %u: %s", call, (int)(si-call), err.what());
		throw std::runtime_error( errbuf);
	}
}


