/*
 * Copyright (c) 2017 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/// \brief Structures and functions for scanning papuga JSON request bodies for further processing
/// \file requestParser_json.cpp
#include "papuga/requestParser.h"
#include "papuga/valueVariant.h"
#include "papuga/valueVariant.hpp"
#include "cjson/cJSON.h"
#include "textwolf/xmlscanner.hpp"
#include "requestParser_utils.h"
#include <cstdlib>
#include <string>
#include <vector>
#include <sstream>
#include <iostream>

using namespace papuga;

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

static void papuga_destroy_RequestParser_json( papuga_RequestParser* self);
static papuga_RequestElementType papuga_RequestParser_json_next( papuga_RequestParser* self, papuga_ValueVariant* value);
static int papuga_RequestParser_json_position( const papuga_RequestParser* self, char* locbuf, size_t locbufsize);
static std::vector<TextwolfItem> getTextwolfItems( const cJSON* tree, papuga_ErrorCode* errcode);

namespace {
struct JsonTreeRef
{
	JsonTreeRef( cJSON* ptr_=0)	:m_ptr(ptr_){}
	~JsonTreeRef()	{if (m_ptr) cJSON_Delete( m_ptr);}

	void operator=( cJSON* ptr_)	{if (m_ptr) cJSON_Delete( m_ptr); m_ptr = ptr_;}
	operator cJSON*()		{return m_ptr;}

private:
	cJSON* m_ptr;
};

struct RequestParser_json
{
	papuga_RequestParserHeader header;
	std::string elembuf;
	std::string content;
	JsonTreeRef tree;
	std::vector<TextwolfItem> items;
	std::vector<TextwolfItem>::const_iterator iter;

	RequestParser_json( const std::string& content_)
		:elembuf(),content(content_)
	{
		header.type = papuga_ContentType_JSON;
		header.errcode = papuga_Ok;
		header.errpos = -1;
		header.libname = "cjson";
		header.destroy = &papuga_destroy_RequestParser_json;
		header.next = &papuga_RequestParser_json_next;
		header.position = &papuga_RequestParser_json_position;

		cJSON_Context ctx;
		tree = cJSON_Parse( content.c_str(), &ctx);
		if (!tree)
		{
			if (!ctx.position)
			{
				header.errcode = papuga_NoMemError;
			}
			else
			{
				header.errcode = papuga_SyntaxError;
				header.errpos = ctx.position-1;
			}
		}
		else
		{
			items = getTextwolfItems( tree, &header.errcode);
		}
		iter = items.begin();
	}

	void getLocationInfo( char* locbuf, std::size_t locbufsize) const
	{
		if (locbufsize == 0) return;
		if (header.errpos >= 0)
		{
			fillErrorLocation( locbuf, locbufsize, content.c_str(), header.errpos, "<!>");
		}
		else if (iter != items.end())
		{
			try
			{
				typedef textwolf::XMLScannerBase tx;

				std::ostringstream out;
				std::vector<int> stk;
				int cnt = 0;
				stk.push_back( 0);

				std::vector<TextwolfItem>::const_iterator start = iter;
				for (cnt=7; start != items.begin() && cnt; --cnt,--start){}
				for (; start != items.end() && cnt < 15; ++start,++cnt)
				{
					if (start == iter)
					{
						out << "<!>";
					}
					switch (start->type)
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
							break;
						case tx::TagAttribName:
							if (!stk.empty())
							{
								if (stk.back() > 0) out << ", ";
								++stk.back();
							}
							out << "-" << start->value << "=";
						case tx::TagAttribValue:
							out << "\"" << start->value << "\"";
						case tx::OpenTag:
							if (!stk.empty())
							{
								if (stk.back() > 0) out << ", ";
								++stk.back();
							}
							stk.push_back( 0);
							out << "{";
						case tx::CloseTag:
						case tx::CloseTagIm:
							if (!stk.empty())
							{
								stk.pop_back();
							}
							out << "}";
						case tx::Content:
							if (!stk.empty())
							{
								if (stk.back() > 0) out << ", ";
								++stk.back();
							}
							out << "\"" << start->value << "\"";
					}
				}
				std::string outstr( out.str());
				if (outstr.size() >= locbufsize)
				{
					std::memcpy( locbuf, outstr.c_str(), locbufsize);
					locbuf[ locbufsize-1] = 0;
				}
				else
				{
					std::memcpy( locbuf, outstr.c_str(), outstr.size());
					locbuf[ outstr.size()] = 0;
				}
			}
			catch (const std::bad_alloc&)
			{
				locbuf[ 0] = 0;
			}
		}
		else
		{
			locbuf[ 0] = 0;
		}
	}

	papuga_RequestElementType getNext( papuga_ValueVariant* value)
	{
		typedef textwolf::XMLScannerBase tx;
		if (iter == items.end())
		{
			papuga_init_ValueVariant( value);
			return papuga_RequestElementType_None;
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
			tx::ElementType tp = iter->type;
			++iter;
			switch (tp)
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
					return papuga_RequestElementType_None;
				case tx::TagAttribName:
					return papuga_RequestElementType_AttributeName;
				case tx::TagAttribValue:
					return papuga_RequestElementType_AttributeValue;
				case tx::OpenTag:
					return papuga_RequestElementType_Open;
				case tx::CloseTag:
				case tx::CloseTagIm:
					return papuga_RequestElementType_Close;
				case tx::Content:
					return papuga_RequestElementType_Value;
			}
			header.errcode = papuga_LogicError;
			return papuga_RequestElementType_None;
		}
	}
};
}//anonymous namespace

struct papuga_RequestParser
{
	RequestParser_json impl;
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
	try
	{
		if (!getTextwolfItems_( rt, tree, errcode)) return std::vector<TextwolfItem>();
	}
	catch (const std::bad_alloc&)
	{
		*errcode = papuga_NoMemError;
	}
	catch (...)
	{
		*errcode = papuga_UncaughtException;
	}
	return rt;
}

extern "C" papuga_RequestParser* papuga_create_RequestParser_json( papuga_StringEncoding encoding, const char* content, size_t size, papuga_ErrorCode* errcode)
{
	papuga_RequestParser* rt = (papuga_RequestParser*)std::calloc( 1, sizeof(papuga_RequestParser));
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
		new (&rt->impl) RequestParser_json( contentUTF8);
	}
	catch (const std::bad_alloc&)
	{
		*errcode = papuga_NoMemError;
		if (rt) std::free( rt);
		return NULL;
	}
	return rt;
}

static void papuga_destroy_RequestParser_json( papuga_RequestParser* self)
{
	self->impl.~RequestParser_json();
	std::free( self);
}

static papuga_RequestElementType papuga_RequestParser_json_next( papuga_RequestParser* self, papuga_ValueVariant* value)
{
	if (self->impl.header.errcode != papuga_Ok)
	{
		return papuga_RequestElementType_None;
	}
	else
	{
		return self->impl.getNext( value);
	}
}

static int papuga_RequestParser_json_position( const papuga_RequestParser* self, char* locbuf, size_t locbufsize)
{
	self->impl.getLocationInfo( locbuf, locbufsize);
	return self->impl.header.errpos;
}



