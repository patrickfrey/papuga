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
#include <string>
#include <cstring>
#include <cstdlib>
#include <stdexcept>

static void* encodeRequestResultString( const std::string& out, papuga_StringEncoding enc, size_t* len, papuga_ErrorCode* err)
{
	if (enc == papuga_UTF8)
	{
		void* rt = (void*)std::malloc( out.size()+1);
		if (!rt)
		{
			*err = papuga_NoMemError;
			return NULL;
		}
		*len = out.size();
		std::memcpy( (char*)rt, out.c_str(), (*len)+1);
		return rt;
	}
	else
	{
		papuga_ValueVariant outvalue;
		papuga_init_ValueVariant_string( &outvalue, out.c_str(), out.size());
		size_t usize = papuga_StringEncoding_unit_size( enc);
		size_t rtbufsize = (out.size()+16) * usize;
		void* rtbuf = std::malloc( rtbufsize);
		if (!rtbuf)
		{
			*err = papuga_NoMemError;
			return NULL;
		}
		const void* rtstr = papuga_ValueVariant_tostring_enc( &outvalue, enc, rtbuf, rtbufsize, len, err);
		if (!rtstr)
		{
			std::free( rtbuf);
			return NULL;
		}
		void* rt = (void*)std::realloc( rtbuf, (*len + 1) * usize);
		if (!rt) rt = rtbuf;
		std::memset( (char*)rt + (*len) * usize, 0, usize); //... null termination
		return rt;
	}
}

enum StyleType {StyleHTML,StyleXML,StyleTEXT,StyleJSON};

struct OutputContext
{
	StyleType styleType;
	std::string out;
	const papuga_StructInterfaceDescription* structs;
	papuga_ErrorCode errcode;
	int maxDepth;
	int invisibleDepth;
	std::string indent;
	const char* array_separator;

	OutputContext( StyleType styleType_, const papuga_StructInterfaceDescription* structs_, int maxDepth_)
		:styleType(styleType_)
		,out()
		,structs(structs_)
		,errcode(papuga_Ok)
		,maxDepth(maxDepth_)
		,invisibleDepth(maxDepth_)
		,indent()
		,array_separator(0)
	{
		switch (styleType)
		{
			case StyleXML:
			case StyleHTML:
				break;
			case StyleJSON:
				array_separator = ",";
				/*no break here!*/
			case StyleTEXT:
				indent = "\n";
				break;
		}
	}
	void setNextTagInvisible()
	{
		invisibleDepth = maxDepth-2;
	}
	bool titleVisible() const
	{
		return maxDepth <= invisibleDepth;
	}
};

// Forward declarations:
static bool Serialization_tomarkup( OutputContext& ctx, const char* name, papuga_Serialization* ser);
static bool Serialization_tomarkup_fwd( OutputContext& ctx, papuga_Serialization* ser);
static bool ValueVariant_tomarkup( OutputContext& ctx, const char* name, const papuga_ValueVariant& value);
static bool ValueVariant_tomarkup_fwd( OutputContext& ctx, const papuga_ValueVariant& value);

static void append_tag_open_array_json( OutputContext& ctx, const char* name)
{
	ctx.out.append( ctx.indent);
	ctx.out.push_back( '\"');
	ctx.out.append( name);
	ctx.out.append( "\": [");
	ctx.indent.push_back( '\t');
}

static void append_tag_close_array_json( OutputContext& ctx)
{
	ctx.indent.resize( ctx.indent.size()-1);
	ctx.out.push_back( ']');
}

static void append_tag_open_struct( OutputContext& ctx)
{
	if (ctx.styleType != StyleJSON) throw std::logic_error("must not get here with other than JSON");
	ctx.out.append( ctx.indent);
	ctx.out.append( "{");
	ctx.indent.push_back( '\t');
}

static void append_tag_close_struct( OutputContext& ctx)
{
	if (ctx.styleType != StyleJSON) throw std::logic_error("must not get here with other than JSON");
	if (!ctx.indent.empty()) ctx.indent.resize( ctx.indent.size()-1);
	ctx.out.push_back( '}');
}

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
			ctx.out.append( "<div class=\"");
			ctx.out.append( name);
			ctx.out.append( "\">");
			if (ctx.titleVisible())
			{
				ctx.out.append( "<span class=\"title\">");
				ctx.out.append( name);
				ctx.out.append( "</span>");
			}
			break;
		case StyleTEXT:
			if (ctx.titleVisible())
			{
				ctx.out.append( ctx.indent);
				ctx.out.append( name);
				ctx.out.push_back( ':');
				ctx.indent.append( "  ");
			}
			break;
		case StyleJSON:
			if (ctx.titleVisible())
			{
				ctx.out.append( ctx.indent);
				ctx.out.push_back( '\"');
				ctx.out.append( name);
				ctx.out.append( "\": {");
				ctx.indent.push_back( '\t');
			}
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
		case StyleTEXT:
			if (ctx.indent.size() >= 2)
			{
				ctx.indent.resize( ctx.indent.size()-2);
			}
			break;
		case StyleJSON:
			if (ctx.titleVisible())
			{
				ctx.out.push_back( '}');
			}
			if (!ctx.indent.empty())
			{
				ctx.indent.resize( ctx.indent.size()-1);
			}
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
		case StyleTEXT:
			ctx.out.append( ctx.indent);
			ctx.out.append( name);
			ctx.out.push_back( ':');
			break;
		case StyleJSON:
			ctx.out.append( ctx.indent);
			ctx.out.push_back( '\"');
			ctx.out.append( name);
			ctx.out.append( "\": {}");
			break;
	}
}

static void append_tag_open_root( OutputContext& ctx, const char* name)
{
	switch (ctx.styleType)
	{
		case StyleHTML:
		case StyleTEXT:
			break;
		case StyleXML:
		case StyleJSON:
			append_tag_open( ctx, name);
			break;
	}
}

static void append_tag_close_root( OutputContext& ctx, const char* name)
{
	switch (ctx.styleType)
	{
		case StyleHTML:
		case StyleTEXT:
			break;
		case StyleXML:
		case StyleJSON:
			append_tag_close( ctx, name);
			break;
	}
}


static void append_encoded_entities_xml( OutputContext& ctx, const char* str, std::size_t len)
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

static void append_encoded_entities_ansi_c( OutputContext& ctx, const char* str, std::size_t len)
{
	const char* entity = 0;
	char const* si = str;
	const char* se = str + len;

	while (si != se)
	{
		char const* start = si;
		for (; si != se; ++si)
		{
			switch (*si)
			{
				case '\n': entity = "n"; break;
				case '\r': entity = "r"; break;
				case '\b': entity = "b"; break;
				case '\f': entity = "f"; break;
				case '\t': entity = "t"; break;
				case '"': entity = "\\\""; break;
				case '\\': entity = "\\\\"; break;
				default: continue;
			}
			break;
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

static void append_encoded_entities_as_string( OutputContext& ctx, const char* str, std::size_t len)
{
	switch (ctx.styleType)
	{
		case StyleTEXT:
			ctx.out.append( str, len);
			break;
		case StyleJSON:
			ctx.out.push_back( '"');
			append_encoded_entities_ansi_c( ctx, str, len);
			ctx.out.push_back( '"');
			break;
		case StyleHTML:
		case StyleXML:
			append_encoded_entities_xml( ctx, str, len);
			break;
	}
}

static bool append_value( OutputContext& ctx, const papuga_ValueVariant& value)
{
	if (value.valuetype == papuga_TypeString)
	{
		if ((papuga_StringEncoding)value.encoding == papuga_UTF8)
		{
			append_encoded_entities_as_string( ctx, value.value.string, value.length);
		}
		else
		{
			std::string utf8string;
			if (!papuga::ValueVariant_append_string( utf8string, value, ctx.errcode)) return false;
			append_encoded_entities_as_string( ctx, utf8string.c_str(), utf8string.size());
		}
	}
	else
	{
		if (!papuga::ValueVariant_append_string( ctx.out, value, ctx.errcode)) return false;
	}
	return true;
}

static void append_null_value( OutputContext& ctx)
{
	switch (ctx.styleType)
	{
		case StyleXML:
		case StyleHTML:
		case StyleTEXT: break;
		case StyleJSON:
			ctx.out.append( "null");
			break;
	}
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
			if (ctx.titleVisible())
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
		case StyleTEXT:
			ctx.out.append( ctx.indent);
			if (ctx.titleVisible())
			{
				ctx.out.append( name);
				ctx.out.append( ": ");
			}
			if (!append_value( ctx, value)) return false;
			break;
		case StyleJSON:
			ctx.out.append( ctx.indent);
			if (ctx.titleVisible())
			{
				ctx.out.push_back( '\"');
				ctx.out.append( name);
				ctx.out.append( "\": ");
			}
			if (!append_value( ctx, value)) return false;
			break;
	}
	return true;
}

static bool CallResult_tomarkup( OutputContext& ctx, const char* name, const papuga_CallResult& result)
{
	static const char* tupletags[ papuga_MAX_NOF_RETURNS] = {"1","2","3","4","5","6","7","8"};
	bool rt = true;
	if (result.nofvalues > 1)
	{
		append_tag_open( ctx, name);
		int ri = 0, re = result.nofvalues;
		for (; ri != re; ++ri)
		{
			rt &= ValueVariant_tomarkup( ctx, tupletags[ri], result.valuear[ri]);
		}
		append_tag_close( ctx, name);
	}
	else if (result.nofvalues == 1)
	{
		rt &= ValueVariant_tomarkup( ctx, name, result.valuear[0]);
	}
	else
	{
		append_tag_open_close_imm( ctx, name);
	}
	if (!rt)
	{
		ctx.errcode = papuga_NoMemError;
		return false;
	}
	return true;
}

static bool CallResult_tomarkup_fwd( OutputContext& ctx, const papuga_CallResult& result)
{
	bool rt = true;
	if (result.nofvalues > 1)
	{
		int ri = 0, re = result.nofvalues;
		for (; ri != re; ++ri)
		{
			rt &= ValueVariant_tomarkup_fwd( ctx, result.valuear[ri]);
		}
	}
	else if (result.nofvalues == 1)
	{
		rt &= ValueVariant_tomarkup_fwd( ctx, result.valuear[0]);
	}
	else
	{
		append_null_value( ctx);
	}
	if (!rt)
	{
		ctx.errcode = papuga_NoMemError;
		return false;
	}
	return true;
}

struct CallResultMapper
{
	
};

static bool Iterator_tomarkup( OutputContext& ctx, const char* name, papuga_Iterator* iterator)
{
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
			rt &= CallResult_tomarkup( ctx, name, result);
			papuga_destroy_CallResult( &result);
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

static bool Iterator_tomarkup_fwd( OutputContext& ctx, papuga_Iterator* iterator)
{
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
	if (!ctx.array_separator)
	{
		ctx.errcode = papuga_SyntaxError;
		return false;
	}
	papuga_init_Allocator( &allocator, result_mem, sizeof(result_mem));
	try
	{
		papuga_init_CallResult( &result, &allocator, false, error_mem, sizeof(error_mem));
		
		for (; itercnt < PAPUGA_MAX_ITERATOR_EXPANSION_LENGTH && rt && iterator->getNext( iterator->data, &result); ++itercnt)
		{
			if (itercnt)
			{
				ctx.out.append( ctx.array_separator);
			}
			rt &= CallResult_tomarkup_fwd( ctx, result);
			papuga_destroy_CallResult( &result);
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

static bool ValueVariant_tomarkup( OutputContext& ctx, const char* name, const papuga_ValueVariant& value)
{
	bool rt = true;
	if (papuga_ValueVariant_isatomic(&value))
	{
		if (!append_key_value( ctx, name, value)) return false;
	}
	else if (value.valuetype == papuga_TypeSerialization)
	{
		rt &= Serialization_tomarkup( ctx, name, value.value.serialization);
	}
	else if (value.valuetype == papuga_TypeIterator)
	{
		rt &= Iterator_tomarkup( ctx, name, value.value.iterator);
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

static bool ValueVariant_tomarkup_fwd( OutputContext& ctx, const papuga_ValueVariant& value)
{
	bool rt = true;
	if (papuga_ValueVariant_isatomic(&value))
	{
		if (!append_value( ctx, value)) return false;
	}
	else if (value.valuetype == papuga_TypeSerialization)
	{
		rt &= Serialization_tomarkup_fwd( ctx, value.value.serialization);
	}
	else if (value.valuetype == papuga_TypeIterator)
	{
		rt &= Iterator_tomarkup_fwd( ctx, value.value.iterator);
	}
	else if (!papuga_ValueVariant_defined( &value))
	{
		append_null_value( ctx);
	}
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
static bool SerializationIter_tomarkup_array( OutputContext& ctx, papuga_SerializationIter* seritr, const char* name);
static bool SerializationIter_tomarkup_array_fwd( OutputContext& ctx, papuga_SerializationIter* seritr);
static bool SerializationIter_tomarkup_struct( OutputContext& ctx, papuga_SerializationIter* seritr, int structid);
static bool SerializationIter_tomarkup_dict( OutputContext& ctx, papuga_SerializationIter* seritr);

struct StructType
{
	enum Id {Dict,Array,Struct,Empty};
	Id id;
	int structid;
};

static bool getSubStructType( StructType& st, const papuga_SerializationIter* seritr, papuga_ErrorCode& errcode)
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
			default: st.id = StructType::Empty; break;
		}
	}
	return true;
}

static bool getStructType( StructType& st, const papuga_Serialization* ser, const papuga_SerializationIter* seritr, papuga_ErrorCode& errcode)
{
	bool rt = true;

	// Get the structure type of the serialization:
	if (papuga_SerializationIter_tag( seritr) == papuga_TagName)
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
	return rt;
}


static bool ValueVariant_tomarkup_node( OutputContext& ctx, const char* name, const papuga_ValueVariant& value)
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
			return ValueVariant_tomarkup_fwd( ctx, value);
		}
		else
		{
			ctx.setNextTagInvisible();
			return ValueVariant_tomarkup( ctx, name, value);
		}
	}
	return ValueVariant_tomarkup( ctx, name, value);
}

static inline bool SerializationIter_tomarkup_elem_fwd( OutputContext& ctx, papuga_SerializationIter* seritr)
{
	bool rt = true;
	switch( papuga_SerializationIter_tag(seritr))
	{
		case papuga_TagClose:
		{
			ctx.errcode = papuga_UnexpectedEof;
			///... this case is caught before
			return false;
		}
		case papuga_TagValue:
		{
			const papuga_ValueVariant* value = papuga_SerializationIter_value(seritr);
			rt &= ValueVariant_tomarkup_fwd( ctx, *value);
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
			if (!getSubStructType( st, seritr, ctx.errcode)) return false;
			append_tag_open_struct( ctx);
			papuga_SerializationIter_skip(seritr);

			switch (st.id)
			{
				case StructType::Empty:
					append_null_value( ctx);
					break;
				case StructType::Array:
					rt &= SerializationIter_tomarkup_array_fwd( ctx, seritr);
					break;
				case StructType::Dict:
					rt &= SerializationIter_tomarkup_dict( ctx, seritr);
					break;
				case StructType::Struct:
					rt &= SerializationIter_tomarkup_struct( ctx, seritr, st.structid);
					break;
			}
			if (rt && papuga_SerializationIter_eof(seritr))
			{
				ctx.errcode = papuga_UnexpectedEof;
				return false;
			}
			append_tag_close_struct( ctx);
			break;
		}
	}
	return rt;
}

static inline bool SerializationIter_tomarkup_named_elem( OutputContext& ctx, papuga_SerializationIter* seritr, const char* name)
{
	bool rt = true;
	switch( papuga_SerializationIter_tag(seritr))
	{
		case papuga_TagClose:
		{
			ctx.errcode = papuga_UnexpectedEof;
			///... can only get here from SerializationIter_tomarkup_dict_elem( OutputContext& ctx, papuga_SerializationIter* seritr) after a name
			///	other cases are caught before
			return false;
		}
		case papuga_TagValue:
		{
			const papuga_ValueVariant* value = papuga_SerializationIter_value(seritr);
			rt &= ValueVariant_tomarkup( ctx, name, *value);
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
			if (!getSubStructType( st, seritr, ctx.errcode)) return false;
			papuga_SerializationIter_skip(seritr);

			switch (st.id)
			{
				case StructType::Empty:
					append_tag_open_close_imm( ctx, name);
					break;
				case StructType::Array:
					rt &= SerializationIter_tomarkup_array( ctx, seritr, name);
					break;
				case StructType::Dict:
					append_tag_open( ctx, name);
					rt &= SerializationIter_tomarkup_dict( ctx, seritr);
					append_tag_close( ctx, name);
					break;
				case StructType::Struct:
					append_tag_open( ctx, name);
					rt &= SerializationIter_tomarkup_struct( ctx, seritr, st.structid);
					append_tag_close( ctx, name);
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

static inline bool SerializationIter_tomarkup_dict_elem( OutputContext& ctx, papuga_SerializationIter* seritr)
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
	return SerializationIter_tomarkup_named_elem( ctx, seritr, name);
}

static bool SerializationIter_tomarkup_array( OutputContext& ctx, papuga_SerializationIter* seritr, const char* name)
{
	bool rt = true;
	if (ctx.styleType == StyleJSON)
	{
		append_tag_open_array_json( ctx, name);
		rt &= SerializationIter_tomarkup_array_fwd( ctx, seritr);
		append_tag_close_array_json( ctx);
	}
	else
	{
		if (--ctx.maxDepth == 0)
		{
			ctx.errcode = papuga_MaxRecursionDepthReached;
			return false;
		}
		for (; rt && papuga_SerializationIter_tag(seritr) != papuga_TagClose; papuga_SerializationIter_skip(seritr))
		{
			rt &= SerializationIter_tomarkup_named_elem( ctx, seritr, name);
		}
		++ctx.maxDepth;
	}
	return rt;
}

static bool SerializationIter_tomarkup_array_fwd( OutputContext& ctx, papuga_SerializationIter* seritr)
{
	bool rt = true;

	if (--ctx.maxDepth == 0)
	{
		ctx.errcode = papuga_MaxRecursionDepthReached;
		return false;
	}
	if (!ctx.array_separator)
	{
		ctx.errcode = papuga_SyntaxError;
		return false;
	}
	for (int elemcnt=0; rt && papuga_SerializationIter_tag(seritr) != papuga_TagClose; papuga_SerializationIter_skip(seritr),++elemcnt)
	{
		if (elemcnt) ctx.out.append( ctx.array_separator);
		rt &= SerializationIter_tomarkup_elem_fwd( ctx, seritr);
	}
	++ctx.maxDepth;
	return rt;
}

static bool SerializationIter_tomarkup_struct( OutputContext& ctx, papuga_SerializationIter* seritr, int structid)
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
		if (!name)
		{
			ctx.errcode = papuga_SyntaxError;
			return false;
		}
		if (ctx.array_separator && elementcnt) ctx.out.append( ctx.array_separator);
		rt &= SerializationIter_tomarkup_named_elem( ctx, seritr, name);
	}
	++ctx.maxDepth;
	return rt;
}

static bool SerializationIter_tomarkup_dict( OutputContext& ctx, papuga_SerializationIter* seritr)
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
		if (ctx.array_separator && elementcnt) ctx.out.append( ctx.array_separator);
		rt &= SerializationIter_tomarkup_dict_elem( ctx, seritr);
	}
	++ctx.maxDepth;
	return rt;
}

static bool Serialization_tomarkup( OutputContext& ctx, const char* name, papuga_Serialization* ser)
{
	bool rt = true;
	StructType st;
	papuga_SerializationIter seritr;
	papuga_init_SerializationIter( &seritr, ser);

	// Get the structure type of the serialization:
	if (!getStructType( st, ser, &seritr, ctx.errcode)) return false;

	// Do the job:
	switch (st.id)
	{
		case StructType::Empty:
			append_tag_open_close_imm( ctx, name);
			break;
		case StructType::Array:
			rt &= SerializationIter_tomarkup_array( ctx, &seritr, name);
			break;
		case StructType::Dict:
			append_tag_open( ctx, name);
			rt &= SerializationIter_tomarkup_dict( ctx, &seritr);
			append_tag_close( ctx, name);
			break;
		case StructType::Struct:
			append_tag_open( ctx, name);
			rt &= SerializationIter_tomarkup_struct( ctx, &seritr, st.structid);
			append_tag_close( ctx, name);
			break;
	}
	if (rt && !papuga_SerializationIter_eof( &seritr))
	{
		ctx.errcode = papuga_SyntaxError;
		return false;
	}
	return rt;
}

static bool Serialization_tomarkup_fwd( OutputContext& ctx, papuga_Serialization* ser)
{
	bool rt = true;
	StructType st;
	papuga_SerializationIter seritr;
	papuga_init_SerializationIter( &seritr, ser);

	// Get the structure type of the serialization:
	if (!getStructType( st, ser, &seritr, ctx.errcode)) return false;

	// Do the job:
	switch (st.id)
	{
		case StructType::Empty:
			break;
		case StructType::Array:
			rt &= SerializationIter_tomarkup_array_fwd( ctx, &seritr);
			break;
		case StructType::Dict:
			rt &= SerializationIter_tomarkup_dict( ctx, &seritr);
			break;
		case StructType::Struct:
			rt &= SerializationIter_tomarkup_struct( ctx, &seritr, st.structid);
			break;
	}
	if (rt && !papuga_SerializationIter_eof( &seritr))
	{
		ctx.errcode = papuga_SyntaxError;
		return false;
	}
	return rt;
}

static void* RequestResult_tomarkup( const papuga_RequestResult* self, StyleType styleType, const char* hdr, const char* tail, papuga_StringEncoding enc, size_t* len, papuga_ErrorCode* err)
{
	OutputContext ctx( styleType, self->structdefs, PAPUGA_MAX_RECURSION_DEPTH);
	const char* rootelem = self->name;

	ctx.out.append( hdr);
	if (rootelem)
	{
		append_tag_open_root( ctx, rootelem);
	}
	papuga_RequestResultNode const* nd = self->nodes;
	for (; nd; nd = nd->next)
	{
		if (ctx.array_separator && nd != self->nodes) ctx.out.append( ctx.array_separator);
		if (nd->name_optional)
		{
			if (!ValueVariant_tomarkup_node( ctx, nd->name, nd->value)) break;
		}
		else
		{
			if (!ValueVariant_tomarkup( ctx, nd->name, nd->value)) break;
		}
	}
	if (nd)
	{
		*err = ctx.errcode;
		return NULL;
	}
	if (rootelem)
	{
		append_tag_close_root( ctx, rootelem);
	}
	ctx.out.append( tail);
	void* rt = encodeRequestResultString( ctx.out, enc, len, &ctx.errcode);
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
		return RequestResult_tomarkup( self, StyleXML, hdrbuf, "\n", enc, len, err);
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
		return RequestResult_tomarkup( self, StyleHTML, hdr.c_str(), "\n</body>\n</html>", enc, len, err);
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
	try
	{
		return RequestResult_tomarkup( self, StyleTEXT, ""/*hdr*/, "\n"/*tail*/, enc, len, err);
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

extern "C" void* papuga_RequestResult_tojson( const papuga_RequestResult* self, papuga_StringEncoding enc, size_t* len, papuga_ErrorCode* err)
{
	try
	{
		return RequestResult_tomarkup( self, StyleJSON, "{"/*hdr*/, "\n}\n"/*tail*/, enc, len, err);
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




