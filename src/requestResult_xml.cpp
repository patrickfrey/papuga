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

enum StyleType {StyleHTML,StyleXML};

// Forward declaration:
static bool Serialization_toxml( StyleType styleType, std::string& out, papuga_Serialization* ser, const char* name, const papuga_StructInterfaceDescription* structs, int maxDepth, papuga_ErrorCode& errcode);

static void append_tag_open( StyleType styleType, std::string& out, const char* name)
{
	switch (styleType)
	{
		case StyleXML:
			out.push_back( '<');
			out.append( name);
			out.push_back( '>');
			break;
		case StyleHTML:
			out.append( "<span class=\"title\">");
			out.append( name);
			out.append( "</span>");
			out.append( "<div class=\"");
			out.append( name);
			out.append( "\">");
			break;
	}
}

static void append_tag_close( StyleType styleType, std::string& out, const char* name)
{
	switch (styleType)
	{
		case StyleXML:
			out.append( "</");
			out.append( name);
			out.push_back( '>');
			break;
		
		case StyleHTML:
			out.append( "</div>");
			break;
	}
}

static void append_tag_open_node( StyleType styleType, std::string& out, const char* name)
{
	switch (styleType)
	{
		case StyleXML:
			out.push_back( '<');
			out.append( name);
			out.push_back( '>');
			break;
		case StyleHTML:
			break;
	}
}

static void append_tag_open_close_imm( StyleType styleType, std::string& out, const char* name)
{
	switch (styleType)
	{
		case StyleXML:
			out.push_back( '<');
			out.append( name);
			out.append( "/>");
			break;
		case StyleHTML:
			out.append( "<div class=\"");
			out.append( name);
			out.append( "\"/>");
			break;
	}
}

static bool append_key_value( StyleType styleType, std::string& out, const char* name, const papuga_ValueVariant& value, papuga_ErrorCode& errcode)
{
	switch (styleType)
	{
		case StyleXML:
			out.push_back( '<');
			out.append( name);
			out.push_back( '>');
			if (!papuga::ValueVariant_append_string( out, value, errcode)) return false;
			out.append( "</");
			out.append( name);
			out.push_back( '>');
			break;
		case StyleHTML:
			out.append( "<span class=\"name\">");
			out.append( name);
			out.append( "</span>");
			out.append( "<span class=\"value\">");
			if (!papuga::ValueVariant_append_string( out, value, errcode)) return false;
			out.append( "</span>");
			break;
	}
	return true;
}

static bool ValueVariant_toxml( StyleType styleType, std::string& out, const char* name, const papuga_ValueVariant& value, const papuga_StructInterfaceDescription* structs, int maxDepth, papuga_ErrorCode& errcode)
{
	static const char* tupletags[ papuga_MAX_NOF_RETURNS] = {"1","2","3","4","5","6","7","8"};
	bool rt = true;
	if (--maxDepth == 0)
	{
		errcode = papuga_MaxRecursionDepthReached;
		return false;
	}
	if (papuga_ValueVariant_isatomic(&value))
	{
		if (!append_key_value( styleType, out, name, value, errcode)) return false;
	}
	else if (value.valuetype == papuga_TypeSerialization)
	{
		rt &= Serialization_toxml( styleType, out, value.value.serialization, name, structs, maxDepth, errcode);
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
		try
		{
			papuga_init_CallResult( &result, &allocator, false, error_mem, sizeof(error_mem));
			
			while (itercnt++ < PAPUGA_MAX_ITERATOR_EXPANSION_LENGTH && rt && iterator->getNext( iterator->data, &result))
			{
				if (result.nofvalues > 1)
				{
					append_tag_open( styleType, out, name);
					int ri = 0, re = result.nofvalues;
					for (; ri != re; ++ri)
					{
						rt &= ValueVariant_toxml( styleType, out, tupletags[ri], result.valuear[ri], structs, maxDepth, errcode);
					}
					append_tag_close( styleType, out, name);
				}
				else if (result.nofvalues == 1)
				{
					rt &= ValueVariant_toxml( styleType, out, name, result.valuear[0], structs, maxDepth, errcode);
				}
				else
				{
					append_tag_open_close_imm( styleType, out, name);
				}
				papuga_destroy_CallResult( &result);
				if (!rt)
				{
					errcode = papuga_NoMemError;
					break;
				}
				papuga_destroy_Allocator( &allocator);
				papuga_init_Allocator( &allocator, result_mem, sizeof(result_mem));
				papuga_init_CallResult( &result, &allocator, false, error_mem, sizeof(error_mem));
			}
		}
		catch (const std::bad_alloc&)
		{
			papuga_destroy_Allocator( &allocator);
			errcode = papuga_NoMemError;
			rt = false;
		}
		if (papuga_CallResult_hasError( &result))
		{
			errcode = papuga_IteratorFailed;
			rt = false;
		}
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

// Forward declarations:
static bool SerializationIter_toxml_array( StyleType styleType, std::string& out, papuga_SerializationIter* seritr, const char* name, const papuga_StructInterfaceDescription* structs, int maxDepth, papuga_ErrorCode& errcode);
static bool SerializationIter_toxml_struct( StyleType styleType, std::string& out, papuga_SerializationIter* seritr, int structid, const papuga_StructInterfaceDescription* structs, int maxDepth, papuga_ErrorCode& errcode);
static bool SerializationIter_toxml_dict( StyleType styleType, std::string& out, papuga_SerializationIter* seritr, const papuga_StructInterfaceDescription* structs, int maxDepth, papuga_ErrorCode& errcode);

struct StructType
{
	enum Id {Dict,Array,Struct,Empty};
	Id id;
	int structid;
};

static bool getStructType( StructType& st, const papuga_SerializationIter* seritr, papuga_ErrorCode& errcode)
{
	papuga_SerializationIter si;
	papuga_init_SerializationIter_copy( &si, seritr);
	const papuga_ValueVariant* value = papuga_SerializationIter_value(&si);
	st.structid = papuga_ValueVariant_defined( value) ? papuga_ValueVariant_toint( value, &errcode) : 0;
	if (st.structid)
	{
		st.id = StructType::Struct;
	}
	else
	{
		papuga_SerializationIter_skip( &si);
		switch (papuga_SerializationIter_tag( &si))
		{
			case papuga_TagName: st.id = StructType::Dict; break;
			case papuga_TagValue: st.id = StructType::Array; break;
			case papuga_TagOpen: st.id = StructType::Array; break;
			case papuga_TagClose: st.id = StructType::Empty; break;
		}
	}
	return true;
}

static bool ValueVariant_toxml_node( StyleType styleType, std::string& out, const char* name, const papuga_ValueVariant& value, const papuga_StructInterfaceDescription* structs, int maxDepth, papuga_ErrorCode& errcode)
{
	if (value.valuetype == papuga_TypeSerialization)
	{
		StructType::Id stid;
		papuga_SerializationIter seritr;
		papuga_init_SerializationIter( &seritr, value.value.serialization);
		switch (papuga_SerializationIter_tag( &seritr))
		{
			case papuga_TagName: stid = StructType::Dict; break;
			case papuga_TagValue: stid = StructType::Array; break;
			case papuga_TagOpen: stid = StructType::Array; break;
			case papuga_TagClose: stid = StructType::Empty; break;
		}
		if (stid != StructType::Array)
		{
			return ValueVariant_toxml( styleType, out, NULL/*name*/, value, structs, maxDepth, errcode);
		}
	}
	return ValueVariant_toxml( styleType, out, name, value, structs, maxDepth, errcode);
}

static inline bool SerializationIter_toxml_named_elem( StyleType styleType, std::string& out, papuga_SerializationIter* seritr, const char* name, const papuga_StructInterfaceDescription* structs, int maxDepth, papuga_ErrorCode& errcode)
{
	bool rt = true;
	switch( papuga_SerializationIter_tag(seritr))
	{
		case papuga_TagClose:
		{
			errcode = papuga_UnexpectedEof;
			///... can only get here from SerializationIter_toxml_dict_elem( StyleType styleType, std::string& out, papuga_SerializationIter* seritr, const papuga_StructInterfaceDescription* structs, int maxDepth, papuga_ErrorCode& errcode)
			///	other cases are caught before
			return false;
		}
		case papuga_TagValue:
		{
			const papuga_ValueVariant* value = papuga_SerializationIter_value(seritr);
			rt &= ValueVariant_toxml( styleType, out, name, *value, structs, maxDepth, errcode);
			break;
		}
		case papuga_TagName:
		{
			errcode = papuga_SyntaxError;
			return false;
		}
		case papuga_TagOpen:
		{
			StructType st;
			if (!getStructType( st, seritr, errcode)) return false;
			papuga_SerializationIter_skip(seritr);

			switch (st.id)
			{
				case StructType::Empty:
					break;
				case StructType::Array:
					rt &= SerializationIter_toxml_array( styleType, out, seritr, name, structs, maxDepth, errcode);
					break;
				case StructType::Dict:
					if (name)
					{
						append_tag_open( styleType, out, name);
						rt &= SerializationIter_toxml_dict( styleType, out, seritr, structs, maxDepth, errcode);
						append_tag_close( styleType, out, name);
					}
					else
					{
						rt &= SerializationIter_toxml_dict( styleType, out, seritr, structs, maxDepth, errcode);
					}
					break;
				case StructType::Struct:
					if (name)
					{
						append_tag_open( styleType, out, name);
						rt &= SerializationIter_toxml_struct( styleType, out, seritr, st.structid, structs, maxDepth, errcode);
						append_tag_close( styleType, out, name);
					}
					else
					{
						rt &= SerializationIter_toxml_struct( styleType, out, seritr, st.structid, structs, maxDepth, errcode);
					}
					break;
			}
			if (rt && papuga_SerializationIter_eof(seritr))
			{
				errcode = papuga_UnexpectedEof;
				return false;
			}
			break;
		}
	}
	return rt;
}

static inline bool SerializationIter_toxml_dict_elem( StyleType styleType, std::string& out, papuga_SerializationIter* seritr, const papuga_StructInterfaceDescription* structs, int maxDepth, papuga_ErrorCode& errcode)
{
	const char* name = NULL;
	char namebuf[ 128];
	std::size_t namelen;

	if (papuga_SerializationIter_tag(seritr) == papuga_TagName)
	{
		const papuga_ValueVariant* value = papuga_SerializationIter_value(seritr);
		name = (const char*)papuga_ValueVariant_tostring_enc( value, papuga_UTF8, namebuf, sizeof(namebuf), &namelen, &errcode);
		if (!name) return false;
		namebuf[ namelen] = 0;
	}
	else
	{
		errcode = papuga_SyntaxError;
		return false;
	}
	papuga_SerializationIter_skip(seritr);
	return SerializationIter_toxml_named_elem( styleType, out, seritr, name, structs, maxDepth, errcode);
}

static bool SerializationIter_toxml_array( StyleType styleType, std::string& out, papuga_SerializationIter* seritr, const char* name, const papuga_StructInterfaceDescription* structs, int maxDepth, papuga_ErrorCode& errcode)
{
	bool rt = true;

	if (--maxDepth == 0)
	{
		errcode = papuga_MaxRecursionDepthReached;
		return false;
	}
	for (; rt && papuga_SerializationIter_tag(seritr) != papuga_TagClose; papuga_SerializationIter_skip(seritr))
	{
		rt &= SerializationIter_toxml_named_elem( styleType, out, seritr, name, structs, maxDepth, errcode);
	}
	return rt;
}

static bool SerializationIter_toxml_struct( StyleType styleType, std::string& out, papuga_SerializationIter* seritr, int structid, const papuga_StructInterfaceDescription* structs, int maxDepth, papuga_ErrorCode& errcode)
{
	bool rt = true;
	int elementcnt = 0;

	if (--maxDepth == 0)
	{
		errcode = papuga_MaxRecursionDepthReached;
		return false;
	}
	for (; rt && papuga_SerializationIter_tag(seritr) != papuga_TagClose; papuga_SerializationIter_skip(seritr),++elementcnt)
	{
		const char* name = structs[ structid-1].members[ elementcnt].name;
		rt &= SerializationIter_toxml_named_elem( styleType, out, seritr, name, structs, maxDepth, errcode);
	}
	return rt;
}

static bool SerializationIter_toxml_dict( StyleType styleType, std::string& out, papuga_SerializationIter* seritr, const papuga_StructInterfaceDescription* structs, int maxDepth, papuga_ErrorCode& errcode)
{
	bool rt = true;

	if (--maxDepth == 0)
	{
		errcode = papuga_MaxRecursionDepthReached;
		return false;
	}
	for (; rt && papuga_SerializationIter_tag(seritr) != papuga_TagClose; papuga_SerializationIter_skip(seritr))
	{
		rt &= SerializationIter_toxml_dict_elem( styleType, out, seritr, structs, maxDepth, errcode);
	}
	return rt;
}

static bool Serialization_toxml( StyleType styleType, std::string& out, papuga_Serialization* ser, const char* name, const papuga_StructInterfaceDescription* structs, int maxDepth, papuga_ErrorCode& errcode)
{
	bool rt = true;
	StructType st;
	papuga_SerializationIter seritr;
	papuga_init_SerializationIter( &seritr, ser);

	// Get the structure type of the serialization:
	if (papuga_SerializationIter_tag( &seritr) == papuga_TagName)
	{
		if (ser->structid)
		{
			errcode = papuga_SyntaxError;
			return false;
		}
		st.id = StructType::Dict;
		st.structid = 0;
	}
	else if (ser->structid)
	{
		st.id = StructType::Struct;
		st.structid = ser->structid;
	}
	else
	{
		st.id = StructType::Array;
		st.structid = 0;
	}
	if (st.id == StructType::Array)
	{
		if (ser->structid)
		{
			st.id = StructType::Struct;
			st.structid = ser->structid;
		}
	}

	// Do the job:
	switch (st.id)
	{
		case StructType::Empty:
			break;
		case StructType::Array:
			rt &= SerializationIter_toxml_array( styleType, out, &seritr, name, structs, maxDepth, errcode);
			break;
		case StructType::Dict:
			append_tag_open( styleType, out, name);
			rt &= SerializationIter_toxml_dict( styleType, out, &seritr, structs, maxDepth, errcode);
			append_tag_close( styleType, out, name);
			break;
		case StructType::Struct:
			append_tag_open( styleType, out, name);
			rt &= SerializationIter_toxml_struct( styleType, out, &seritr, st.structid, structs, maxDepth, errcode);
			append_tag_close( styleType, out, name);
			break;
	}
	if (rt && !papuga_SerializationIter_eof( &seritr))
	{
		errcode = papuga_SyntaxError;
		return false;
	}
	return rt;
}

static void* RequestResult_toxml( StyleType styleType, const char* hdr, const char* tail, const papuga_RequestResult* self, papuga_StringEncoding enc, size_t* len, papuga_ErrorCode* err)
{
	std::string out;
	const char* rootelem = self->name;

	out.append( hdr);
	if (rootelem)
	{
		append_tag_open_node( styleType, out, rootelem);
	}
	papuga_RequestResultNode const* nd = self->nodes;
	for (; nd; nd = nd->next)
	{
		if (nd->name_optional)
		{
			if (!ValueVariant_toxml_node( styleType, out, nd->name, nd->value, self->structdefs, PAPUGA_MAX_RECURSION_DEPTH, *err)) return NULL;
		}
		else
		{
			if (!ValueVariant_toxml( styleType, out, nd->name, nd->value, self->structdefs, PAPUGA_MAX_RECURSION_DEPTH, *err)) return NULL;
		}
	}
	if (rootelem)
	{
		append_tag_close( styleType, out, rootelem);
	}
	out.append( tail);
	void* rt = papuga::encodeRequestResultString( out, enc, len, err);
	if (rt) papuga_Allocator_add_free_mem( self->allocator, rt);
	return rt;
}

extern "C" void* papuga_RequestResult_toxml( const papuga_RequestResult* self, papuga_StringEncoding enc, size_t* len, papuga_ErrorCode* err)
{
	try
	{
		char hdrbuf[ 256];
	
		std::snprintf( hdrbuf, sizeof(hdrbuf), "<?xml version=\"1.0\" encoding=\"%s\" standalone=\"yes\"?>\n", papuga_StringEncoding_name( enc));
		return RequestResult_toxml( StyleXML, hdrbuf, "\n", self, enc, len, err);
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

extern "C" void* papuga_RequestResult_tohtml5( const papuga_RequestResult* self, papuga_StringEncoding enc, const char* head, size_t* len, papuga_ErrorCode* err)
{
	try
	{
		std::string hdr;
		char hdrbuf[ 256];

		std::snprintf( hdrbuf, sizeof(hdrbuf), "<!DOCTYPE html>\n<head>\n<head>\n<body>\n<meta>\n<charset=\"%s\">\n</meta>\n", papuga_StringEncoding_name( enc));
		hdr.append( hdrbuf);
		hdr.append( head);
		hdr.append( "</head>\n<body>");
		return RequestResult_toxml( StyleHTML, hdr.c_str(), "\n</body>\n</html>", self, enc, len, err);
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



