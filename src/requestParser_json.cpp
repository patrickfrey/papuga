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
#include "papuga/allocator.h"
#include "papuga/constants.h"
#include "papuga/serialization.h"
#include "cjson/cJSON.h"
#include "textwolf/xmlscanner.hpp"
#include "requestParser_utils.h"
#include <cstdlib>
#include <string>
#include <vector>
#include <sstream>
#include <iostream>

using namespace papuga;

#undef PAPUGA_LOWLEVEL_DEBUG

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
static std::vector<TextwolfItem> getTextwolfItems( papuga_Allocator* allocator, const cJSON* tree, papuga_ErrorCode* errcode);

#ifdef PAPUGA_LOWLEVEL_DEBUG
static void printTextwolfItemList( std::ostream& out, const char* title, const std::vector<TextwolfItem>& tiar)
{
	out << title << ":" << std::endl;
	std::vector<TextwolfItem>::const_iterator ti = tiar.begin(), te = tiar.end();
	for (; ti != te; ++ti)
	{
		out << textwolf::XMLScannerBase::getElementTypeName( ti->type) << " '" << ti->value << "'" << std::endl;
	}
}
#endif


namespace {
struct JsonTreeRef
{
	JsonTreeRef( cJSON* ptr_=0)	:m_ptr(ptr_){}
	~JsonTreeRef()	{if (m_ptr) cJSON_Delete( m_ptr);}

	void operator=( cJSON* ptr_)	{if (m_ptr) cJSON_Delete( m_ptr); m_ptr = ptr_;}
	operator cJSON*()		{return m_ptr;}
	cJSON* operator ->() const	{return m_ptr;}

private:
	cJSON* m_ptr;
};

static const papuga_RequestElementType textWolfElementType2requestElementType[]
{
	papuga_RequestElementType_None,			///< None
	papuga_RequestElementType_None,			///< ErrorOccurred
	papuga_RequestElementType_None,			///< HeaderStart
	papuga_RequestElementType_None,			///< HeaderAttribName
	papuga_RequestElementType_None,			///< HeaderAttribValue
	papuga_RequestElementType_None,			///< HeaderEnd
	papuga_RequestElementType_None,			///< DocAttribValue
	papuga_RequestElementType_None,			///< DocAttribEnd
	papuga_RequestElementType_AttributeName, 	///< TagAttribName
	papuga_RequestElementType_AttributeValue,	///< TagAttribValue
	papuga_RequestElementType_Open,			///< OpenTag
	papuga_RequestElementType_Close,		///< CloseTag
	papuga_RequestElementType_Close,		///< CloseTagIm
	papuga_RequestElementType_Value,		///< Content
	papuga_RequestElementType_None			///< Exit
};

struct RequestParser_json
{
	papuga_RequestParserHeader header;
	papuga_Allocator* allocator;
	std::string elembuf;
	std::string content;
	JsonTreeRef tree;
	std::vector<TextwolfItem> items;
	std::vector<TextwolfItem>::const_iterator iter;

	RequestParser_json( papuga_Allocator* allocator_, const std::string& content_)
		:allocator(allocator_),elembuf(),content(content_),tree(),items(),iter()
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
			if (ctx.position < 0)
			{
				header.errcode = papuga_NoMemError;
			}
			else
			{
				header.errcode = papuga_SyntaxError;
				header.errpos = ctx.position;
			}
		}
		else
		{
			items = getTextwolfItems( allocator, tree, &header.errcode);
#ifdef PAPUGA_LOWLEVEL_DEBUG
			printTextwolfItemList( std::cerr, "JSON items parsed", items);
#endif
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
							break;
						case tx::TagAttribValue:
							out << "\"" << start->value << "\"";
							break;
						case tx::OpenTag:
							out << "\"" << start->value << "\"";
							if (!stk.empty())
							{
								if (stk.back() > 0) out << ", ";
								++stk.back();
							}
							stk.push_back( 0);
							out << "{";
							break;
						case tx::CloseTag:
						case tx::CloseTagIm:
							if (!stk.empty())
							{
								stk.pop_back();
							}
							out << "}";
							break;
						case tx::Content:
							if (!stk.empty())
							{
								if (stk.back() > 0) out << ", ";
								++stk.back();
							}
							out << "\"" << start->value << "\"";
							break;
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

			return textWolfElementType2requestElementType[ tp];
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

static bool getTextwolfItems_( std::vector<TextwolfItem>& itemar, papuga_Allocator* allocator, cJSON const* nd, int depth, papuga_ErrorCode* errcode)
{
	if (depth > PAPUGA_MAX_RECURSION_DEPTH)
	{
		*errcode = papuga_MaxRecursionDepthReached;
		return false;
	}
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
					if (!getTextwolfItems_( itemar, allocator, chnd, depth+1, errcode)) return false;
					itemar.push_back( TextwolfItem( TX::CloseTag, nd->string));
				}
			}
			else
			{
				unsigned int idx=1;
				char idxstr[ 64];
				for (;chnd; chnd = chnd->next,++idx)
				{
					std::size_t idxstrlen = std::snprintf( idxstr, sizeof( idxstr), "%u", idx);
					char* idxstr_copy = (char*)papuga_Allocator_alloc( allocator, idxstrlen+1, 1);
					if (!idxstr_copy)
					{
						*errcode = papuga_NoMemError;
						return false;
					}
					std::memcpy( idxstr_copy, idxstr, idxstrlen+1);
					itemar.push_back( TextwolfItem( TX::OpenTag, idxstr_copy));
					if (!getTextwolfItems_( itemar, allocator, chnd, depth+1, errcode)) return false;
					itemar.push_back( TextwolfItem( TX::CloseTag, idxstr_copy));
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
					if (!getTextwolfItems_( itemar, allocator, chnd, depth+1, errcode)) return false;
				}
				itemar.push_back( TextwolfItem( TX::CloseTag, nd->string));
			}
			else
			{
				for (;chnd; chnd = chnd->next)
				{
					if (!getTextwolfItems_( itemar, allocator, chnd, depth+1, errcode)) return false;
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

static std::vector<TextwolfItem> getTextwolfItems( papuga_Allocator* allocator, const cJSON* tree, papuga_ErrorCode* errcode)
{
	std::vector<TextwolfItem> rt;
	try
	{
		if (!getTextwolfItems_( rt, allocator, tree, 0, errcode)) return std::vector<TextwolfItem>();
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

static inline bool serializeJsonName( papuga_Serialization* serialization, cJSON const* nd, bool deep)
{
	if (nd->string && nd->string[0] != '-' && nd->string[0] != '#')
	{
		const char* name = deep ? papuga_Allocator_copy_charp( serialization->allocator, nd->string) : nd->string;
		return papuga_Serialization_pushName_charp( serialization, name);
	}
	return true;
}

static bool getJsonSerialization( papuga_Serialization* serialization, cJSON const* nd, int depth, bool deep, papuga_ErrorCode* errcode)
{
	bool rt = true;
	if (depth > PAPUGA_MAX_RECURSION_DEPTH)
	{
		*errcode = papuga_MaxRecursionDepthReached;
		return false;
	}
	switch (nd->type & 0x7F)
	{
		case cJSON_False:
			rt &= serializeJsonName( serialization, nd, deep);
			rt &= papuga_Serialization_pushValue_bool( serialization, false);
			break;
		case cJSON_True:
			rt &= serializeJsonName( serialization, nd, deep);
			rt &= papuga_Serialization_pushValue_bool( serialization, true);
			break;
		case cJSON_NULL:
			rt &= serializeJsonName( serialization, nd, deep);
			rt &= papuga_Serialization_pushValue_void( serialization);
			break;
		case cJSON_String:
		case cJSON_Number:
			rt &= serializeJsonName( serialization, nd, deep);
			if (!nd->valuestring)
			{
				*errcode = papuga_ValueUndefined;
				return false;
			}
			else
			{
				const char* valstr = deep ? papuga_Allocator_copy_charp( serialization->allocator, nd->valuestring) : nd->valuestring;
				rt &= papuga_Serialization_pushValue_charp( serialization, valstr);
				break;
			}
		case cJSON_Array:
		case cJSON_Object:
		{
			cJSON const* chnd = nd->child;
			if (nd->string)
			{
				const char* name = deep ? papuga_Allocator_copy_charp( serialization->allocator, nd->string) : nd->string;
				rt &= papuga_Serialization_pushName_charp( serialization, name);
			}
			bool isroot = papuga_Serialization_empty( serialization);
			if (!isroot)
			{
				rt &= papuga_Serialization_pushOpen( serialization);
				for (;chnd; chnd = chnd->next)
				{
					rt &= getJsonSerialization( serialization, chnd, depth+1, deep, errcode);
				}
				rt &= papuga_Serialization_pushClose( serialization);
			}
			else
			{
				for (;chnd; chnd = chnd->next)
				{
					rt &= getJsonSerialization( serialization, chnd, depth+1, deep, errcode);
				}
			}
			break;
		}
		default:
			*errcode = papuga_LogicError;
			return false;
	}
	return rt;
}

extern "C" papuga_RequestParser* papuga_create_RequestParser_json( papuga_Allocator* allocator, papuga_StringEncoding encoding, const char* content, size_t size, papuga_ErrorCode* errcode)
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
			papuga_init_ValueVariant_string_enc( &input, encoding, content, size);
			contentUTF8 = ValueVariant_tostring( input, *errcode);
		}
		new (&rt->impl) RequestParser_json( allocator, contentUTF8);
	}
	catch (const std::bad_alloc&)
	{
		*errcode = papuga_NoMemError;
		return NULL;
	}
	return rt;
}

static void papuga_destroy_RequestParser_json( papuga_RequestParser* self)
{
	self->impl.~RequestParser_json();
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

extern "C" bool papuga_init_ValueVariant_json( papuga_ValueVariant* self, papuga_Allocator* allocator, papuga_StringEncoding encoding, const char* contentstr, size_t contentlen, papuga_ErrorCode* errcode)
{
	papuga_init_ValueVariant( self);
	papuga_Serialization* ser = (papuga_Serialization*)papuga_Allocator_alloc_Serialization( allocator);
	if (!ser)
	{
		*errcode = papuga_NoMemError;
		return false;
	}
	try
	{
		JsonTreeRef tree;
		std::string contentUTF8;
		if (encoding == papuga_UTF8)
		{
			contentUTF8.append( contentstr, contentlen);
		}
		else
		{
			papuga_ValueVariant input;
			papuga_init_ValueVariant_string_enc( &input, encoding, contentstr, contentlen);
			contentUTF8 = ValueVariant_tostring( input, *errcode);
		}
		cJSON_Context ctx;
		tree = cJSON_Parse( contentUTF8.c_str(), &ctx);
		if (!tree)
		{
			*errcode = (ctx.position < 0) ? papuga_NoMemError : papuga_SyntaxError;
			return false;
		}
		else if (!tree->child)
		{
			*errcode = papuga_SyntaxError;
			return false;
		}
		bool rt = getJsonSerialization( ser, tree, 0, true/*deep*/, errcode);
		if (rt)
		{
			papuga_init_ValueVariant_serialization( self, ser);
		}
		return rt;
	}
	catch (const std::bad_alloc&)
	{
		*errcode = papuga_NoMemError;
		return false;
	}
}



