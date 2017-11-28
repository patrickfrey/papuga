/*
 * Copyright (c) 2017 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/// \brief Structures and functions for scanning papuga XML documents for further processing
/// \file document_xml.cpp
#include "papuga/document.h"
#include "papuga/valueVariant.h"
#include "textwolf/xmlscanner.hpp"
#include "textwolf/charset.hpp"
#include <cstdlib>
#include <string>

static void papuga_destroy_DocumentParser_xml( papuga_DocumentParser* self);
static papuga_DocumentElementType papuga_DocumentParser_xml_next( papuga_DocumentParser* self, papuga_ValueVariant* value);

struct papuga_DocumentParser
{
	papuga_DocumentParserHeader header;
	std::string elembuf;
	const char* content;
	size_t contentsize;

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

	papuga_DocumentParser( const char* content_, size_t contentsize_)
		:elembuf(),content(content_),contentsize(contentsize_)
	{
		header.type = papuga_DocumentType_XML;
		header.errcode = papuga_Ok;
		header.errpos = -1;
		header.libname = "textwolf";
		header.destroy = &papuga_destroy_DocumentParser_xml;
		header.next = &papuga_DocumentParser_xml_next;

		srciter.putInput( content_, contentsize_, &eom);
		scanner.setSource( srciter);
		itr = scanner.begin( false);
		end = scanner.end();
	}

	papuga_DocumentElementType getNext( papuga_ValueVariant* value)
	{
		typedef textwolf::XMLScannerBase tx;
		if (setjmp(eom) != 0)
		{
			header.errcode = papuga_UnexpectedEof;
			header.errpos = contentsize;
			return papuga_DocumentElementType_None;
		}
		for (;;)
		{
			++itr;
			switch (itr->type())
			{
				case tx::None:
					header.errpos = scanner.getTokenPosition();
					header.errcode = papuga_ValueUndefined;
					return papuga_DocumentElementType_None;
				case tx::Exit:
					return papuga_DocumentElementType_None;
				case tx::ErrorOccurred:
					header.errpos = scanner.getTokenPosition();
					header.errcode = papuga_SyntaxError;
					return papuga_DocumentElementType_None;
	
				case tx::HeaderStart:
				case tx::HeaderAttribName:
				case tx::HeaderAttribValue:
				case tx::HeaderEnd:
				case tx::DocAttribValue:
				case tx::DocAttribEnd:
					continue;
				case tx::TagAttribName:
					papuga_init_ValueVariant_string( value, itr->content(), itr->size());
					return papuga_DocumentElementType_AttributeName;
				case tx::TagAttribValue:
					papuga_init_ValueVariant_string( value, itr->content(), itr->size());
					return papuga_DocumentElementType_AttributeValue;
				case tx::OpenTag:
					papuga_init_ValueVariant_string( value, itr->content(), itr->size());
					return papuga_DocumentElementType_Open;
				case tx::CloseTag:
				case tx::CloseTagIm:
					papuga_init_ValueVariant( value);
					return papuga_DocumentElementType_Close;
				case tx::Content:
					papuga_init_ValueVariant_string( value, itr->content(), itr->size());
					return papuga_DocumentElementType_Value;
			}
		}
	}
};

extern "C" papuga_DocumentParser* papuga_create_DocumentParser_xml( papuga_StringEncoding encoding, const char* content, size_t size)
{
	papuga_DocumentParser* rt = (papuga_DocumentParser*)std::calloc( 1, sizeof(papuga_DocumentParser));
	if (!rt) return NULL;
	new (&rt) papuga_DocumentParser( content, size);
	return rt;
}

static void papuga_destroy_DocumentParser_xml( papuga_DocumentParser* self)
{
	self->~papuga_DocumentParser();
	std::free( self);
}

static papuga_DocumentElementType papuga_DocumentParser_xml_next( papuga_DocumentParser* self, papuga_ValueVariant* value)
{
	return self->getNext( value);
}


