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

struct OutputContext
{
	StyleType styleType;
	std::string out;
	const papuga_StructInterfaceDescription* structs;
	papuga_ErrorCode errcode;
	int maxDepth;
	int invisibleDepth;

	OutputContext( StyleType styleType_, const papuga_StructInterfaceDescription* structs_, int maxDepth_)
		:styleType(styleType_),out(),structs(structs_),errcode(papuga_Ok),maxDepth(maxDepth_),invisibleDepth(maxDepth_){}

	void htmlSetNextTagInvisible()
	{
		invisibleDepth = maxDepth-2;
	}
	bool htmlTitleVisible() const
	{
		return maxDepth <= invisibleDepth;
	}
};

// Forward declarations:
static bool Serialization_toxml( OutputContext& ctx, const char* name, papuga_Serialization* ser);
static bool ValueVariant_toxml( OutputContext& ctx, const char* name, const papuga_ValueVariant& value);

static void append_tag_open( OutputContext& ctx, const char* name)
{
	switch (ctx.styleType)
	{
		case StyleXML:
			ctx.out.push_back( '<');
			ctx.out.append( name);
			ctx.out.push_back( '>');
			break;
		case StyleHTML:
			if (ctx.htmlTitleVisible())
			{
				ctx.out.append( "<span class=\"title\">");
				ctx.out.append( name);
				ctx.out.append( "</span>");
			}
			ctx.out.append( "<div class=\"");
			ctx.out.append( name);
			ctx.out.append( "\">");
			break;
	}
}

static void append_tag_close( OutputContext& ctx, const char* name)
{
	switch (ctx.styleType)
	{
		case StyleXML:
			ctx.out.append( "</");
			ctx.out.append( name);
			ctx.out.push_back( '>');
			break;
		
		case StyleHTML:
			ctx.out.append( "</div>");
			break;
	}
}

static void append_tag_open_node( OutputContext& ctx, const char* name)
{
	switch (ctx.styleType)
	{
		case StyleXML:
			ctx.out.push_back( '<');
			ctx.out.append( name);
			ctx.out.push_back( '>');
			break;
		case StyleHTML:
			break;
	}
}

static void append_tag_open_close_imm( OutputContext& ctx, const char* name)
{
	switch (ctx.styleType)
	{
		case StyleXML:
			ctx.out.push_back( '<');
			ctx.out.append( name);
			ctx.out.append( "/>");
			break;
		case StyleHTML:
			ctx.out.append( "<div class=\"");
			ctx.out.append( name);
			ctx.out.append( "\"/>");
			break;
	}
}

static void append_encoded_entities( OutputContext& ctx, const char* str, std::size_t len)
{
	const char* entity = 0;
	char const* si = str;
	const char* se = str + len;

	while (si != se)
	{
		char const* start = si;
		for (; si != se; ++si)
		{
			if (*si == '&')
			{
				entity = "&amp;";
				break;
			}
			else if (*si == '<')
			{
				entity = "&lt;";
				break;
			}
			else if (*si == '>')
			{
				entity = "&gt;";
				break;
			}
			else if (*si == '"')
			{
				entity = "&quot;";
				break;
			}
			else if (*si == '\'')
			{
				entity = "&apos;";
				break;
			}
		}
		ctx.out.append( start, si-start);
		if (entity)
		{
			ctx.out.append( entity);
			entity = 0;
			++si;
		}
	}
}

static bool append_value( OutputContext& ctx, const papuga_ValueVariant& value)
{
	if (value.valuetype == papuga_TypeString)
	{
		if ((papuga_StringEncoding)value.encoding == papuga_UTF8)
		{
			append_encoded_entities( ctx, value.value.string, value.length);
		}
		else
		{
			std::string utf8string;
			if (!papuga::ValueVariant_append_string( utf8string, value, ctx.errcode)) return false;
			append_encoded_entities( ctx, utf8string.c_str(), utf8string.size());
		}
	}
	else
	{
		if (!papuga::ValueVariant_append_string( ctx.out, value, ctx.errcode)) return false;
	}
	return true;
}

static bool append_key_value( OutputContext& ctx, const char* name, const papuga_ValueVariant& value)
{
	switch (ctx.styleType)
	{
		case StyleXML:
			ctx.out.push_back( '<');
			ctx.out.append( name);
			ctx.out.push_back( '>');
			if (!append_value( ctx, value)) return false;
			ctx.out.append( "</");
			ctx.out.append( name);
			ctx.out.push_back( '>');
			break;
		case StyleHTML:
			ctx.out.append( "<div class=\"");
			ctx.out.append( name);
			ctx.out.append( "\">");
			if (ctx.htmlTitleVisible())
			{
				ctx.out.append( "<span class=\"name\">");
				ctx.out.append( name);
				ctx.out.append( "</span>");
			}
			ctx.out.append( "<span class=\"value\">");
			if (!append_value( ctx, value)) return false;
			ctx.out.append( "</span>");
			ctx.out.append( "</div>");
			break;
	}
	return true;
}

static bool Iterator_toxml( OutputContext& ctx, const char* name, papuga_Iterator* iterator)
{
	static const char* tupletags[ papuga_MAX_NOF_RETURNS] = {"1","2","3","4","5","6","7","8"};
	int itercnt = 0;
	papuga_Allocator allocator;
	papuga_CallResult result;
	int result_mem[ 1024];
	char error_mem[ 128];
	bool rt = true;

	if (--ctx.maxDepth == 0)
	{
		ctx.errcode = papuga_MaxRecursionDepthReached;
		return false;
	}
	papuga_init_Allocator( &allocator, result_mem, sizeof(result_mem));
	try
	{
		papuga_init_CallResult( &result, &allocator, false, error_mem, sizeof(error_mem));
		
		while (itercnt++ < PAPUGA_MAX_ITERATOR_EXPANSION_LENGTH && rt && iterator->getNext( iterator->data, &result))
		{
			if (result.nofvalues > 1)
			{
				append_tag_open( ctx, name);
				int ri = 0, re = result.nofvalues;
				for (; ri != re; ++ri)
				{
					rt &= ValueVariant_toxml( ctx, tupletags[ri], result.valuear[ri]);
				}
				append_tag_close( ctx, name);
			}
			else if (result.nofvalues == 1)
			{
				rt &= ValueVariant_toxml( ctx, name, result.valuear[0]);
			}
			else
			{
				append_tag_open_close_imm( ctx, name);
			}
			papuga_destroy_CallResult( &result);
			if (!rt)
			{
				ctx.errcode = papuga_NoMemError;
				break;
			}
			papuga_destroy_Allocator( &allocator);
			papuga_init_Allocator( &allocator, result_mem, sizeof(result_mem));
			papuga_init_CallResult( &result, &allocator, false, error_mem, sizeof(error_mem));
		}
	}
	catch (...)
	{
		papuga_destroy_Allocator( &allocator);
		ctx.errcode = papuga_NoMemError;
		rt = false;
	}
	if (papuga_CallResult_hasError( &result))
	{
		ctx.errcode = papuga_IteratorFailed;
		rt = false;
	}
	++ctx.maxDepth;
	return rt;
}

static bool ValueVariant_toxml( OutputContext& ctx, const char* name, const papuga_ValueVariant& value)
{
	bool rt = true;
	if (papuga_ValueVariant_isatomic(&value))
	{
		if (!append_key_value( ctx, name, value)) return false;
	}
	else if (value.valuetype == papuga_TypeSerialization)
	{
		rt &= Serialization_toxml( ctx, name, value.value.serialization);
	}
	else if (value.valuetype == papuga_TypeIterator)
	{
		rt &= Iterator_toxml( ctx, name, value.value.iterator);
	}
	else if (!papuga_ValueVariant_defined( &value))
	{}
	else
	{
		ctx.errcode = papuga_TypeError;
		return false;
	}
	if (!rt && ctx.errcode == papuga_Ok)
	{
		ctx.errcode = papuga_NoMemError;
	}
	return rt;
}

// Forward declarations:
static bool SerializationIter_toxml_array( OutputContext& ctx, papuga_SerializationIter* seritr, const char* name);
static bool SerializationIter_toxml_struct( OutputContext& ctx, papuga_SerializationIter* seritr, int structid);
static bool SerializationIter_toxml_dict( OutputContext& ctx, papuga_SerializationIter* seritr);

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

static bool ValueVariant_toxml_node( OutputContext& ctx, const char* name, const papuga_ValueVariant& value)
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
			return ValueVariant_toxml( ctx, NULL/*name*/, value);
		}
		else
		{
			ctx.htmlSetNextTagInvisible();
			return ValueVariant_toxml( ctx, name, value);
		}
	}
	return ValueVariant_toxml( ctx, name, value);
}

static inline bool SerializationIter_toxml_named_elem( OutputContext& ctx, papuga_SerializationIter* seritr, const char* name)
{
	bool rt = true;
	switch( papuga_SerializationIter_tag(seritr))
	{
		case papuga_TagClose:
		{
			ctx.errcode = papuga_UnexpectedEof;
			///... can only get here from SerializationIter_toxml_dict_elem( OutputContext& ctx, papuga_SerializationIter* seritr)
			///	other cases are caught before
			return false;
		}
		case papuga_TagValue:
		{
			const papuga_ValueVariant* value = papuga_SerializationIter_value(seritr);
			rt &= ValueVariant_toxml( ctx, name, *value);
			break;
		}
		case papuga_TagName:
		{
			ctx.errcode = papuga_SyntaxError;
			return false;
		}
		case papuga_TagOpen:
		{
			StructType st;
			if (!getStructType( st, seritr, ctx.errcode)) return false;
			papuga_SerializationIter_skip(seritr);

			switch (st.id)
			{
				case StructType::Empty:
					break;
				case StructType::Array:
					rt &= SerializationIter_toxml_array( ctx, seritr, name);
					break;
				case StructType::Dict:
					if (name)
					{
						append_tag_open( ctx, name);
						rt &= SerializationIter_toxml_dict( ctx, seritr);
						append_tag_close( ctx, name);
					}
					else
					{
						rt &= SerializationIter_toxml_dict( ctx, seritr);
					}
					break;
				case StructType::Struct:
					if (name)
					{
						append_tag_open( ctx, name);
						rt &= SerializationIter_toxml_struct( ctx, seritr, st.structid);
						append_tag_close( ctx, name);
					}
					else
					{
						rt &= SerializationIter_toxml_struct( ctx, seritr, st.structid);
					}
					break;
			}
			if (rt && papuga_SerializationIter_eof(seritr))
			{
				ctx.errcode = papuga_UnexpectedEof;
				return false;
			}
			break;
		}
	}
	return rt;
}

static inline bool SerializationIter_toxml_dict_elem( OutputContext& ctx, papuga_SerializationIter* seritr)
{
	const char* name = NULL;
	char namebuf[ 128];
	std::size_t namelen;

	if (papuga_SerializationIter_tag(seritr) == papuga_TagName)
	{
		const papuga_ValueVariant* value = papuga_SerializationIter_value(seritr);
		name = (const char*)papuga_ValueVariant_tostring_enc( value, papuga_UTF8, namebuf, sizeof(namebuf), &namelen, &ctx.errcode);
		if (!name) return false;
		namebuf[ namelen] = 0;
	}
	else
	{
		ctx.errcode = papuga_SyntaxError;
		return false;
	}
	papuga_SerializationIter_skip(seritr);
	return SerializationIter_toxml_named_elem( ctx, seritr, name);
}

static bool SerializationIter_toxml_array( OutputContext& ctx, papuga_SerializationIter* seritr, const char* name)
{
	bool rt = true;

	if (--ctx.maxDepth == 0)
	{
		ctx.errcode = papuga_MaxRecursionDepthReached;
		return false;
	}
	for (; rt && papuga_SerializationIter_tag(seritr) != papuga_TagClose; papuga_SerializationIter_skip(seritr))
	{
		rt &= SerializationIter_toxml_named_elem( ctx, seritr, name);
	}
	++ctx.maxDepth;
	return rt;
}

static bool SerializationIter_toxml_struct( OutputContext& ctx, papuga_SerializationIter* seritr, int structid)
{
	bool rt = true;
	int elementcnt = 0;

	if (--ctx.maxDepth == 0)
	{
		ctx.errcode = papuga_MaxRecursionDepthReached;
		return false;
	}
	for (; rt && papuga_SerializationIter_tag(seritr) != papuga_TagClose; papuga_SerializationIter_skip(seritr),++elementcnt)
	{
		const char* name = ctx.structs[ structid-1].members[ elementcnt].name;
		rt &= SerializationIter_toxml_named_elem( ctx, seritr, name);
	}
	++ctx.maxDepth;
	return rt;
}

static bool SerializationIter_toxml_dict( OutputContext& ctx, papuga_SerializationIter* seritr)
{
	bool rt = true;

	if (--ctx.maxDepth == 0)
	{
		ctx.errcode = papuga_MaxRecursionDepthReached;
		return false;
	}
	for (; rt && papuga_SerializationIter_tag(seritr) != papuga_TagClose; papuga_SerializationIter_skip(seritr))
	{
		rt &= SerializationIter_toxml_dict_elem( ctx, seritr);
	}
	++ctx.maxDepth;
	return rt;
}

static bool Serialization_toxml( OutputContext& ctx, const char* name, papuga_Serialization* ser)
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
			ctx.errcode = papuga_SyntaxError;
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
			rt &= SerializationIter_toxml_array( ctx, &seritr, name);
			break;
		case StructType::Dict:
			if (name)
			{
				append_tag_open( ctx, name);
				rt &= SerializationIter_toxml_dict( ctx, &seritr);
				append_tag_close( ctx, name);
			}
			else
			{
				rt &= SerializationIter_toxml_dict( ctx, &seritr);
			}
			break;
		case StructType::Struct:
			if (name)
			{
				append_tag_open( ctx, name);
				rt &= SerializationIter_toxml_struct( ctx, &seritr, st.structid);
				append_tag_close( ctx, name);
			}
			else
			{
				rt &= SerializationIter_toxml_struct( ctx, &seritr, st.structid);
			}
			break;
	}
	if (rt && !papuga_SerializationIter_eof( &seritr))
	{
		ctx.errcode = papuga_SyntaxError;
		return false;
	}
	return rt;
}

static void* RequestResult_toxml( const papuga_RequestResult* self, StyleType styleType, const char* hdr, const char* tail, papuga_StringEncoding enc, size_t* len, papuga_ErrorCode* err)
{
	OutputContext ctx( styleType, self->structdefs, PAPUGA_MAX_RECURSION_DEPTH);
	const char* rootelem = self->name;

	ctx.out.append( hdr);
	if (rootelem)
	{
		append_tag_open_node( ctx, rootelem);
	}
	papuga_RequestResultNode const* nd = self->nodes;
	for (; nd; nd = nd->next)
	{
		if (nd->name_optional)
		{
			if (!ValueVariant_toxml_node( ctx, nd->name, nd->value)) break;
		}
		else
		{
			if (!ValueVariant_toxml( ctx, nd->name, nd->value)) break;
		}
	}
	if (nd)
	{
		*err = ctx.errcode;
		return NULL;
	}
	if (rootelem)
	{
		append_tag_close( ctx, rootelem);
	}
	ctx.out.append( tail);
	void* rt = papuga::encodeRequestResultString( ctx.out, enc, len, &ctx.errcode);
	if (rt) papuga_Allocator_add_free_mem( self->allocator, rt);
	*err = ctx.errcode;
	return rt;
}

extern "C" void* papuga_RequestResult_toxml( const papuga_RequestResult* self, papuga_StringEncoding enc, size_t* len, papuga_ErrorCode* err)
{
	try
	{
		char hdrbuf[ 256];
	
		std::snprintf( hdrbuf, sizeof(hdrbuf), "<?xml version=\"1.0\" encoding=\"%s\" standalone=\"yes\"?>\n", papuga_StringEncoding_name( enc));
		return RequestResult_toxml( self, StyleXML, hdrbuf, "\n", enc, len, err);
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
		char hdrbuf[ 512];

		std::snprintf( hdrbuf, sizeof(hdrbuf), "<!DOCTYPE html>\n<html>\n<head>\n<meta charset=\"%s\"/>\n", papuga_StringEncoding_name( enc));
		hdr.append( hdrbuf);
		hdr.append( head);
		hdr.append( "</head>\n<body>\n");
		return RequestResult_toxml( self, StyleHTML, hdr.c_str(), "\n</body>\n</html>", enc, len, err);
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



