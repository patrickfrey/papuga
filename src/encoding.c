/*
 * Copyright (c) 2017 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/*
* @brief Parsing and mapping string encoding names
* @file encoding.c
*/
#include "papuga/encoding.h"
#include <string.h>

bool papuga_getStringEncodingFromName( papuga_StringEncoding* encoding, const char* name)
{
	char buf[32];
	int bi=0;
	char const* ni = name;
	if (!name) return false;
	for (; *ni && bi < sizeof(buf)-1; ++ni)
	{
		unsigned char ch = *ni | 32;
		if (ch == '-' || ch == ' ') continue;
		buf[ bi++] = ch;
		if (ch >= '0' && ch <= '9') continue;
		if (ch >= 'a' && ch <= 'z') continue;
		break;
	}
	if (*ni) return false;
	buf[ bi] = 0;
	if (0==strcmp( buf, "utf8")) *encoding = papuga_UTF8; return true;
	if (0==strcmp( buf, "utf16be")) *encoding = papuga_UTF16BE; return true;
	if (0==strcmp( buf, "utf16le")) *encoding = papuga_UTF16LE; return true;
	if (0==strcmp( buf, "utf16")) *encoding = papuga_UTF16; return true;
	if (0==strcmp( buf, "utf32be")) *encoding = papuga_UTF32BE; return true;
	if (0==strcmp( buf, "utf32le")) *encoding = papuga_UTF32LE; return true;
	if (0==strcmp( buf, "utf32")) *encoding = papuga_UTF32; return true;
	if (0==strcmp( buf, "binary")) *encoding = papuga_Binary; return true;
	return false;
}

const char* papuga_stringEncodingName( papuga_StringEncoding encoding)
{
	switch (encoding)
	{
		case papuga_UTF8: return "UTF-8";
		case papuga_UTF16BE: return "UTF-16BE";
		case papuga_UTF16LE: return "UTF-16LE";
		case papuga_UTF16: return "UTF-16";
		case papuga_UTF32BE: return "UTF-32BE";
		case papuga_UTF32LE: return "UTF-32LE";
		case papuga_UTF32: return "UTF-32";
		case papuga_Binary: return "Binary";
	}
	return 0;
}


