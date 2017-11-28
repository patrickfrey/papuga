/*
 * Copyright (c) 2017 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/// \brief Structures and functions for scanning papuga JSON documents for further processing
/// \file document_json.cpp
#include "papuga/document.h"
#include "papuga/valueVariant.h"
#include "cjson/cJSON.h"
#include "textwolf/xmlscanner.hpp"
#include <cstdlib>
#include <string>
#include <vector>

struct TextwolfItem
{
	typedef textwolf::XMLScannerBase::ElementType Type;
	Type type;
	const char* value;

	TextwolfItem( const TextwolfItem& o)
		:type(o.type),value(o.value){}
	explicit TextwolfItem( const Type& type_, const char* value_=0)
		:type(type_),value(value_){}
};

static void papuga_destroy_DocumentParser_json( papuga_DocumentParser* self);
static papuga_DocumentElementType papuga_DocumentParser_json_next( papuga_DocumentParser* self, papuga_ValueVariant* value);
static std::vector<TextwolfItem> parseJsonTree( const char* content, papuga_ErrorCode* errcode, int* errpos);

struct papuga_DocumentParser
{
	papuga_DocumentParserHeader header;
	std::string elembuf;
	const char* content;
	size_t contentsize;
	std::vector<TextwolfItem> items;
	std::vector<TextwolfItem>::const_iterator iter;

	papuga_DocumentParser( const char* content_, size_t contentsize_)
		:elembuf(),content(content_),contentsize(contentsize_)
	{
		header.type = papuga_DocumentType_JSON;
		header.errcode = papuga_Ok;
		header.errpos = -1;
		header.libname = "cjson";
		header.destroy = &papuga_destroy_DocumentParser_json;
		header.next = &papuga_DocumentParser_json_next;
		items = parseJsonTree( content_, &header.errcode, &header.errpos);
		iter = items.begin();
	}

	papuga_DocumentElementType getNext( papuga_ValueVariant* value)
	{
		typedef textwolf::XMLScannerBase tx;
		if (iter == items.end())
		{
			papuga_init_ValueVariant( value);
			return papuga_DocumentElementType_None;
		}
		else
		{
			if (iter->value)
			{
				papuga_init_ValueVariant_charp( value, iter->value);
			}
			else
			{
				papuga_init_ValueVariant( value);
			}
			switch (iter->type)
			{
				case tx::None:
				case tx::Exit:
				case tx::ErrorOccurred:
				case tx::HeaderStart:
				case tx::HeaderAttribName:
				case tx::HeaderAttribValue:
				case tx::HeaderEnd:
				case tx::DocAttribEnd:
				case tx::DocAttribValue:
					return papuga_DocumentElementType_None;
				case tx::TagAttribName:
					return papuga_DocumentElementType_AttributeName;
				case tx::TagAttribValue:
					return papuga_DocumentElementType_AttributeValue;
				case tx::OpenTag:
					return papuga_DocumentElementType_Open;
				case tx::CloseTag:
				case tx::CloseTagIm:
					return papuga_DocumentElementType_Close;
				case tx::Content:
					return papuga_DocumentElementType_Value;
			}
		}
		header.errcode = papuga_LogicError;
		return papuga_DocumentElementType_None;
	}
};

static void getTextwolfValue( std::vector<TextwolfItem>& tiar, cJSON const* nd, const char* value)
{
	typedef textwolf::XMLScannerBase TX;
	if (nd->string)
	{
		if (nd->string[0] == '-')
		{
			tiar.push_back( TextwolfItem( TX::TagAttribName, nd->string+1));
			tiar.push_back( TextwolfItem( TX::TagAttribValue, value));
		}
		else if (nd->string[0] == '#' && std::strcmp( nd->string, "#text") == 0)
		{
			tiar.push_back( TextwolfItem( TX::Content, value));
		}
		else
		{
			tiar.push_back( TextwolfItem( TX::OpenTag, nd->string));
			tiar.push_back( TextwolfItem( TX::Content, value));
			tiar.push_back( TextwolfItem( TX::CloseTag, nd->string));
		}
	}
	else
	{
		tiar.push_back( TextwolfItem( TX::Content, value));
	}
}

static bool getTextwolfItems_( std::vector<TextwolfItem>& itemar, cJSON const* nd, papuga_ErrorCode* errcode)
{
	typedef textwolf::XMLScannerBase TX;
	switch (nd->type & 0x7F)
	{
		case cJSON_False:
			getTextwolfValue( itemar, nd, "false");
			break;
		case cJSON_True:
			getTextwolfValue( itemar, nd, "true");
			break;
		case cJSON_NULL:
			if (nd->string && nd->string[0] != '-' && nd->string[0] != '#')
			{
				itemar.push_back( TextwolfItem( TX::OpenTag, nd->string));
				itemar.push_back( TextwolfItem( TX::CloseTagIm));
			}
			break;
		case cJSON_String:
			getTextwolfValue( itemar, nd, nd->valuestring);
			break;
		case cJSON_Number:
			if (!nd->valuestring)
			{
				*errcode = papuga_ValueUndefined;
				return false;
			}
			getTextwolfValue( itemar, nd, nd->valuestring);
			break;
		case cJSON_Array:
		{
			cJSON const* chnd = nd->child;
			if (nd->string)
			{
				for (;chnd; chnd = chnd->next)
				{
					itemar.push_back( TextwolfItem( TX::OpenTag, nd->string));
					if (!getTextwolfItems_( itemar, chnd, errcode)) return false;
					itemar.push_back( TextwolfItem( TX::CloseTag, nd->string));
				}
			}
			else
			{
				unsigned int idx=0;
				char idxstr[ 64];
				for (;chnd; chnd = chnd->next)
				{
					snprintf( idxstr, sizeof( idxstr), "%u", idx);
					itemar.push_back( TextwolfItem( TX::OpenTag, idxstr));
					if (!getTextwolfItems_( itemar, chnd, errcode)) return false;
					itemar.push_back( TextwolfItem( TX::CloseTag, idxstr));
				}
			}
			break;
		}
		case cJSON_Object:
		{
			cJSON const* chnd = nd->child;
			if (nd->string)
			{
				itemar.push_back( TextwolfItem( TX::OpenTag, nd->string));
				for (;chnd; chnd = chnd->next)
				{
					if (!getTextwolfItems_( itemar, chnd, errcode)) return false;
				}
				itemar.push_back( TextwolfItem( TX::CloseTag, nd->string));
			}
			else
			{
				for (;chnd; chnd = chnd->next)
				{
					if (!getTextwolfItems_( itemar, chnd, errcode)) return false;
				}
			}
			break;
		}
		default:
			*errcode = papuga_LogicError;
			return false;
	}
	return true;
}

static std::vector<TextwolfItem> getTextwolfItems( const cJSON* tree, papuga_ErrorCode* errcode)
{
	std::vector<TextwolfItem> rt;
	getTextwolfItems_( rt, tree, errcode);
	return rt;
}

static std::vector<TextwolfItem> parseJsonTree( const char* content, papuga_ErrorCode* errcode, int* errpos)
{
	cJSON_Context ctx;
	cJSON* tree = cJSON_Parse( content, &ctx);
	if (!tree)
	{
		if (!ctx.position)
		{
			*errcode = papuga_NoMemError;
		}
		else
		{
			*errcode = papuga_SyntaxError;
			*errpos = ctx.position;
		}
	}
	try
	{
		return getTextwolfItems( tree, errcode);
	}
	catch (const std::bad_alloc&)
	{
		*errcode = papuga_NoMemError;
	}
	catch (...)
	{
		*errcode = papuga_UncaughtException;
	}
	return std::vector<TextwolfItem>();
}

extern "C" papuga_DocumentParser* papuga_create_DocumentParser_json( papuga_StringEncoding encoding, const char* content, size_t size)
{
	papuga_DocumentParser* rt = (papuga_DocumentParser*)std::calloc( 1, sizeof(papuga_DocumentParser));
	if (!rt) return NULL;
	new (&rt) papuga_DocumentParser( content, size);
	if (rt->header.errcode != papuga_Ok)
	{
		papuga_destroy_DocumentParser_json( rt);
		return NULL;
	}
	return rt;
}

static void papuga_destroy_DocumentParser_json( papuga_DocumentParser* self)
{
	self->~papuga_DocumentParser();
	std::free( self);
}

static papuga_DocumentElementType papuga_DocumentParser_json_next( papuga_DocumentParser* self, papuga_ValueVariant* value)
{
	return self->getNext( value);
}



