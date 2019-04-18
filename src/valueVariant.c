/*
 * Copyright (c) 2017 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/*
* @brief Representation of a typed value for language bindings
* @file valueVariant.c
*/
#include "papuga/valueVariant.h"
#include "papuga/allocator.h"
#include "papuga/serialization.h"
#include "papuga/typedefs.h"
#include <stdlib.h>

const char* papuga_Type_name( int/*papuga_Type*/ type)
{
	switch (type)
	{
		case papuga_TypeVoid: return "Void";
		case papuga_TypeDouble: return "Double";
		case papuga_TypeInt: return "Int";
		case papuga_TypeBool: return "Bool";
		case papuga_TypeString: return "String";
		case papuga_TypeHostObject: return "HostObject";
		case papuga_TypeSerialization: return "Serialization";
		case papuga_TypeIterator: return "Iterator";
		default: return "unknown";
	}
}

const char* papuga_StringEncoding_name( papuga_StringEncoding encoding)
{
	switch (encoding)
	{
		case papuga_UTF8: return "UTF-8";
		case papuga_UTF16BE: return "UTF-16BE";
		case papuga_UTF16LE: return "UTF-16LE";
		case papuga_UTF16: return "UTF-16";
		case papuga_UTF32BE: return "UTF-32BE";
		case papuga_UTF32LE: return "UTF-32LE";
		case papuga_UTF32: return "UTF-32";
		case papuga_Binary: return "binary";
		default: return "unknown";
			
	}
}

int papuga_StringEncoding_unit_size( papuga_StringEncoding encoding)
{
	switch (encoding)
	{
		case papuga_UTF8: return 1;
		case papuga_UTF16BE: return 2;
		case papuga_UTF16LE: return 2;
		case papuga_UTF16: return 2;
		case papuga_UTF32BE: return 4;
		case papuga_UTF32LE: return 4;
		case papuga_UTF32: return 4;
		case papuga_Binary: return 1;
		default: return 0;
			
	}
}

bool papuga_ValueVariant_print( FILE* out, const papuga_ValueVariant* val)
{
	const char* str;
	size_t len;
	papuga_Allocator allocator;
	papuga_ErrorCode errcode = papuga_Ok;
	char allocbuf[ 1024];
	papuga_init_Allocator( &allocator, allocbuf, sizeof(allocbuf));

	str = papuga_ValueVariant_tostring( val, &allocator, &len, &errcode);
	if (str)
	{
		fprintf( out, "%s", str);
		papuga_destroy_Allocator( &allocator);
		return true;
	}
	else
	{
		papuga_destroy_Allocator( &allocator);
		return false;
	}
}

static bool isValidStructId( int64_t id)
{
	return id >= 0 && id <= 255;
}

static bool ValueVariant_isvalid( const papuga_ValueVariant* value);

static bool Serialization_isvalid( const papuga_Serialization* ser)
{
	int taglevel = 0;
	papuga_SerializationIter iter;

	if (!isValidStructId( ser->structid)) return false;
	papuga_init_SerializationIter( &iter, (papuga_Serialization*)ser);
	for (; !papuga_SerializationIter_eof( &iter); papuga_SerializationIter_skip( &iter))
	{
		papuga_ValueVariant* value = papuga_SerializationIter_value( &iter);
		switch (papuga_SerializationIter_tag( &iter))
		{
			case papuga_TagValue:
				if (!ValueVariant_isvalid( value))
				{
					return false;
				}
				break;
			case papuga_TagOpen:
				++taglevel;
				if (papuga_ValueVariant_defined( value))
				{
					if (value->valuetype != papuga_TypeInt || !isValidStructId( value->value.Int))
					{
						return false;
					}
				}
				break;
			case papuga_TagClose:
				--taglevel;
				if (papuga_ValueVariant_defined( value))
				{
					return false;
				}
				break;
			case papuga_TagName:
				if (papuga_ValueVariant_defined( value))
				{
					if (!papuga_ValueVariant_isatomic( value)) return false;
				}
				break;
			default:
				return false;
		}
	}
	if (taglevel != 0) return false;
	return true;
}

static bool ValueVariant_isvalid( const papuga_ValueVariant* value)
{
	if (!papuga_ValueVariant_defined( value))
	{
		return true;
	}
	else if (papuga_ValueVariant_isatomic( value))
	{
		return true;
	}
	else if (value->valuetype == papuga_TypeSerialization)
	{
		return Serialization_isvalid( value->value.serialization);
	}
	else if (value->valuetype == papuga_TypeHostObject || value->valuetype == papuga_TypeIterator)
	{
		return true;
	}
	else
	{
		return false;
	}
}

bool papuga_ValueVariant_isvalid( const papuga_ValueVariant* self)
{
	return ValueVariant_isvalid( self);
}

