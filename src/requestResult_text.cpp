/*
 * Copyright (c) 2017 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/* \brief Structure to build and map the Result of an XML/JSON request
 * @file requestResult_text.cpp
 */
#include "papuga/requestResult.h"
#include "papuga/serialization.hpp"
#include "papuga/serialization.h"
#include "papuga/valueVariant.hpp"
#include "papuga/valueVariant.h"
#include "papuga/allocator.h"
#include "papuga/callResult.h"
#include "papuga/constants.h"
#include "requestResult_utils.hpp"
#include <string>
#include <cstring>
#include <cstdlib>
#include <iostream>
#include <sstream>

#define INDENT_INCREASE_STR "  "
#define INDENT_INCREASE_LEN 2

static bool SerializationIter_totext( std::ostream& out, const std::string& indent_, papuga_SerializationIter* seritr, int structid, const papuga_StructInterfaceDescription* structs, papuga_ErrorCode& errcode);

static bool ValueVariant_totext( std::ostream& out, const std::string& indent, const papuga_ValueVariant& value, const papuga_StructInterfaceDescription* structs, papuga_ErrorCode& errcode)
{
	bool rt = true;
	if (papuga_ValueVariant_isatomic(&value))
	{
		std::string elem;
		rt &= papuga::ValueVariant_append_string( elem, value, errcode);
		out << elem;
	}
	else if (value.valuetype == papuga_TypeSerialization)
	{
		papuga_SerializationIter subitr;
		papuga_init_SerializationIter( &subitr, value.value.serialization);
		while (rt && !papuga_SerializationIter_eof( &subitr))
		{
			out << indent;
			rt &= SerializationIter_totext( out, indent+INDENT_INCREASE_STR, &subitr, value.value.serialization->structid, structs, errcode);
		}
	}
	else if (value.valuetype == papuga_TypeIterator)
	{
		int itercnt = 0;
		papuga_Allocator allocator;
		papuga_CallResult result;
		int result_mem[ 1024];
		char error_mem[ 128];
		papuga_Iterator* iterator = value.value.iterator;

		papuga_init_Allocator( &allocator, result_mem, sizeof(result_mem));
		papuga_init_CallResult( &result, &allocator, true, error_mem, sizeof(error_mem));
		while (itercnt++ < PAPUGA_MAX_ITERATOR_EXPANSION_LENGTH && rt && iterator->getNext( iterator->data, &result))
		{
			out << indent;
			if (result.nofvalues > 1)
			{
				int ri = 0, re = result.nofvalues;
				for (; ri != re; ++ri)
				{
					if (ri) out << " ";
					rt &= ValueVariant_totext( out, indent+INDENT_INCREASE_STR, result.valuear[ri], structs, errcode);
				}
			}
			else if (result.nofvalues == 1)
			{
				rt &= ValueVariant_totext( out, indent+INDENT_INCREASE_STR, result.valuear[0], structs, errcode);
			}
			papuga_destroy_CallResult( &result);
			papuga_init_Allocator( &allocator, result_mem, sizeof(result_mem));
			papuga_init_CallResult( &result, &allocator, true, error_mem, sizeof(error_mem));
		}
		if (papuga_CallResult_hasError( &result))
		{
			errcode = papuga_IteratorFailed;
			rt = false;
		}
		papuga_destroy_CallResult( &result);
	}
	else
	{
		errcode = papuga_TypeError;
		rt = false;
	}
	if (!rt && errcode == papuga_Ok)
	{
		errcode = papuga_NoMemError;
	}
	return rt;
}

static bool SerializationIter_totext( std::ostream& out, const std::string& indent, papuga_SerializationIter* seritr, int structid, const papuga_StructInterfaceDescription* structs, papuga_ErrorCode& errcode)
{
	bool rt = true;
	int elementcnt = 0;

	for (; rt; papuga_SerializationIter_skip(seritr))
	{
		switch(papuga_SerializationIter_tag(seritr))
		{
			case papuga_TagClose:
			{
				return true;
			}
			case papuga_TagValue:
			{
				const papuga_ValueVariant* value = papuga_SerializationIter_value(seritr);
				if (structid)
				{
					out << indent << structs[ structid-1].members[ elementcnt].name << ":";
				}
				rt &= ValueVariant_totext( out, indent+INDENT_INCREASE_STR, *value, structs, errcode);
				++elementcnt;
				break;
			}
			case papuga_TagOpen:
			{
				const papuga_ValueVariant* value = papuga_SerializationIter_value(seritr);
				if (structid)
				{
					out << indent << structs[ structid-1].members[ elementcnt].name << ":";
				}
				int substructid = papuga_ValueVariant_defined( value) ? papuga_ValueVariant_toint( value, &errcode) : 0;
				if (errcode != papuga_Ok) return false;
				rt &= SerializationIter_totext( out, indent+INDENT_INCREASE_STR, seritr, substructid, structs, errcode);
				if (papuga_SerializationIter_tag(seritr) != papuga_TagClose)
				{
					errcode = papuga_SyntaxError;
					return false;
				}
				++elementcnt;
				break;
			}
			case papuga_TagName:
			{
				if (structid)
				{
					errcode = papuga_SyntaxError;
					return false;
				}
				char namebuf[ 256];
				std::size_t namelen = 0;
				if (!papuga_ValueVariant_tostring_enc( papuga_SerializationIter_value(seritr), papuga_UTF8, namebuf, sizeof(namebuf)-1, &namelen, &errcode))
				{
					return false;
				}
				namebuf[ namelen] = 0;
				out << indent << namebuf << ":";
			}
		}
	}
	if (!rt && errcode == papuga_Ok)
	{
		errcode = papuga_NoMemError;
	}
	return rt;
}

static void* RequestResult_totext( const papuga_RequestResult* self, papuga_StringEncoding enc, size_t* len, papuga_ErrorCode* err)
{
	try
	{
		std::ostringstream out;
		std::string indent = "\n";
		const char* rootelem = self->name;

		if (rootelem)
		{
			out << rootelem << ":";
			indent.append( INDENT_INCREASE_STR);
		}
		papuga_RequestResultNode const* nd = self->nodes;
		for (; nd; nd = nd->next)
		{
			out << "\n" << indent << nd->name << ":";
			if (!ValueVariant_totext( out, indent, nd->value, self->structdefs, *err)) return NULL;
		}
		void* rt = papuga::encodeRequestResultString( out.str(), enc, len, err);
		if (rt) papuga_Allocator_add_free_mem( self->allocator, rt);
		return rt;
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

extern "C" void* papuga_RequestResult_totext( const papuga_RequestResult* self, papuga_StringEncoding enc, size_t* len, papuga_ErrorCode* err)
{
	return RequestResult_totext( self, enc, len, err);
}


