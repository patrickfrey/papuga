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
		return true;
	}
	else
	{
		return false;
	}
}

