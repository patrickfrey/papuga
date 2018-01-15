/*
 * Copyright (c) 2017 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/// \brief Expand a request result as XML
/// \file requestResult_xml.cpp
#include "papuga/requestResult.h"
#include "papuga/serialization.h"
#include "papuga/valueVariant.hpp"
#include "papuga/valueVariant.h"
#include "papuga/allocator.h"
#include "papuga/typedefs.h"
#include "papuga/constants.h"
#include "papuga/callResult.h"
#include "papuga/interfaceDescription.h"
#include "papuga/stack.h"
#include "requestResult_utils.hpp"
#include <string>
#include <cstring>

static bool SerializationIter_toxml( std::string& out, papuga_SerializationIter* seritr, const char* name, int structid, int elementcnt, const papuga_StructInterfaceDescription* structs, papuga_ErrorCode& errcode);

static bool append_tag_open( std::string& out, const char* name)
{
	try
	{
		out.push_back( '<');
		out.append( name);
		out.push_back( '>');
	}
	catch (...)
	{
		return false;
	}
	return true;
}

static bool append_tag_close( std::string& out, const char* name)
{
	try
	{
		out.append( "</");
		out.append( name);
		out.push_back( '>');
	}
	catch (...)
	{
		return false;
	}
	return true;
}

static bool append_tag_open_close_imm( std::string& out, const char* name)
{
	try
	{
		out.push_back( '<');
		out.append( name);
		out.append( "/>");
	}
	catch (...)
	{
		return false;
	}
	return true;
}

static bool ValueVariant_toxml( std::string& out, const char* name, const papuga_ValueVariant& value, const papuga_StructInterfaceDescription* structs, papuga_ErrorCode& errcode)
{
	static const char* tupletags[ papuga_MAX_NOF_RETURNS] = {"1","2","3","4","5","6","7","8"};
	bool rt = true;
	if (papuga_ValueVariant_isatomic(&value))
	{
		rt &= append_tag_open( out, name);
		rt &= papuga::ValueVariant_append_string( out, value, errcode);
		rt &= append_tag_close( out, name);
	}
	else if (value.valuetype == papuga_TypeSerialization)
	{
		papuga_SerializationIter subitr;
		papuga_init_SerializationIter( &subitr, value.value.serialization);
		bool isdict = value.value.serialization->structid || papuga_SerializationIter_tag( &subitr) == papuga_TagName;
		int elementcnt = 0;
		while (rt && !papuga_SerializationIter_eof( &subitr))
		{
			if (isdict)
			{
				rt &= append_tag_open( out, name);
				rt &= SerializationIter_toxml( out, &subitr, NULL/*name*/, value.value.serialization->structid, elementcnt++, structs, errcode);
				rt &= append_tag_close( out, name);
			}
			else
			{
				rt &= SerializationIter_toxml( out, &subitr, name, value.value.serialization->structid, elementcnt++, structs, errcode);
			}
		}
	}
	else if (value.valuetype == papuga_TypeIterator)
	{
		int itercnt = 0;
		papuga_CallResult result;
		int result_mem[ 1024];
		char error_mem[ 128];
		papuga_Iterator* iterator = value.value.iterator;

		papuga_init_CallResult( &result, result_mem, sizeof(result_mem), error_mem, sizeof(error_mem));
		while (itercnt++ < PAPUGA_MAX_ITERATOR_EXPANSION_LENGTH && rt && iterator->getNext( iterator->data, &result))
		{
			if (result.nofvalues > 1)
			{
				rt &= append_tag_open( out, name);
				int ri = 0, re = result.nofvalues;
				for (; ri != re; ++ri)
				{
					rt &= ValueVariant_toxml( out, tupletags[ri], result.valuear[ri], structs, errcode);
				}
				rt &= append_tag_close( out, name);
			}
			else if (result.nofvalues == 1)
			{
				rt &= ValueVariant_toxml( out, name, result.valuear[0], structs, errcode);
			}
			else
			{
				append_tag_open_close_imm( out, name);
			}
			papuga_destroy_CallResult( &result);
			if (!rt)
			{
				errcode = papuga_NoMemError;
				break;
			}
			papuga_init_CallResult( &result, result_mem, sizeof(result_mem), error_mem, sizeof(error_mem));
		}
		if (papuga_CallResult_hasError( &result))
		{
			errcode = papuga_IteratorFailed;
			papuga_destroy_CallResult( &result);
			return false;
		}
		papuga_destroy_CallResult( &result);
	}
	else if (!papuga_ValueVariant_defined( &value))
	{}
	else
	{
		errcode = papuga_TypeError;
		return false;
	}
	if (!rt && errcode == papuga_Ok)
	{
		errcode = papuga_NoMemError;
	}
	return rt;
}

struct SerializationIterStackElem
{
	int parent_structid;
	int parent_elementcnt;
	char name[ 128];
};

class SerializationIterStack
{
public:
	SerializationIterStack()
	{
		papuga_init_Stack( &m_namestk, sizeof(SerializationIterStackElem), 128, m_namestk_mem, sizeof(m_namestk_mem));
	}
	~SerializationIterStack()
	{
		papuga_destroy_Stack( &m_namestk);
	}

	bool push( int structid, int elementcnt, const char* name, papuga_ErrorCode& errcode)
	{
		SerializationIterStackElem* stkelem = (SerializationIterStackElem*)papuga_Stack_push( &m_namestk);
		if (!stkelem)
		{
			errcode = papuga_NoMemError;
			return false;
		}
		stkelem->parent_structid = structid;
		stkelem->parent_elementcnt = elementcnt;
		if (name)
		{
			size_t namelen = std::snprintf( stkelem->name, sizeof(stkelem->name), "%s", name);
			if (sizeof(stkelem->name) >= namelen)
			{
				errcode = papuga_BufferOverflowError;
				return false;
			}
			stkelem->name[ namelen] = 0;
		}
		else
		{
			stkelem->name[ 0] = 0;
		}
		return true;
	}

	bool pop( int& structid, int& elementcnt, char const*& name)
	{
		SerializationIterStackElem* stkelem = (SerializationIterStackElem*)papuga_Stack_pop( &m_namestk);
		if (!stkelem) return false;
		structid = stkelem->parent_structid;
		elementcnt = stkelem->parent_elementcnt;
		name = stkelem->name[ 0] ? stkelem->name : NULL;
		return true;
	}

	bool empty() const
	{
		return papuga_Stack_empty( &m_namestk);
	}

private:
	papuga_Stack m_namestk;
	int m_namestk_mem[ 1024];
};

static bool SerializationIter_toxml( std::string& out, papuga_SerializationIter* seritr, const char* name, int structid, int elementcnt, const papuga_StructInterfaceDescription* structs, papuga_ErrorCode& errcode)
{
	bool rt = true;
	SerializationIterStack namestk;
	char namebuf[ 128];
	char const* tagname;

	for (; rt; papuga_SerializationIter_skip(seritr)) switch( papuga_SerializationIter_tag(seritr))
	{
		case papuga_TagClose:
		{
			if (name)
			{
				errcode = papuga_SyntaxError;
				return false;
			}
			if (!namestk.pop( structid, elementcnt, tagname))
			{
				goto EXIT;
			}
			if (tagname)
			{
				rt &= append_tag_close( out, tagname);
			}
			if (namestk.empty())
			{
				papuga_SerializationIter_skip(seritr);
				goto EXIT;
			}
			break;
		}
		case papuga_TagValue:
		{
			const papuga_ValueVariant* value = papuga_SerializationIter_value(seritr);
			if (!name)
			{
				if (structid)
				{
					name = structs[ structid-1].members[ elementcnt].name;
				}
				else
				{
					errcode = papuga_SyntaxError;
					return false;
				}
			}
			++elementcnt;
			rt &= ValueVariant_toxml( out, name, *value, structs, errcode);
			name = NULL;
			if (namestk.empty())
			{
				papuga_SerializationIter_skip(seritr);
				goto EXIT;
			}
			break;
		}
		case papuga_TagOpen:
		{
			const papuga_ValueVariant* value = papuga_SerializationIter_value(seritr);
			if (!name)
			{
				if (structid)
				{
					name = structs[ structid-1].members[ elementcnt].name;
				}
				else
				{
					errcode = papuga_SyntaxError;
					return false;
				}
			}
			++elementcnt;
			if (!namestk.push( structid, elementcnt, name, errcode))
			{
				return false;
			}
			structid = 0;
			elementcnt = 0;
			if (papuga_ValueVariant_defined( value))
			{
				structid = papuga_ValueVariant_toint( value, &errcode);
				if (errcode != papuga_Ok) return false;
			}
			rt &= append_tag_open( out, name);
			name = NULL;
			break;
		}
		case papuga_TagName:
		{
			size_t namelen;
			if (name)
			{
				errcode = papuga_SyntaxError;
				return false;
			}
			if (!papuga_ValueVariant_tostring_enc( papuga_SerializationIter_value(seritr), papuga_UTF8, namebuf, sizeof(namebuf)-1, &namelen, &errcode))
			{
				return false;
			}
			namebuf[ namelen] = 0;
			name = namebuf;
		}
	}
	if (name)
	{
		errcode = papuga_SyntaxError;
		return false;
	}
EXIT:
	if (!rt && errcode == papuga_Ok)
	{
		errcode = papuga_NoMemError;
	}
	return rt;
}

extern "C" void* papuga_RequestResult_toxml( const papuga_RequestResult* self, papuga_StringEncoding enc, size_t* len, papuga_ErrorCode* err)
{
	try
	{
		char hdrbuf[ 128];
		std::string out;
		const char* rootelem = self->name;

		if (rootelem)
		{
			std::snprintf( hdrbuf, sizeof(hdrbuf), "<?xml version=\"1.0\" encoding=\"%s\" standalone=\"yes\"?>\n<%s>", papuga_StringEncoding_name( enc), rootelem);
		}
		else
		{
			std::snprintf( hdrbuf, sizeof(hdrbuf), "<?xml version=\"1.0\" encoding=\"%s\" standalone=\"yes\"?>\n", papuga_StringEncoding_name( enc));
		}
		out.append( hdrbuf);

		papuga_RequestResultNode const* nd = self->nodes;
		for (; nd; nd = nd->next)
		{
			if (!ValueVariant_toxml( out, nd->name, nd->value, self->structdefs, *err)) return NULL;
		}
		if (rootelem)
		{
			out.append( "</");
			out.append( rootelem);
			out.append( ">\n");
		}
		else
		{
			out.append( "\n");
		}
		return papuga::encodeRequestResultString( out, enc, len, err);
	}
	catch (const std::bad_alloc&)
	{
		*err = papuga_NoMemError;
		return NULL;
	}
	catch (...)
	{
		*err = papuga_UncaughtException;
		return NULL;
	}
}

