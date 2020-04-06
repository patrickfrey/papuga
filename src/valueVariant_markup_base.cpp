/*
 * Copyright (c) 2019 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/// \brief Common base for the markup modules
/// \file valueVariant_markup_base.cpp
#include "valueVariant_markup_base.hpp"

using namespace papuga;
using namespace papuga::markup;

OutputContextBase::OutputContextBase( const papuga_StructInterfaceDescription* structs_, int maxDepth_, papuga_StringEncoding encoding_)
		:out(),structs(structs_),depth(0),maxDepth(maxDepth_),encoding(encoding_)
{
	out.reserve( 4096);
}

void OutputContextBase::reset()
{
	depth = 0;
	out.clear();
}

bool OutputContextBase::isEqualAscii( const papuga_ValueVariant& val, const char* oth)
{
	papuga_ErrorCode errcode = papuga_Ok;
	if (val.valuetype != papuga_TypeString) return false;
	if (val.encoding == papuga_UTF8)
	{
		char const* si = val.value.string;
		for (; *oth && *oth == *si; ++oth,++si){}
		return (!*oth && !*si);
	}
	else
	{
		int pos = 0;
		int chr = papuga_ValueVariant_nextchar( &val, &pos, &errcode);
		for (; *oth && *oth == chr; ++oth,chr = papuga_ValueVariant_nextchar( &val, &pos, &errcode)){}
		if (errcode != papuga_Ok) throw ErrorException( errcode);
		return (!*oth && !chr);
	}
}

bool OutputContextBase::hasProtocolPrefix( const papuga_ValueVariant& val)
{
	papuga_ErrorCode errcode = papuga_Ok;
	int pos = 0;
	int chr = papuga_ValueVariant_nextchar( &val, &pos, &errcode);
	for (; pos<7 && chr < 127 && isAlpha( chr); chr = papuga_ValueVariant_nextchar( &val, &pos, &errcode)){}
	if (errcode == papuga_Ok && pos >= 3)
	{
		if (':' == chr
		&&  '/' == papuga_ValueVariant_nextchar( &val, &pos, &errcode)
		&&  '/' == papuga_ValueVariant_nextchar( &val, &pos, &errcode)) return true;
	}
	return false;
}

bool OutputContextBase::isArray( const papuga_ValueVariant& val)
{
	if (val.valuetype == papuga_TypeIterator) return true;
	if (val.valuetype == papuga_TypeSerialization)
	{
		papuga_SerializationIter iter;
		papuga_init_SerializationIter( &iter, val.value.serialization);
		return (papuga_SerializationIter_tag( &iter) == papuga_TagOpen
			|| papuga_SerializationIter_tag( &iter) == papuga_TagValue);
	}
	return false;
}

void OutputContextBase::appendEncoded_Xml( const char* str, std::size_t len)
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
		out.append( start, si-start);
		if (entity)
		{
			out.append( entity);
			entity = 0;
			++si;
		}
	}
}

void OutputContextBase::appendEncoded_AnsiC( const char* str, std::size_t len)
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
				case '\n': entity = "\\n"; break;
				case '\r': entity = "\\r"; break;
				case '\b': entity = "\\b"; break;
				case '\f': entity = "\\f"; break;
				case '\t': entity = "\\t"; break;
				case '"': entity = "\\\""; break;
				case '\\': entity = "\\\\"; break;
				default: continue;
			}
			break;
		}
		out.append( start, si-start);
		if (entity)
		{
			out.append( entity);
			entity = 0;
			++si;
		}
	}
}

void OutputContextBase::appendEncoded_Html5( const char* str, std::size_t len)
{
	char buf[ 4096];
	std::size_t encodedlen;
	papuga_ErrorCode errcode;
	const char* encoded = papuga_uri_encode_Html5( buf, sizeof(buf), &encodedlen, str, len, "/", &errcode);
	if (!encoded) throw ErrorException( errcode);
	out.append( encoded, encodedlen);
}

void OutputContextBase::appendEncoded_Rfc3986( const char* str, std::size_t len)
{
	char buf[ 4096];
	std::size_t encodedlen;
	papuga_ErrorCode errcode;
	const char* encoded = papuga_uri_encode_Rfc3986( buf, sizeof(buf), &encodedlen, str, len, "/", &errcode);
	if (!encoded) throw ErrorException( errcode);
	out.append( encoded, encodedlen);
}

void OutputContextBase::appendDecoded_AttributeName( const char* str, std::size_t len)
{
	if (len == 0 || str[0] != '-') throw ErrorException( papuga_SyntaxError);
	appendEncoded_Xml( str+1, len-1);
}

void OutputContextBase::appendAtomicValue_withEncoder( const papuga_ValueVariant& value, void (OutputContextBase::* encoder)( const char*, std::size_t))
{
	papuga_ErrorCode errcode;
	if (value.valuetype == papuga_TypeString)
	{
		if ((papuga_StringEncoding)value.encoding == papuga_UTF8)
		{
			((*this).*encoder)( value.value.string, value.length);
		}
		else
		{
			std::string utf8string;
			if (!papuga::ValueVariant_append_string( utf8string, value, errcode))
			{
				throw ErrorException( errcode);
			}
			((*this).*encoder)( utf8string.c_str(), utf8string.size());
		}
	}
	else if (!papuga::ValueVariant_append_string( out, value, errcode))
	{
		throw ErrorException( errcode);
	}
}

void OutputContextBase::appendAtomicValue( const papuga_ValueVariant& val)
{
	int sbuf[ 1024];
	size_t slen;
	papuga_Allocator allocator;
	papuga_ErrorCode errcode;
	
	papuga_init_Allocator( &allocator, sbuf, sizeof(sbuf));
	const char* sptr = papuga_ValueVariant_tostring( &val, &allocator, &slen, &errcode);
	if (!sptr)
	{
		papuga_destroy_Allocator( &allocator);
		throw ErrorException( errcode);
	}
	try
	{
		out.append( sptr, slen);
	}
	catch (...)
	{
		papuga_destroy_Allocator( &allocator);
		throw std::bad_alloc();
	}
	papuga_destroy_Allocator( &allocator);
}

void OutputContextBase::consumeClose( papuga_SerializationIter& iter)
{
	if (papuga_SerializationIter_tag( &iter) == papuga_TagClose)
	{
		if (papuga_SerializationIter_eof( &iter)) throw ErrorException( papuga_UnexpectedEof);
		papuga_SerializationIter_skip( &iter);
	}
	else
	{
		throw ErrorException( papuga_MixedConstruction);
	}
}

void* OutputContextBase::encodeRequestResultString( const std::string& out, papuga_Allocator* allocator, papuga_StringEncoding enc, size_t* len)
{
	void* rt;
	if (enc == papuga_UTF8)
	{
		rt = (void*)std::malloc( out.size()+1);
		if (!rt) throw std::bad_alloc();
		*len = out.size();
		std::memcpy( (char*)rt, out.c_str(), (*len)+1);
	}
	else
	{
		papuga_ErrorCode errcode;
		papuga_ValueVariant outvalue;
		papuga_init_ValueVariant_string( &outvalue, out.c_str(), out.size());
		size_t usize = papuga_StringEncoding_unit_size( enc);
		size_t rtbufsize = (out.size()*6) + usize;
		void* rtbuf = std::malloc( rtbufsize);
		if (!rtbuf) throw std::bad_alloc();
		const void* rtstr = papuga_ValueVariant_tostring_enc( &outvalue, enc, rtbuf, rtbufsize, len, &errcode);
		if (!rtstr)
		{
			std::free( rtbuf);
			throw ErrorException( errcode);
		}
		rt = (void*)std::realloc( rtbuf, *len + usize);
		if (!rt) rt = rtbuf;
	}
	if (rt && allocator)
	{
		if (!papuga_Allocator_add_free_mem( allocator, rt))
		{
			std::free( rt);
			throw std::bad_alloc();
		}
	}
	return rt;
}

void OutputContextBase::printValueOstream( std::ostream& out, const char* tagname, const papuga_ValueVariant& value)
{
	if (value.valuetype == papuga_TypeSerialization)
	{
		out << std::endl << "*" << std::endl;
		papuga_SerializationIter iter;
		papuga_init_SerializationIter( &iter, value.value.serialization);
		while (!papuga_SerializationIter_eof( &iter))
		{
			const char* elemtagname = papuga_Tag_name( papuga_SerializationIter_tag( &iter));
			const papuga_ValueVariant* elem = papuga_SerializationIter_value( &iter);
			papuga_SerializationIter_skip( &iter);
			printValueOstream( out, elemtagname, *elem);
			out << std::endl;
		}
		out << "*" << std::endl;
	}
	else if (!papuga_ValueVariant_defined( &value))
	{
		if (tagname)
		{
			out << tagname;
		}
	}
	else if (papuga_ValueVariant_isatomic( &value))
	{
		papuga_ErrorCode ec = papuga_Ok;
		papuga_Allocator localAllocator;
		papuga_init_Allocator( &localAllocator, 0, 0);
		size_t elemlen = 0;
		const char* elemstr = papuga_ValueVariant_tostring( &value, &localAllocator, &elemlen, &ec);
		if (!elemstr) elemstr = "";
		if (tagname)
		{
			out << tagname << " " << elemstr;
		}
		else
		{
			out << elemstr;
		}
	}
}

