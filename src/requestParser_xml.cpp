/*
 * Copyright (c) 2017 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/// \brief Structures and functions for scanning papuga XML request bodies for further processing
/// \file requestParser_xml.cpp
#include "papuga/requestParser.h"
#include "papuga/valueVariant.h"
#include "papuga/valueVariant.hpp"
#include "papuga/allocator.h"
#include "textwolf/xmlscanner.hpp"
#include "textwolf/charset.hpp"
#include "requestParser_utils.h"
#include <cstdlib>
#include <string>

using namespace papuga;

static void papuga_destroy_RequestParser_xml( papuga_RequestParser* self);
static papuga_RequestElementType papuga_RequestParser_xml_next( papuga_RequestParser* self, papuga_ValueVariant* value);
static int papuga_RequestParser_xml_position( const papuga_RequestParser* self, char* locbuf, size_t locbufsize);

namespace {
struct RequestParser_xml
{
	papuga_RequestParserHeader header;
	papuga_Allocator* allocator;
	std::string elembuf;
	std::string content;

	typedef textwolf::XMLScanner<
			textwolf::SrcIterator,
			textwolf::charset::UTF8,
			textwolf::charset::UTF8,
			std::string
		> XMLScanner;

	textwolf::SrcIterator srciter;
	XMLScanner scanner;
	typename XMLScanner::iterator itr;
	typename XMLScanner::iterator end;
	jmp_buf eom;
	int taglevel;
	int tagcnt;

	explicit RequestParser_xml( papuga_Allocator* allocator_, const std::string& content_)
		:allocator(allocator_),elembuf(),content(content_),taglevel(0),tagcnt(0)
	{
		header.type = papuga_ContentType_XML;
		header.errcode = papuga_Ok;
		header.errpos = -1;
		header.libname = "textwolf";
		header.destroy = &papuga_destroy_RequestParser_xml;
		header.next = &papuga_RequestParser_xml_next;
		header.position = &papuga_RequestParser_xml_position;

		srciter.putInput( content.c_str(), content.size(), &eom);
		scanner.setSource( srciter);
		itr = scanner.begin( false);
		end = scanner.end();
	}

	papuga_RequestElementType getNext( papuga_ValueVariant* value)
	{
		typedef textwolf::XMLScannerBase tx;
		if (setjmp(eom) != 0)
		{
			if (taglevel != 0 || tagcnt == 0)
			{
				header.errcode = papuga_UnexpectedEof;
				header.errpos = content.size();
			}
			return papuga_RequestElementType_None;
		}
		for (;;)
		{
			++itr;
			switch (itr->type())
			{
				case tx::None:
					header.errpos = scanner.getTokenPosition();
					header.errcode = papuga_ValueUndefined;
					return papuga_RequestElementType_None;
				case tx::Exit:
					return papuga_RequestElementType_None;
				case tx::ErrorOccurred:
					header.errpos = scanner.getTokenPosition();
					header.errcode = papuga_SyntaxError;
					return papuga_RequestElementType_None;
	
				case tx::HeaderStart:
				case tx::HeaderAttribName:
				case tx::HeaderAttribValue:
				case tx::HeaderEnd:
				case tx::DocAttribValue:
				case tx::DocAttribEnd:
					continue;
				case tx::TagAttribName:
					papuga_init_ValueVariant_string( value, itr->content(), itr->size());
					return papuga_RequestElementType_AttributeName;
				case tx::TagAttribValue:
					papuga_init_ValueVariant_string( value, itr->content(), itr->size());
					return papuga_RequestElementType_AttributeValue;
				case tx::OpenTag:
					++taglevel;++tagcnt;
					papuga_init_ValueVariant_string( value, itr->content(), itr->size());
					return papuga_RequestElementType_Open;
				case tx::CloseTag:
				case tx::CloseTagIm:
					--taglevel;
					papuga_init_ValueVariant( value);
					return papuga_RequestElementType_Close;
				case tx::Content:
					papuga_init_ValueVariant_string( value, itr->content(), itr->size());
					return papuga_RequestElementType_Value;
			}
		}
	}
};
}//anonymous namespace

struct papuga_RequestParser
{
	RequestParser_xml impl;
};

extern "C" papuga_RequestParser* papuga_create_RequestParser_xml( papuga_Allocator* allocator, papuga_StringEncoding encoding, const char* content, size_t size, papuga_ErrorCode* errcode)
{
	papuga_RequestParser* rt = (papuga_RequestParser*)papuga_Allocator_alloc( allocator, sizeof(papuga_RequestParser), 0/*default alignment*/);
	if (!rt) return NULL;
	try
	{
		std::string contentUTF8;
		if (encoding == papuga_UTF8)
		{
			contentUTF8.append( content, size);
		}
		else
		{
			papuga_ValueVariant input;
			size_t unitsize = size / papuga_StringEncoding_unit_size( encoding);
			papuga_init_ValueVariant_string_enc( &input, encoding, content, unitsize);
			contentUTF8 = ValueVariant_tostring( input, *errcode);
		}
		new (&rt->impl) RequestParser_xml( allocator, contentUTF8);
	}
	catch (const std::bad_alloc&)
	{
		*errcode = papuga_NoMemError;
		return NULL;
	}
	return rt;
}

static void papuga_destroy_RequestParser_xml( papuga_RequestParser* self)
{
	self->impl.~RequestParser_xml();
}

static papuga_RequestElementType papuga_RequestParser_xml_next( papuga_RequestParser* self, papuga_ValueVariant* value)
{
	return self->impl.getNext( value);
}

static int papuga_RequestParser_xml_position( const papuga_RequestParser* self, char* locbuf, size_t locbufsize)
{
	fillErrorLocation( locbuf, locbufsize, self->impl.content.c_str(), self->impl.header.errpos, "!$!");
	return self->impl.header.errpos;
}

