/*
 * Copyright (c) 2019 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/// \brief URI encoder for exported links
/// \file "uriEncode.cpp"
#include "papuga/uriEncode.h"
#include <cstdio>
#include <utility>

static inline bool isAlpha( unsigned char ch)
{
	return (ch|32) >= 'a' && (ch|32) <= 'z';
}
static inline bool isDigit( unsigned char ch)
{
	return ch >= '0' && ch <= '9';
}
static inline bool isAlnum( unsigned char ch)
{
	return isAlpha(ch)||isDigit(ch);
}
static inline bool isSpace( unsigned char ch)
{
	return ch == ' ';
}
static inline bool isPunctRfc3986( unsigned char ch)
{
	return ch == '~' || ch == '-' || ch == '.' || ch == '_';
}
static inline bool isPunctHtml5( unsigned char ch)
{
	return ch == '*' || ch == '-' || ch == '.' || ch == '_';
}
static inline bool printCharEncoded( char* buf, size_t bufsize, size_t& bufpos, unsigned char ch, unsigned char ch_mapped)
{
	static const char HEX[17] = "0123456789abcdef";
	if (ch_mapped)
	{
		if (bufpos+1 >= bufsize) return false;
		buf[ bufpos++] = ch_mapped;
	}
	else
	{
		if (bufpos+4 >= bufsize) return false;
		buf[ bufpos++] = '%';
		buf[ bufpos++] = HEX[ ch / 16];
		buf[ bufpos++] = HEX[ ch % 16];
	}
	return true;
}

class CharTableRfc3986
{
	char ar[ 256];

public:
	CharTableRfc3986()
	{
		for (int ii = 0; ii < 256; ii++)
		{
			if (isAlnum(ii) || isPunctRfc3986(ii))
			{
				ar[ ii] = ii;
			}
			else
			{
				ar[ ii] = 0;
			}
		}
	}

	bool encode( char* buf, size_t bufsize, size_t& bufpos, const char* input, size_t inputlen) const
	{
		char const* si = input;
		char const* se = input + inputlen;
		for (; si != se; ++si)
		{
			if (!printCharEncoded( buf, bufsize, bufpos, *si, ar[(unsigned char)*si])) return false;
		}
		buf[ bufpos] = 0;
		return true;
	}
};

class CharTableHtml5
{
	char ar[ 256];

public:
	CharTableHtml5()
	{
		for (int ii = 0; ii < 256; ii++)
		{
			if (isAlnum(ii) || isPunctHtml5(ii))
			{
				ar[ ii] = ii;
			}
			else if (isSpace(ii))
			{
				ar[ ii] = '+';
			}
			else
			{
				ar[ ii] = 0;
			}
		}
	}

	bool encode( char* buf, size_t bufsize, size_t& bufpos, const char* input, size_t inputlen) const
	{
		char const* si = input;
		char const* se = input + inputlen;
		for (; si != se; ++si)
		{
			if (!printCharEncoded( buf, bufsize, bufpos, *si, ar[(unsigned char)*si])) return false;
		}
		buf[ bufpos] = 0;
		return true;
	}
};

static const CharTableRfc3986 g_tab_rfc3986;
static const CharTableHtml5 g_tab_html5;

extern "C" const char* papuga_uri_encode_Html5( char* destbuf, size_t destbufsize, size_t* destlen, const char* input, size_t inputlen, papuga_ErrorCode* err)
{
	if (!g_tab_html5.encode( destbuf, destbufsize, *destlen, input, inputlen))
	{
		*err = papuga_BufferOverflowError;
		return NULL;
	}
	return destbuf;
}

extern "C" const char* papuga_uri_encode_Rfc3986( char* destbuf, size_t destbufsize, size_t* destlen, const char* input, size_t inputlen, papuga_ErrorCode* err)
{
	if (!g_tab_rfc3986.encode( destbuf, destbufsize, *destlen, input, inputlen))
	{
		*err = papuga_BufferOverflowError;
		return NULL;
	}
	return destbuf;
}

