/*
 * Copyright (c) 2017 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/// \brief Conversion methods for the variant type
/// \file valueVariant.cpp
#include "papuga/valueVariant.h"
#include "papuga/valueVariant.hpp"
#include "papuga/allocator.h"
#include "textwolf/cstringiterator.hpp"
#include "textwolf/charset.hpp"
#include "textwolf/textscanner.hpp"
#include <string.h>
#include <limits>
#ifndef __STDC_FORMAT_MACROS
#define __STDC_FORMAT_MACROS
#endif
#include <inttypes.h>

#define PRINT_FORMAT_INT "%" PRId64
#define PRINT_FORMAT_DOUBLE "%.15g"
#define SCAN_FORMAT_INT "%" PRId64
#define SCAN_FORMAT_DOUBLE "%lf"

#ifdef _MSC_VER
#define IS_BIG_ENDIAN  0
#elif __GNUC__
#define IS_BIG_ENDIAN  (__ORDER_BIG_ENDIAN__)
#else
#error Cannot determine endianness of platform
#endif

#if IS_BIG_ENDIAN
#define W16CHARSET textwolf::charset::UTF16BE
#define W32CHARSET textwolf::charset::UCS4BE
#else
#define W16CHARSET textwolf::charset::UTF16LE
#define W32CHARSET textwolf::charset::UCS4LE
#endif

static const unsigned char* convertEndianess2( const void* ptr, size_t len, papuga_Allocator* allocator, papuga_ErrorCode* err)
{
	unsigned char* rt = (unsigned char*)papuga_Allocator_alloc( allocator, len, 2);
	if (!rt)
	{
		*err = papuga_NoMemError;
		return NULL;
	}
	unsigned char const* pi = (const unsigned char*)ptr;
	const unsigned char* pe = pi + len;
	unsigned char* ri = (unsigned char*)rt;
	for (; pi != pe; pi += 2, ri += 2)
	{
		ri[1] = pi[0];
		ri[0] = pi[1];
	}
	return rt;
}

static const unsigned char* convertEndianess4( const void* ptr, size_t len, papuga_Allocator* allocator, papuga_ErrorCode* err)
{
	unsigned char* rt = (unsigned char*)papuga_Allocator_alloc( allocator, len, 4);
	if (!rt)
	{
		*err = papuga_NoMemError;
		return NULL;
	}
	unsigned char const* pi = (const unsigned char*)ptr;
	const unsigned char* pe = pi + len;
	unsigned char* ri = (unsigned char*)rt;
	for (; pi != pe; pi += 4, ri += 4)
	{
		ri[3] = pi[0];
		ri[2] = pi[1];
		ri[1] = pi[2];
		ri[0] = pi[3];
	}
	return rt;
}

template <class DESTCHARSET>
static void* uft8string_to_string_enc( char* destbuf, int destbufsize, size_t* destlen, const char* str, size_t strsize, papuga_ErrorCode* err)
{
	typedef textwolf::TextScanner<textwolf::CStringIterator,textwolf::charset::UTF8> ScannerUTF8;
	ScannerUTF8 itr( textwolf::CStringIterator( str, strsize));

	DESTCHARSET wcout;
	textwolf::StaticBuffer out( destbuf, destbufsize);

	textwolf::UChar chr;
	for (; 0!=(chr=*itr); ++itr)
	{
		if (chr == 0xFFFF) continue;

		wcout.print( chr, out);
	}
	*destlen = out.size();
	if (out.overflow())
	{
		*err = papuga_BufferOverflowError;
		return 0;
	}
	return destbuf;
}

static void* uft8string_to_any_string_enc( papuga_StringEncoding enc, const char* str, std::size_t strsize, void* buf, size_t bufsize, size_t* len, papuga_ErrorCode* err)
{
	void* rt;
	switch (enc)
	{
		case papuga_UTF8:
			rt = uft8string_to_string_enc<textwolf::charset::UTF8>( (char*)buf, bufsize, len, str, strsize, err);
			break;
		case papuga_UTF16BE:
			rt = uft8string_to_string_enc<textwolf::charset::UTF16BE>( (char*)buf, bufsize, len, str, strsize, err);
			break;
		case papuga_UTF16LE:
			rt = uft8string_to_string_enc<textwolf::charset::UTF16LE>( (char*)buf, bufsize, len, str, strsize, err);
			break;
		case papuga_UTF16:
			rt = uft8string_to_string_enc<W16CHARSET>( (char*)buf, bufsize, len, str, strsize, err);
			break;
		case papuga_UTF32BE:
			rt = uft8string_to_string_enc<textwolf::charset::UCS4BE>( (char*)buf, bufsize, len, str, strsize, err);
			break;
		case papuga_UTF32LE:
			rt = uft8string_to_string_enc<textwolf::charset::UCS4LE>( (char*)buf, bufsize, len, str, strsize, err);
			break;
		case papuga_UTF32:
			rt = uft8string_to_string_enc<W32CHARSET>( (char*)buf, bufsize, len, str, strsize, err);
			break;
		case papuga_Binary:
			*err = papuga_NotImplemented;
			rt = 0;
			break;
		default:
			*err = papuga_TypeError;
			rt = 0;
	}
	return rt;
}

template <class LANGCHARSET>
static char* string_enc_to_uft8string( papuga_Allocator* allocator, const char* str, size_t strsize, papuga_ErrorCode* err)
{
	typedef textwolf::TextScanner<textwolf::CStringIterator,LANGCHARSET> ScannerStringEnc;
	ScannerStringEnc itr( textwolf::CStringIterator( str, strsize));

	std::string out;

	textwolf::charset::UTF8 u8out;

	textwolf::UChar chr;
	for (; 0!=(chr=*itr); ++itr)
	{
		u8out.print( chr, out);
	}
	return papuga_Allocator_copy_string( allocator, out.c_str(), out.size());
}

static char* any_string_enc_to_uft8string( papuga_Allocator* allocator, papuga_StringEncoding enc, const void* str, std::size_t strsize, papuga_ErrorCode* err)
{
	switch (enc)
	{
		case papuga_UTF8:
			return string_enc_to_uft8string<textwolf::charset::UTF8>( allocator, (const char*)str, strsize, err);
		case papuga_UTF16BE:
			return string_enc_to_uft8string<textwolf::charset::UTF16BE>( allocator, (const char*)str, strsize, err);
		case papuga_UTF16LE:
			return string_enc_to_uft8string<textwolf::charset::UTF16LE>( allocator, (const char*)str, strsize, err);
		case papuga_UTF16:
			return string_enc_to_uft8string<W16CHARSET>( allocator, (const char*)str, strsize, err);
		case papuga_UTF32BE:
			return string_enc_to_uft8string<textwolf::charset::UCS4BE>( allocator, (const char*)str, strsize, err);
		case papuga_UTF32LE:
			return string_enc_to_uft8string<textwolf::charset::UCS4LE>( allocator, (const char*)str, strsize, err);
		case papuga_UTF32:
			return string_enc_to_uft8string<W32CHARSET>( allocator, (const char*)str, strsize, err);
		case papuga_Binary:
			*err = papuga_NotImplemented;
			return 0;
		default:
			*err = papuga_TypeError;
			return 0;
	}
}

template <class LANGCHARSET>
static std::string string_enc_to_uft8string_stl( const char* str, size_t strsize)
{
	typedef textwolf::TextScanner<textwolf::CStringIterator,LANGCHARSET> ScannerStringEnc;
	ScannerStringEnc itr( textwolf::CStringIterator( str, strsize));

	std::string rt;
	textwolf::charset::UTF8 u8out;

	textwolf::UChar chr;
	for (; 0!=(chr=*itr); ++itr)
	{
		u8out.print( chr, rt);
	}
	return rt;
}

static std::string any_string_enc_to_uft8string_stl( papuga_StringEncoding enc, const void* str, std::size_t strsize)
{
	switch (enc)
	{
		case papuga_UTF8:
			return string_enc_to_uft8string_stl<textwolf::charset::UTF8>( (const char*)str, strsize);
		case papuga_UTF16BE:
			return string_enc_to_uft8string_stl<textwolf::charset::UTF16BE>( (const char*)str, strsize);
		case papuga_UTF16LE:
			return string_enc_to_uft8string_stl<textwolf::charset::UTF16LE>( (const char*)str, strsize);
		case papuga_UTF16:
			return string_enc_to_uft8string_stl<W16CHARSET>( (const char*)str, strsize);
		case papuga_UTF32BE:
			return string_enc_to_uft8string_stl<textwolf::charset::UCS4BE>( (const char*)str, strsize);
		case papuga_UTF32LE:
			return string_enc_to_uft8string_stl<textwolf::charset::UCS4LE>( (const char*)str, strsize);
		case papuga_UTF32:
			return string_enc_to_uft8string_stl<W32CHARSET>( (const char*)str, strsize);
		case papuga_Binary:
		default:
			break;
	}
	return std::string();

}

template <class LANGCHARSET>
static char* string_enc_toascii( char* destbuf, size_t destbufsize, const char* str, size_t strsize, char nonAsciiSubstChar)
{
	typedef textwolf::TextScanner<textwolf::CStringIterator,LANGCHARSET> ScannerStringEnc;
	ScannerStringEnc itr( textwolf::CStringIterator( str, strsize));
	size_t destpos = 0;
	unsigned char chr;
	for (; destpos < destbufsize; ++itr)
	{
		if (0!=(chr=itr.ascii()))
		{
			destbuf[ destpos++] = chr;
		}
		else if (nonAsciiSubstChar)
		{
			destbuf[ destpos++] = nonAsciiSubstChar;
		}
		else
		{
			return 0;
		}
	}
	if (destpos >= destbufsize) return 0;
	destbuf[ destpos] = 0;
	return destbuf;
}

static char* any_string_enc_toascii( papuga_StringEncoding enc, char* destbuf, size_t destbufsize, void const* str, size_t strsize, char nonAsciiSubstChar)
{
	switch (enc)
	{
		case papuga_UTF8:
			return string_enc_toascii<textwolf::charset::UTF8>( destbuf, destbufsize, (const char*)str, strsize, nonAsciiSubstChar);
		case papuga_UTF16BE:
			return string_enc_toascii<textwolf::charset::UTF16BE>( destbuf, destbufsize, (const char*)str, strsize, nonAsciiSubstChar);
		case papuga_UTF16LE:
			return string_enc_toascii<textwolf::charset::UTF16LE>( destbuf, destbufsize, (const char*)str, strsize, nonAsciiSubstChar);
		case papuga_UTF16:
			return string_enc_toascii<W16CHARSET>( destbuf, destbufsize, (const char*)str, strsize, nonAsciiSubstChar);
		case papuga_UTF32BE:
			return string_enc_toascii<textwolf::charset::UCS4BE>( destbuf, destbufsize, (const char*)str, strsize, nonAsciiSubstChar);
		case papuga_UTF32LE:
			return string_enc_toascii<textwolf::charset::UCS4LE>( destbuf, destbufsize, (const char*)str, strsize, nonAsciiSubstChar);
		case papuga_UTF32:
			return string_enc_toascii<W32CHARSET>( destbuf, destbufsize, (const char*)str, strsize, nonAsciiSubstChar);
		case papuga_Binary:
		default:
			return 0;
	}
}

enum NumericType {NumericDouble,NumericInt,NumericNone};
template <class LANGCHARSET>
static NumericType string_enc_tonumstr( char* destbuf, size_t destbufsize, const char* str, size_t strsize)
{
	NumericType rt = NumericNone;
	typedef textwolf::TextScanner<textwolf::CStringIterator,LANGCHARSET> ScannerStringEnc;
	ScannerStringEnc itr( textwolf::CStringIterator( str, strsize));
	size_t destpos = 0;
	unsigned char chr = itr.ascii();
	++itr;
	if (!chr || destbufsize < 2) return NumericNone;
	if (chr == '-')
	{
		destbuf[ destpos++] = chr;
		chr = itr.ascii();
		++itr;
		if (chr < '0' || chr > '9') return NumericNone;

		rt = NumericInt;
	}
	else if (chr >= '0' && chr <= '9')
	{
		rt = NumericInt;
	}
	else
	{
		return NumericNone;
	}
	destbuf[ destpos++] = chr;

	for (; 0!=(chr=itr.ascii()) && destpos < destbufsize; ++itr)
	{
		if (chr == '.')
		{
			if (rt == NumericDouble) return NumericNone;
			rt = NumericDouble;
		}
		else if (chr < '0' || chr > '9')
		{
			break;
		}
		destbuf[ destpos++] = chr;
	}
	if (destpos < destbufsize && (chr|32) == 'e')
	{
		rt = NumericDouble;
		destbuf[ destpos++] = chr;
		++itr;
		chr = itr.ascii();
		if (destpos < destbufsize && (chr == '-' || chr == '+'))
		{
			destbuf[ destpos++] = chr;
			++itr;
		}
		for (; 0!=(chr=itr.ascii()) && destpos < destbufsize; ++itr)
		{
			if (chr < '0' || chr > '9')
			{
				break;
			}
			destbuf[ destpos++] = chr;
		}
	}
	if (chr != 0 || destpos >= destbufsize) return NumericNone;
	destbuf[ destpos] = 0;
	return rt;
}

static NumericType any_string_enc_tonumstr( papuga_StringEncoding enc, char* destbuf, size_t destbufsize, const void* str, size_t strsize)
{
	switch (enc)
	{
		case papuga_UTF8:
			return string_enc_tonumstr<textwolf::charset::UTF8>( destbuf, destbufsize, (const char*)str, strsize);
		case papuga_UTF16BE:
			return string_enc_tonumstr<textwolf::charset::UTF16BE>( destbuf, destbufsize, (const char*)str, strsize);
		case papuga_UTF16LE:
			return string_enc_tonumstr<textwolf::charset::UTF16LE>( destbuf, destbufsize, (const char*)str, strsize);
		case papuga_UTF16:
			return string_enc_tonumstr<W16CHARSET>( destbuf, destbufsize, (const char*)str, strsize);
		case papuga_UTF32BE:
			return string_enc_tonumstr<textwolf::charset::UCS4BE>( destbuf, destbufsize, (const char*)str, strsize);
		case papuga_UTF32LE:
			return string_enc_tonumstr<textwolf::charset::UCS4LE>( destbuf, destbufsize, (const char*)str, strsize);
		case papuga_UTF32:
			return string_enc_tonumstr<W32CHARSET>( destbuf, destbufsize, (const char*)str, strsize);
		case papuga_Binary:
		default:
			return NumericNone;
	}
}

static bool numstr_to_variant( papuga_ValueVariant* value, NumericType numtype, const char* numstr)
{
	switch (numtype)
	{
		case NumericDouble:
		{
			papuga_Float val;
			if (std::sscanf( numstr, SCAN_FORMAT_DOUBLE, &val) < 1)
			{
				return false;
			}
			papuga_init_ValueVariant_double( value, val);
			return true;
		}
		case NumericInt:
		{
			papuga_Int val;
			if (std::sscanf( numstr, SCAN_FORMAT_INT, &val) < 1)
			{
				return false;
			}
			papuga_init_ValueVariant_int( value, val);
			return true;
		}
		case NumericNone:
			break;
	}
	return false;
}

static bool bufprint_number_variant( char* buf, std::size_t bufsize, std::size_t& len, const papuga_ValueVariant* value, papuga_ErrorCode* err)
{
	if (value->valuetype == papuga_TypeDouble)
	{
		len = snprintf( buf, bufsize, PRINT_FORMAT_DOUBLE, value->value.Double);
	}
	else if (value->valuetype == papuga_TypeInt)
	{
		len = snprintf( buf, bufsize, PRINT_FORMAT_INT, value->value.Int);
	}
	else if (value->valuetype == papuga_TypeBool)
	{
		len = 1;
		if (bufsize >= 2)
		{
			buf[0] = value->value.Bool?'1':'0';
			buf[1] = 0;
		}
	}
	else
	{
		*err = papuga_TypeError;
		return false;
	}
	if (len >= bufsize)
	{
		*err = papuga_BufferOverflowError;
		return false;
	}
	return true;
}

static char* string_toascii( char* destbuf, size_t destbufsize, papuga_StringEncoding encoding, char const* src, size_t srcsize, char nonAsciiSubstChar)
{
	if (encoding == papuga_UTF8)
	{
		if (destbufsize <= srcsize) return NULL;
		char const* si = src;
		const char* se = src + srcsize;
		char* di = destbuf;
		while (si != se)
		{
			if (*si && (unsigned char)*si < 128)
			{
				*di++ = *si++;
			}
			else if (nonAsciiSubstChar)
			{
				*di++ = nonAsciiSubstChar;
			}
			else
			{
				return NULL;
			}
		}
		*di = 0;
		return destbuf;
	}
	else
	{
		return any_string_enc_toascii( encoding, destbuf, destbufsize, src, srcsize, nonAsciiSubstChar);
	}
}

extern "C" const char* papuga_ValueVariant_toascii( char* destbuf, size_t destbufsize, const papuga_ValueVariant* val, char nonAsciiSubstChar)
{
	if (val->valuetype == papuga_TypeString)
	{
		return string_toascii( destbuf, destbufsize, (papuga_StringEncoding)val->encoding, (const char*)val->value.string, val->length, nonAsciiSubstChar);
	}
	else if (papuga_ValueVariant_isnumeric( val) && destbufsize)
	{
		std::size_t numlen;
		papuga_ErrorCode err = papuga_Ok;
		if (!bufprint_number_variant( destbuf, destbufsize-1, numlen, val, &err)) return NULL;
		destbuf[numlen] = 0;
		return destbuf;
	}
	else if (!papuga_ValueVariant_defined( val) && destbufsize)
	{
		destbuf[0] = 0;
		return destbuf;
	}
	return NULL;
}

extern "C" const char* papuga_ValueVariant_tostring( const papuga_ValueVariant* value, papuga_Allocator* allocator, size_t* len, papuga_ErrorCode* err)
{
	if (value->valuetype == papuga_TypeString)
	{
		if ((papuga_StringEncoding)value->encoding == papuga_UTF8)
		{
			*len = value->length;
			return value->value.string;
		}
		else if ((papuga_StringEncoding)value->encoding == papuga_Binary)
		{
			*len = value->length;
			return value->value.string;
		}
		else
		{
			return any_string_enc_to_uft8string( allocator, (papuga_StringEncoding)value->encoding, value->value.string, value->length, err);
		}
	}
	else if (papuga_ValueVariant_isnumeric( value))
	{
		const char* rt;
		char localbuf[256];
		if (!bufprint_number_variant( localbuf, sizeof(localbuf), *len, value, err)) return NULL;
		rt = papuga_Allocator_copy_string( allocator, localbuf, *len);
		if (!rt)
		{
			*err = papuga_NoMemError;
			return NULL;
		}
		return rt;
	}
	else if (papuga_ValueVariant_defined( value))
	{
		*err = papuga_TypeError;
		return NULL;
	}
	else
	{
		*err = papuga_ValueUndefined;
		return NULL;
	}
}

std::string papuga::ValueVariant_tostring( const papuga_ValueVariant& value, papuga_ErrorCode& errcode)
{
	try
	{
		if (value.valuetype == papuga_TypeString)
		{
			if ((papuga_StringEncoding)value.encoding == papuga_UTF8)
			{
				return std::string( value.value.string, value.length);
			}
			else if ((papuga_StringEncoding)value.encoding == papuga_Binary)
			{
				errcode = papuga_TypeError;
				return std::string();

			}
			else
			{
				return any_string_enc_to_uft8string_stl( (papuga_StringEncoding)value.encoding, value.value.string, value.length);
			}
		}
		else if (papuga_ValueVariant_isnumeric( &value))
		{
			char localbuf[256];
			std::size_t numlen;
			if (!bufprint_number_variant( localbuf, sizeof(localbuf), numlen, &value, &errcode)) return std::string();
			return std::string( localbuf, numlen);
		}
		else if (papuga_ValueVariant_defined( &value))
		{
			errcode = papuga_TypeError;
			return std::string();
		}
		else
		{
			errcode = papuga_ValueUndefined;
			return std::string();
		}
	}
	catch (const std::bad_alloc&)
	{
		errcode = papuga_NoMemError;
		return std::string();
	}
}

bool papuga::ValueVariant_append_string( std::string& dest, const papuga_ValueVariant& value, papuga_ErrorCode& errcode)
{
	try
	{
		if (papuga_ValueVariant_isstring( &value))
		{
			if (value.encoding == papuga_UTF8)
			{
				dest.append( (const char*)value.value.string, value.length);
			}
			else
			{
				size_t usize = papuga_StringEncoding_unit_size( (papuga_StringEncoding)value.encoding);
				size_t capainc = (value.length*6) + usize;
				dest.resize( dest.size() + capainc);
				char* destptr = const_cast<char*>( dest.c_str() + dest.size());
				size_t addlen = 0;
				if (papuga_ValueVariant_tostring_enc( &value, papuga_UTF8, destptr, capainc, &addlen, &errcode))
				{
					dest.resize( dest.size() - capainc + addlen);
				}
				else
				{
					dest.resize( dest.size() - capainc);
					return false;
				}
			}
		}
		else if (papuga_ValueVariant_isnumeric( &value))
		{
			char localbuf[256];
			std::size_t numlen;
			if (!bufprint_number_variant( localbuf, sizeof(localbuf), numlen, &value, &errcode)) return false;
			dest.append( localbuf, numlen);
		}
		else if (papuga_ValueVariant_defined( &value))
		{
			errcode = papuga_TypeError;
		}
		else
		{
			errcode = papuga_ValueUndefined;
			return false;
		}
	}
	catch (const std::bad_alloc&)
	{
		errcode = papuga_NoMemError;
		return false;
	}
	catch (...)
	{
		errcode = papuga_UncaughtException;
		return false;
	}
	return true;
}

extern "C" const void* papuga_ValueVariant_tostring_enc( const papuga_ValueVariant* value, papuga_StringEncoding enc, void* buf, size_t bufsize, size_t* len, papuga_ErrorCode* err)
{
	if (value->valuetype == papuga_TypeString)
	{
		if ((papuga_StringEncoding)value->encoding == papuga_UTF8)
		{
			return uft8string_to_any_string_enc( enc, value->value.string, value->length, (char*)buf, bufsize, len, err);
		}
		else if (enc == value->encoding)
		{
			size_t usize = papuga_StringEncoding_unit_size( enc);
			size_t mm = value->length + usize;
			if (mm <= bufsize)
			{
				std::memcpy( buf, value->value.string, value->length);
				std::memset( (char*)buf+value->length, 0, usize);
				return buf;
			}
			else
			{
				*err = papuga_BufferOverflowError;
				return NULL;
			}
		}
		else
		{
			// arbirary conversions between character set encodings not implemented
			*err = papuga_NotImplemented;
			return NULL;
		}
	}
	else if (papuga_ValueVariant_isnumeric( value))
	{
		char localbuf[256];
		std::size_t numlen;
		if (!bufprint_number_variant( localbuf, sizeof(localbuf), numlen, value, err)) return 0;
		return uft8string_to_any_string_enc( enc, localbuf, numlen, (char*)buf, bufsize, len, err);
	}
	else if (papuga_ValueVariant_defined( value))
	{
		*err = papuga_TypeError;
		return NULL;
	}
	else
	{
		*err = papuga_ValueUndefined;
		return NULL;
	}
}

extern "C" const void* papuga_ValueVariant_toblob( const papuga_ValueVariant* value, papuga_Allocator* allocator, size_t* len, papuga_ErrorCode* err)
{
	if (value->valuetype == papuga_TypeString)
	{
		switch ((papuga_StringEncoding)value->encoding)
		{
			case papuga_UTF8:
				*len = value->length;
				return value->value.string;
			case papuga_UTF16BE:
				*len = value->length;
				if (IS_BIG_ENDIAN)
				{
					return value->value.string;
				}
				else
				{
					return convertEndianess2( value->value.string, value->length, allocator, err);
				}
			case papuga_UTF16LE:
				*len = value->length;
				if (IS_BIG_ENDIAN)
				{
					return convertEndianess2( value->value.string, value->length, allocator, err);
				}
				else
				{
					return value->value.string;
				}
			case papuga_UTF16:
				*len = value->length;
				return value->value.string;
			case papuga_UTF32BE:
				*len = value->length;
				if (IS_BIG_ENDIAN)
				{
					return value->value.string;
				}
				else
				{
					return convertEndianess4( value->value.string, value->length, allocator, err);
				}
			case papuga_UTF32LE:
				*len = value->length;
				if (IS_BIG_ENDIAN)
				{
					return convertEndianess4( value->value.string, value->length, allocator, err);
				}
				else
				{
					return value->value.string;
				}
			case papuga_UTF32:
				*len = value->length;
				return value->value.string;
			case papuga_Binary:
				*len = value->length;
				return value->value.string;
			default:
				*err = papuga_NotImplemented;
				return NULL;
		}
	}
	else if (papuga_ValueVariant_isnumeric( value))
	{
		if ((papuga_Type)value->valuetype == papuga_TypeDouble)
		{
			*len = sizeof(value->value.Double);
			return (const unsigned char*)&value->value.Double;
		}
		else if ((papuga_Type)value->valuetype == papuga_TypeInt)
		{
			*len = sizeof(value->value.Int);
			return (const unsigned char*)&value->value.Int;
		}
		else if ((papuga_Type)value->valuetype == papuga_TypeBool)
		{
			*len = sizeof(value->value.Bool);
			return (const unsigned char*)&value->value.Bool;
		}
		else if ((papuga_Type)value->valuetype == papuga_TypeVoid)
		{
			*len = 0;
			return 0;
		}
		else
		{
			*err = papuga_TypeError;
			return NULL;
		}
	}
	else if (papuga_ValueVariant_defined( value))
	{
		*err = papuga_TypeError;
		return NULL;
	}
	else
	{
		*err = papuga_ValueUndefined;
		return NULL;
	}
}

extern "C" int64_t papuga_ValueVariant_toint( const papuga_ValueVariant* value, papuga_ErrorCode* err)
{
	char destbuf[256];

	if (papuga_ValueVariant_isatomic( value))
	{
		if (value->valuetype == papuga_TypeInt)
		{
			return value->value.Int;
		}
		else if (value->valuetype == papuga_TypeBool)
		{
			return value->value.Bool;
		}
		else if (value->valuetype == papuga_TypeDouble)
		{
			if (value->value.Double > (double)std::numeric_limits<papuga_Int>::max())
			{
				*err = papuga_OutOfRangeError;
				return 0;
			}
			if (value->value.Double > 0)
			{
				return (int64_t)(value->value.Double + std::numeric_limits<float>::epsilon());
			}
			else
			{
				return (int64_t)(value->value.Double - std::numeric_limits<float>::epsilon());
			}
		}
		else if (value->valuetype == papuga_TypeString)
		{
			NumericType numtype = any_string_enc_tonumstr( (papuga_StringEncoding)value->encoding, destbuf, sizeof(destbuf), value->value.string, value->length);
			papuga_ValueVariant numval;
			if (!numstr_to_variant( &numval, numtype, destbuf))
			{
				*err = papuga_TypeError;
				return 0;
			}
			return papuga_ValueVariant_toint( &numval, err);
		}
		else
		{
			*err = papuga_TypeError;
			return 0;
		}
	}
	else if (papuga_ValueVariant_defined( value))
	{
		*err = papuga_AtomicValueExpected;
		return 0;
	}
	else
	{
		*err = papuga_ValueUndefined;
		return 0;
	}
}

extern "C" uint64_t papuga_ValueVariant_touint( const papuga_ValueVariant* value, papuga_ErrorCode* err)
{
	int64_t val = papuga_ValueVariant_toint( value, err);
	if (val < 0)
	{
		*err = papuga_TypeError;
		return 0;
	}
	return (uint64_t)val;
}

extern "C" double papuga_ValueVariant_todouble( const papuga_ValueVariant* value, papuga_ErrorCode* err)
{
	char destbuf[256];

	if (papuga_ValueVariant_isatomic( value))
	{
		if (value->valuetype == papuga_TypeDouble)
		{
			return value->value.Double;
		}
		else if (value->valuetype == papuga_TypeInt)
		{
			return value->value.Int;
		}
		else if (value->valuetype == papuga_TypeBool)
		{
			return value->value.Bool?1.0:0.0;
		}
		else if (value->valuetype == papuga_TypeString)
		{
			NumericType numtype = any_string_enc_tonumstr( (papuga_StringEncoding)value->encoding, destbuf, sizeof(destbuf), value->value.string, value->length);
			papuga_ValueVariant numval;
			if (!numstr_to_variant( &numval, numtype, destbuf))
			{
				*err = papuga_TypeError;
				return 0;
			}
			return papuga_ValueVariant_todouble( &numval, err);
		}
		else
		{
			*err = papuga_TypeError;
			return 0;
		}
	}
	else if (papuga_ValueVariant_defined( value))
	{
		*err = papuga_AtomicValueExpected;
		return 0;
	}
	else
	{
		*err = papuga_ValueUndefined;
		return 0;
	}
}

extern "C" bool papuga_ValueVariant_tobool( const papuga_ValueVariant* value, papuga_ErrorCode* err)
{
	int rt;
	char destbuf[256];
	const char* numstr;

	if (papuga_ValueVariant_isatomic( value))
	{
		if (value->valuetype == papuga_TypeInt)
		{
			return !!value->value.Int;
		}
		else if (value->valuetype == papuga_TypeBool)
		{
			return value->value.Bool;
		}
		else if (value->valuetype == papuga_TypeDouble)
		{
			return (value->value.Double < -std::numeric_limits<float>::epsilon()
				||  value->value.Double > +std::numeric_limits<float>::epsilon());
		}
		else if (value->valuetype == papuga_TypeString)
		{
			numstr = string_toascii( destbuf, sizeof(destbuf), (papuga_StringEncoding)value->encoding, (const char*)value->value.string, value->length, 0/*nonAsciiSubstChar*/);
			if (numstr == NULL) return 0;
			if (!numstr[1])
			{
				if (numstr[0] == '1' || (numstr[0]|32) == 'y' || (numstr[0]|32) == 't') return true;
				if (numstr[0] == '0' || (numstr[0]|32) == 'n' || (numstr[0]|32) == 'f') return false;
			}
			if (std::sscanf( numstr, "%d", &rt) < 1)
			{
				*err = papuga_TypeError;
				return 0;
			}
			return !!rt;
		}
		else
		{
			*err = papuga_TypeError;
			return 0;
		}
	}
	else if (papuga_ValueVariant_defined( value))
	{
		*err = papuga_AtomicValueExpected;
		return 0;
	}
	else
	{
		*err = papuga_ValueUndefined;
		return 0;
	}
}

extern "C" papuga_ValueVariant* papuga_ValueVariant_tonumeric( const papuga_ValueVariant* value, papuga_ValueVariant* res, papuga_ErrorCode* err)
{
	char destbuf[64];

	if (papuga_ValueVariant_isatomic( value))
	{
		if (papuga_ValueVariant_isnumeric( value))
		{
			if (value->valuetype == papuga_TypeInt)
			{
				papuga_init_ValueVariant_int( res, value->value.Int);
				return res;
			}
			else if (value->valuetype == papuga_TypeBool)
			{
				papuga_init_ValueVariant_bool( res, value->value.Bool);
				return res;
			}
			else if (value->valuetype == papuga_TypeDouble)
			{
				papuga_init_ValueVariant_double( res, value->value.Double);
				return res;
			}
			else
			{
				*err = papuga_TypeError;
				return 0;
			}
		}
		else if (value->valuetype == papuga_TypeString)
		{
			NumericType numtype = any_string_enc_tonumstr( (papuga_StringEncoding)value->encoding, destbuf, sizeof(destbuf), value->value.string, value->length);
			papuga_ValueVariant numval;
			if (!numstr_to_variant( &numval, numtype, destbuf))
			{
				*err = papuga_TypeError;
				return 0;
			}
			return papuga_ValueVariant_tonumeric( &numval, res, err);
		}
		else
		{
			*err = papuga_TypeError;
			return 0;
		}
	}
	else if (papuga_ValueVariant_defined( value))
	{
		*err = papuga_AtomicValueExpected;
		return 0;
	}
	else
	{
		*err = papuga_ValueUndefined;
		return 0;
	}
}

class CharIterator
{
public:
	CharIterator( const char* ptr_)
		:m_orig(ptr_),m_ptr(ptr_){}
	void operator++() {++m_ptr;}
	char operator*() const {return *m_ptr;}
	int position_increment() const {return m_ptr-m_orig;}

private:
	char const* m_orig;
	char const* m_ptr;
};

extern "C" int papuga_ValueVariant_nextchar( const papuga_ValueVariant* self, int* pos, papuga_ErrorCode* err)
{
	int rt = 0;
	if (!papuga_ValueVariant_isstring( self))
	{
		*err = papuga_NotImplemented;
		return 0;
	}
	if (*pos >= (int)self->length)
	{
		return 0;
	}
	CharIterator itr( self->value.string + *pos);
	char buf[ 32];
	unsigned int bufpos = 0;

	switch (self->encoding)
	{
		case papuga_UTF8:
			{textwolf::charset::UTF8 chs; rt = chs.value<CharIterator>( buf, bufpos, itr); break;}
		case papuga_UTF16BE:
			{textwolf::charset::UTF16BE chs; rt = chs.value( buf, bufpos, itr); break;}
		case papuga_UTF16LE:
			{textwolf::charset::UTF16LE chs; rt = chs.value( buf, bufpos, itr); break;}
		case papuga_UTF16:
			{textwolf::charset::UTF16<> chs; rt = chs.value( buf, bufpos, itr); break;}
		case papuga_UTF32BE:
			{textwolf::charset::UCS4BE chs; rt = chs.value( buf, bufpos, itr); break;}
		case papuga_UTF32LE:
			{textwolf::charset::UCS4LE chs; rt = chs.value( buf, bufpos, itr); break;}
		case papuga_UTF32:
			{textwolf::charset::UCS4BE chs; rt = chs.value( buf, bufpos, itr); break;}
		case papuga_Binary:
			*err = papuga_NotImplemented;
			return 0;
		default:
			*err = papuga_TypeError;
			return 0;
	}
	*pos += itr.position_increment();
	return rt;
}


