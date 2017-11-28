/*
 * Copyright (c) 2017 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/// \brief Structures and functions for scanning papuga XML and JSON documents for further processing
/// \file requestParser.c
#include "papuga/requestParser.h"
#include <string.h>

void papuga_destroy_RequestParser( papuga_RequestParser* self)
{
	((papuga_RequestParserHeader*)self)->destroy( self);
}

papuga_ErrorCode papuga_RequestParser_last_error( const papuga_RequestParser* self)
{
	return ((papuga_RequestParserHeader*)self)->errcode;
}

int papuga_RequestParser_last_error_pos( const papuga_RequestParser* self)
{
	return ((papuga_RequestParserHeader*)self)->errpos;
}

static bool parse_xml_header( char* hdrbuf, size_t hdrbufsize, const char* src, size_t srcsize)
{
	int state = 0;
	size_t si = 0;
	size_t hi = 0;
	for (; si < srcsize && hi < hdrbufsize; ++si)
	{
		if (src[si] == '\0') continue;
		switch (state)
		{
			case 0:
				if (src[si] == '<')
				{
					state = 1;
				}
				else
				{
					return false;
				}
				break;
			case 1:
				if (src[si] == '?')
				{
					state = 2;
				}
				else
				{
					hdrbuf[0] = 0;
					return true;
				}
				break;
			case 2:
				if (src[si] == '>')
				{
					state = 3;
				}
				else
				{
					return false;
				}
				break;
			case 3:
				hdrbuf[hi] = 0;
				return true;
		}
		hdrbuf[ hi++] = src[si];
	}
	return false;
}

static papuga_StringEncoding detectBOM( const char* src, size_t srcsize, size_t* BOM_size)
{
	*BOM_size = 0;
	if (srcsize < 4) return papuga_Binary;
	const unsigned char* bom = (const unsigned char*)src;
	if (bom[0] == 0xEF && bom[1] == 0xBB && bom[2] == 0xBF) {*BOM_size = 3; return papuga_UTF8;}
	if (bom[0] == 0x00 && bom[1] == 0x00 && bom[2] == 0xFE && bom[3] == 0xFF) {*BOM_size = 4; return papuga_UTF32BE;}
	if (bom[0] == 0xFF && bom[1] == 0xFE && bom[2] == 0x00 && bom[3] == 0x00) {*BOM_size = 4; return papuga_UTF32LE;}
	if (bom[0] == 0xFE && bom[1] == 0xFF) {*BOM_size = 2; return papuga_UTF16BE;}
	if (bom[0] == 0xFF && bom[1] == 0xFE) {*BOM_size = 2; return papuga_UTF16LE;}
	return papuga_Binary;
}

papuga_ContentType papuga_guess_ContentType( const char* src_, size_t srcsize)
{
	char const* src = src_;
	size_t si;
	char hdrbuf[ 256];
	size_t BOM_size;

	(void)detectBOM( src, srcsize, &BOM_size);
	src += BOM_size;
	srcsize -= BOM_size;
	if (parse_xml_header( hdrbuf, sizeof(hdrbuf), src, srcsize)) return papuga_ContentType_XML;
	for (si=0; si<srcsize; ++si)
	{
		if (src[si] == '\'' || src[si] == '\"' || src[si] == '{') return papuga_ContentType_JSON;
		if ((unsigned char)src[si]>32) break;
	}
	return papuga_ContentType_Unknown;
}

static papuga_StringEncoding detectCharsetFromXmlHeader( const char* hdrbuf, size_t hdrbufsize)
{
	char be;
	char const* ee = strstr( hdrbuf, "encoding");
	int encsize;
	char encbuf[ 32];

	if (!ee) return papuga_Binary;
	ee += 8; /* += strlen("encoding")*/
	for (; *ee && (unsigned char)*ee <= 32; ++ee){}
	if (*ee != '=') return papuga_Binary;
	++ee;
	for (; *ee && (unsigned char)*ee <= 32; ++ee){}
	if (*ee != '\'' && *ee != '\"') return papuga_Binary;
	be = *ee++;
	for (encsize=0; encsize<16 && *ee && *ee != be && (signed char)*ee > 0; ++ee)
	{
		if (*ee != ' ' && *ee != '-')
		{
			encbuf[ encsize++] = *ee | 32;
		}
	}
	encbuf[ encsize] = 0;
	if (*ee != be) return papuga_Binary;
	if (strcmp( encbuf, "UTF8")) return papuga_UTF8;
	if (strcmp( encbuf, "UTF16")) return papuga_UTF16BE;
	if (strcmp( encbuf, "UTF32")) return papuga_UTF32BE;
	if (strcmp( encbuf, "UTF16BE")) return papuga_UTF16BE;
	if (strcmp( encbuf, "UTF32BE")) return papuga_UTF32BE;
	if (strcmp( encbuf, "UTF16LE")) return papuga_UTF16LE;
	if (strcmp( encbuf, "UTF32LE")) return papuga_UTF32LE;
	return papuga_Binary;
}

papuga_StringEncoding papuga_guess_StringEncoding( const char* src, size_t srcsize)
{
	char const* ci = src;
	size_t chunksize = 1024;
	const char* ce = (chunksize > srcsize) ? (ci + srcsize) : (ci + chunksize);
	unsigned int zcnt = 0;
	unsigned int max_zcnt = 0;
	unsigned int mcnt[ 4];
	size_t BOM_size;
	char hdrbuf[ 256];

	papuga_StringEncoding encoding = detectBOM( src, srcsize, &BOM_size);
	if (encoding != papuga_Binary) return encoding;

	if (parse_xml_header( hdrbuf, sizeof(hdrbuf), src, srcsize) && hdrbuf[0])
	{
		encoding = detectCharsetFromXmlHeader( hdrbuf, strlen(hdrbuf));
		if (encoding != papuga_Binary) return encoding;
	}

	for (int cidx=0; ci != ce; ++ci,++cidx)
	{
		if (*ci == 0x00)
		{
			++zcnt;
			++mcnt[ cidx % 4];
		}
		else if (max_zcnt < zcnt)
		{
			max_zcnt = zcnt;
			zcnt = 0;
		}
	}
	if (max_zcnt == 0)
	{
		return papuga_UTF8;
	}
	if (mcnt[0] > mcnt[1] && mcnt[1] > mcnt[2] && mcnt[2] > mcnt[3] && mcnt[3] == 0)
	{
		return papuga_UTF32BE;
	}
	if (mcnt[0] == 0 && mcnt[0] < mcnt[1] && mcnt[1] < mcnt[2] && mcnt[2] < mcnt[3])
	{
		return papuga_UTF32LE;
	}
	if (mcnt[0] > mcnt[1] && mcnt[2] > mcnt[3] && mcnt[1] == 0 && mcnt[3] == 0)
	{
		return papuga_UTF16BE;
	}
	if (mcnt[0] == 0 && mcnt[2] == 0 && mcnt[0] < mcnt[1] && mcnt[2] < mcnt[3])
	{
		return papuga_UTF16LE;
	}
	return papuga_Binary;
}

papuga_RequestElementType papuga_RequestParser_next( papuga_RequestParser* self, papuga_ValueVariant* value)
{
	papuga_RequestParserHeader* header = (papuga_RequestParserHeader*)self;
	return header->next( self, value);
}


