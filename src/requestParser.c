/*
 * Copyright (c) 2017 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/* \brief Structures and functions for scanning papuga XML and JSON documents for further processing
 * \file requestParser.c
 */
#include "papuga/requestParser.h"
#include "papuga/request.h"
#include "papuga/allocator.h"
#include <string.h>
#include <stdio.h>

#undef PAPUGA_LOWLEVEL_DEBUG

papuga_ContentType papuga_contentTypeFromName( const char* name)
{
	if (NULL!=strstr( name, "xml") || NULL!=strstr( name, "XML"))
	{
		return papuga_ContentType_XML;
	}
	else if (NULL!=strstr( name, "json") || NULL!=strstr( name, "JSON"))
	{
		return papuga_ContentType_JSON;
	}
	else
	{
		return papuga_ContentType_Unknown;
	}
}

const char* papuga_ContentType_name( papuga_ContentType type)
{
	static const char* ar[] = {"unknown","XML","JSON"};
	return ar[ (int)type];
}

const char* papuga_ContentType_mime( papuga_ContentType type)
{
	static const char* ar[] = {"application/octet-stream","application/xml","application/json"};
	return ar[ (int)type];
}

const char* papuga_requestElementTypeName( papuga_RequestElementType tp)
{
	static const char* ar[] = {"None","Open","Close","AttibuteName","AttibuteValue","Value",0};
	return ar[ (int)tp];
}

void papuga_destroy_RequestParser( papuga_RequestParser* self)
{
	((papuga_RequestParserHeader*)self)->destroy( self);
}

papuga_ErrorCode papuga_RequestParser_last_error( const papuga_RequestParser* self)
{
	return ((papuga_RequestParserHeader*)self)->errcode;
}

int papuga_RequestParser_get_position( const papuga_RequestParser* self, char* locbuf, size_t locbufsize)
{
	return ((papuga_RequestParserHeader*)self)->position( self, locbuf, locbufsize);
}

static bool parse_xml_header( char* hdrbuf, size_t hdrbufsize, const char* src, size_t srcsize)
{
	int state = 0;
	size_t si = 0;
	size_t hi = 0;
	for (; si < srcsize && hi < hdrbufsize; ++si)
	{
		if (src[si] == '\0') continue;
		if ((unsigned char)src[si] >= 128) return false;
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
	const unsigned char* bom = (const unsigned char*)src;
	*BOM_size = 0;
	if (srcsize < 4) return papuga_Binary;
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
	size_t xmlhdrsize = srcsize > 1024 ? 1024 : srcsize;

	(void)detectBOM( src, srcsize, &BOM_size);
	src += BOM_size;
	srcsize -= BOM_size;
	if (parse_xml_header( hdrbuf, sizeof(hdrbuf), src, xmlhdrsize)) return papuga_ContentType_XML;
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
	if (0==strcmp( encbuf, "utf8")) return papuga_UTF8;
	if (0==strcmp( encbuf, "utf16")) return papuga_UTF16BE;
	if (0==strcmp( encbuf, "utf32")) return papuga_UTF32BE;
	if (0==strcmp( encbuf, "utf16be")) return papuga_UTF16BE;
	if (0==strcmp( encbuf, "utf32be")) return papuga_UTF32BE;
	if (0==strcmp( encbuf, "utf16le")) return papuga_UTF16LE;
	if (0==strcmp( encbuf, "utf32le")) return papuga_UTF32LE;
	return papuga_Binary;
}

papuga_StringEncoding papuga_guess_StringEncoding( const char* src, size_t srcsize)
{
	char const* ci = src;
	size_t chunksize = 1024;
	const char* ce = (chunksize > srcsize) ? (ci + srcsize) : (ci + chunksize);
	unsigned int zcnt = 0;
	unsigned int max_zcnt = 0;
	unsigned int mcnt[ 4] = {0,0,0,0};
	int cidx = 0;
	size_t BOM_size;
	char hdrbuf[ 256];

	papuga_StringEncoding encoding = detectBOM( src, srcsize, &BOM_size);
	if (encoding != papuga_Binary) return encoding;

	if (parse_xml_header( hdrbuf, sizeof(hdrbuf), src, srcsize) && hdrbuf[0])
	{
		encoding = detectCharsetFromXmlHeader( hdrbuf, strlen(hdrbuf));
		if (encoding != papuga_Binary) return encoding;
	}

	for (cidx=0; ci != ce; ++ci,++cidx)
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
	if (mcnt[0] >= mcnt[1] && mcnt[1] >= mcnt[2] && mcnt[2] >= mcnt[3] && mcnt[3] == 0)
	{
		return papuga_UTF32BE;
	}
	if (mcnt[0] == 0 && mcnt[0] <= mcnt[1] && mcnt[1] <= mcnt[2] && mcnt[2] <= mcnt[3])
	{
		return papuga_UTF32LE;
	}
	if (mcnt[0] >= mcnt[1] && mcnt[2] >= mcnt[3] && mcnt[1] == 0 && mcnt[3] == 0)
	{
		return papuga_UTF16BE;
	}
	if (mcnt[0] == 0 && mcnt[2] == 0 && mcnt[0] <= mcnt[1] && mcnt[2] <= mcnt[3])
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

bool papuga_RequestParser_feed_request( papuga_RequestParser* parser, papuga_Request* request, papuga_ErrorCode* errcode)
{
	papuga_ValueVariant value;
	bool done = false;

	while (!done)
	{
		papuga_RequestElementType elemtype = papuga_RequestParser_next( parser, &value);
#ifdef PAPUGA_LOWLEVEL_DEBUG
		{
			char buf[ 1024];
			size_t len;
			papuga_ErrorCode err2 = papuga_Ok;
			papuga_ValueVariant_tostring_enc( value, papuga_UTF8, buf, sizeof(buf), &len, &err2);
			if (err2 != papuga_Ok) snprintf( buf, sizeof(buf), "...");
			fprintf( stderr, "parser feed %s '%s'\n", papuga_requestElementTypeName( elemtype), buf);
		}
#endif
		switch (elemtype)
		{
			case papuga_RequestElementType_None:
				*errcode = papuga_RequestParser_last_error( parser);
				if (*errcode != papuga_Ok) return false;
				done = true;
				break;
			case papuga_RequestElementType_Open:
				if (!papuga_Request_feed_open_tag( request, &value))
				{
					*errcode = papuga_Request_last_error( request);
					return false;
				}
				break;
			case papuga_RequestElementType_Close:
				if (!papuga_Request_feed_close_tag( request))
				{
					*errcode = papuga_Request_last_error( request);
					return false;
				}
				break;
			case papuga_RequestElementType_AttributeName:
				if (!papuga_Request_feed_attribute_name( request, &value))
				{
					*errcode = papuga_Request_last_error( request);
					return false;
				}
				break;
			case papuga_RequestElementType_AttributeValue:
				if (!papuga_Request_feed_attribute_value( request, &value))
				{
					*errcode = papuga_Request_last_error( request);
					return false;
				}
				break;
			case papuga_RequestElementType_Value:
				if (!papuga_Request_feed_content_value( request, &value))
				{
					*errcode = papuga_Request_last_error( request);
					return false;
				}
				break;
		}
	}
	if (!papuga_Request_feed_close_tag( request))
	{
		*errcode = papuga_Request_last_error( request);
		return false;
	}
	return papuga_Request_done( request);
}

papuga_RequestParser* papuga_create_RequestParser( papuga_Allocator* allocator, papuga_ContentType doctype, papuga_StringEncoding encoding, const char* content, size_t size, papuga_ErrorCode* errcode)
{
	switch (doctype)
	{
		case papuga_ContentType_XML:  return papuga_create_RequestParser_xml( allocator, encoding, content, size, errcode);
		case papuga_ContentType_JSON: return papuga_create_RequestParser_json( allocator, encoding, content, size, errcode);
		case papuga_ContentType_Unknown: *errcode = papuga_ValueUndefined; return NULL;
		default: *errcode = papuga_NotImplemented; return NULL;
	}
}



