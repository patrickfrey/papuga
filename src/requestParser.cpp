/*
 * Copyright (c) 2019 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/* @brief Structures and functions for scanning papuga XML and JSON documents for further processing
 * @file requestParser.cpp
 */
#include "papuga/requestParser.h"

static char nextNonSpaceChar( char const*& si, const char* se)
{
	for (; si != se && (unsigned char)*si <= 32; ++si){}
	return (si != se) ? *si : 0;
}

static char nextNonNullChar( char const*& si, const char* se)
{
	for (; si != se && (unsigned char)*si == 0; ++si){}
	return (si != se) ? *si : 0;
}

static bool skipUntil( char const*& si, const char* se, char delim)
{
	for (; si != se && (unsigned char)*si != delim; ++si){}
	return (si != se);
}

static void skipXmlHeader( char const*& si, const char* se)
{
	if (nextNonSpaceChar( si, se) != '<') return;
	++si;
	if (nextNonSpaceChar( si, se) == '?')
	{
		++si; if (!skipUntil( si, se, '>')) return;
		++si;
	}
	else
	{
		--si;
	}
}

extern "C" const char* papuga_parseRootElement_xml( char* buf, size_t bufsize, const char* src, size_t srcsize, papuga_ErrorCode* errcode)
{
	char const* si = src;
	const char* se = src + srcsize;
	skipXmlHeader( si, se);
	if (si == se)
	{
		*errcode = papuga_InvalidContentType;
		return 0;
	}
	if (nextNonSpaceChar( si, se) != '<')
	{
		*errcode = papuga_InvalidContentType;
		return 0;
	}
	++si;
	size_t bufpos = 0;
	char eb = '>';
	char hdrch = nextNonNullChar( si, se);
	for (; hdrch && hdrch != eb; ++si,hdrch = nextNonNullChar( si, se))
	{
		if (bufpos >= bufsize)
		{
			*errcode = papuga_BufferOverflowError;
			return 0;
		}
		buf[ bufpos++] = hdrch;
	}
	if (bufpos >= bufsize || hdrch != eb)
	{
		*errcode = papuga_BufferOverflowError;
		return 0;
	}
	buf[ bufpos] = 0;
	return buf;
}

extern "C" const char* papuga_parseRootElement_json( char* buf, size_t bufsize, const char* src, size_t srcsize, papuga_ErrorCode* errcode)
{
	char const* si = src;
	const char* se = src + srcsize;
	if (nextNonSpaceChar( si, se) != '{')
	{
		*errcode = papuga_InvalidContentType;
		return 0;
	}
	++si;
	if (nextNonSpaceChar( si, se) != '"')
	{
		*errcode = papuga_InvalidContentType;
		return 0;
	}
	++si;
	size_t bufpos = 0;
	char eb = '"';
	char hdrch = nextNonNullChar( si, se);
	for (; hdrch && hdrch != eb; ++si,hdrch = nextNonNullChar( si, se))
	{
		if (bufpos >= bufsize)
		{
			*errcode = papuga_BufferOverflowError;
			return 0;
		}
		buf[ bufpos++] = hdrch;
	}
	if (bufpos >= bufsize || hdrch != eb)
	{
		*errcode = papuga_BufferOverflowError;
		return 0;
	}
	buf[ bufpos] = 0;
	return buf;
}

