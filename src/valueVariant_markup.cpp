/*
 * Copyright (c) 2017 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/// \brief Expand a request result as XML
/// \file valueVariant_markup.cpp
#include "valueVariant_markup_json.hpp"
#include "valueVariant_markup_text.hpp"
#include "valueVariant_markup_xml.hpp"
#include "valueVariant_markup_html.hpp"
#include <limits>
#include <iostream>

using namespace papuga;
using namespace papuga::markup;

#define CATCH_ERROR_MAP_RETURN( ERRCODE, RETVAL)\
	catch (const std::bad_alloc&)\
	{\
		ERRCODE = papuga_NoMemError;\
		return RETVAL;\
	}\
	catch (const ErrorException& err)\
	{\
		ERRCODE = err.errcode();\
		return RETVAL;\
	}\
	catch (...)\
	{\
		ERRCODE = papuga_UncaughtException;\
		return RETVAL;\
	}


extern "C" void* papuga_ValueVariant_toxml(
		const papuga_ValueVariant* self,
		papuga_Allocator* allocator,
		const papuga_StructInterfaceDescription* structdefs,
		papuga_StringEncoding enc,
		bool beautified,
		const char* rootname,
		const char* elemname,
		size_t* len,
		papuga_ErrorCode* errcode)
{
	try
	{
		int maxDepth = std::numeric_limits<int>::max();
		OutputContextXML xml( structdefs, maxDepth, enc, beautified);
		std::string output = xml.build( rootname, elemname, *self);
		return OutputContextBase::encodeRequestResultString( output, allocator, enc, len);
	}
	CATCH_ERROR_MAP_RETURN( *errcode, NULL)
}

extern "C" void* papuga_ValueVariant_tohtml5(
		const papuga_ValueVariant* self,
		papuga_Allocator* allocator,
		const papuga_StructInterfaceDescription* structdefs,
		papuga_StringEncoding enc,
		bool beautified,
		const char* rootname,
		const char* elemname,
		const char* head,
		const char* href_base,
		size_t* len,
		papuga_ErrorCode* errcode)
{
	try
	{
		int maxDepth = std::numeric_limits<int>::max();
		OutputContextHTML html( structdefs, maxDepth, enc, beautified, head, href_base);
		std::string output = html.build( rootname, elemname, *self);
		return OutputContextBase::encodeRequestResultString( output, allocator, enc, len);
	}
	CATCH_ERROR_MAP_RETURN( *errcode, NULL)
}

extern "C" void* papuga_ValueVariant_totext(
		const papuga_ValueVariant* self,
		papuga_Allocator* allocator,
		const papuga_StructInterfaceDescription* structdefs,
		papuga_StringEncoding enc,
		bool beautified,
		const char* rootname,
		const char* elemname,
		size_t* len,
		papuga_ErrorCode* errcode)
{
	try
	{
		int maxDepth = std::numeric_limits<int>::max();
		OutputContextTEXT text( structdefs, maxDepth, enc, beautified);
		std::string output = text.build( rootname, elemname, *self);
		return OutputContextBase::encodeRequestResultString( output, allocator, enc, len);
	}
	CATCH_ERROR_MAP_RETURN( *errcode, NULL)
}

extern "C" void* papuga_ValueVariant_tojson(
		const papuga_ValueVariant* self,
		papuga_Allocator* allocator,
		const papuga_StructInterfaceDescription* structdefs,
		papuga_StringEncoding enc,
		bool beautified,
		const char* rootname,
		const char* elemname,
		size_t* len,
		papuga_ErrorCode* errcode)
{
	try
	{
		int maxDepth = std::numeric_limits<int>::max();
		OutputContextJSON json( structdefs, maxDepth, enc, beautified);
		std::string output = json.build( rootname, elemname, *self);
		return OutputContextBase::encodeRequestResultString( output, allocator, enc, len);
	}
	CATCH_ERROR_MAP_RETURN( *errcode, NULL)
}


std::string papuga::ValueVariant_todump( const papuga_ValueVariant& value, const papuga_StructInterfaceDescription* structdefs, bool deterministic, papuga_ErrorCode& errcode)
{
	try
	{
		std::string dump;
		if (!papuga_ValueVariant_defined( &value))
		{
			dump.append( "\tNULL\n");
		}
		else if (papuga_ValueVariant_isatomic( &value))
		{
			dump.append( "\t");
			dump.append( papuga::ValueVariant_tostring( value, errcode));
		}
		else if (value.valuetype == papuga_TypeSerialization)
		{
			if (deterministic)
			{
				dump.append( papuga::Serialization_tostring_deterministic( *value.value.serialization, false/*linemode*/, PAPUGA_MAX_RECURSION_DEPTH, errcode));
			}
			else
			{
				dump.append( papuga::Serialization_tostring( *value.value.serialization, false/*linemode*/, PAPUGA_MAX_RECURSION_DEPTH, errcode));
			}
		}
		else
		{
			dump.append( "\t<");
			dump.append( papuga_Type_name( value.valuetype));
			dump.append( ">\n");
		}
		return dump;
	}
	catch (const std::bad_alloc&)
	{
		errcode = papuga_NoMemError;
		return std::string();
	}
}

extern "C" char* papuga_ValueVariant_todump(
		const papuga_ValueVariant* self,
		papuga_Allocator* allocator,
		const papuga_StructInterfaceDescription* structdefs,
		bool deterministic,
		size_t* len)
{
	papuga_ErrorCode errcode = papuga_Ok;
	try
	{
		std::string res = papuga::ValueVariant_todump( *self, structdefs, deterministic, errcode);
		if (errcode) return NULL;
		char* rt = (char*)std::malloc( res.size() +1);
		if (!rt) return NULL;
		if (allocator)
		{
			if (!papuga_Allocator_add_free_mem( allocator, rt))
			{
				std::free( rt);
				return NULL;
			}
		}
		std::memcpy( rt, res.c_str(), res.size() +1);
		*len = res.size();
		return rt;
	}
	catch (const std::bad_alloc&)
	{
		return NULL;
	}
}



