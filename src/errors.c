/*
 * Copyright (c) 2017 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/*
* @brief Representation of a result of a call to papuga language bindings
* @file callResult.c
*/
#include "papuga/typedefs.h"
#include <stdarg.h>
#include <stdio.h>

/* @brief Hook for GETTEXT */
#define _TXT(x) x

const char* papuga_ErrorCode_tostring( papuga_ErrorCode errorcode)
{
	switch (errorcode)
	{
		case papuga_Ok: return _TXT("Ok");
		case papuga_LogicError: return _TXT("logic error");
		case papuga_NoMemError: return _TXT("out of memory");
		case papuga_TypeError: return _TXT("type mismatch");
		case papuga_EncodingError: return _TXT("string character encoding error");
		case papuga_BufferOverflowError: return _TXT("internal buffer not big enough");
		case papuga_OutOfRangeError: return _TXT("value out of range");
		case papuga_NofArgsError: return _TXT("number of arguments does not match");
		case papuga_MissingSelf: return _TXT("self argument is missing");
		case papuga_InvalidAccess: return _TXT("invalid access");
		case papuga_UnexpectedEof: return _TXT("unexpected EOF");
		case papuga_NotImplemented: return _TXT("not implemented");
		case papuga_ValueUndefined: return _TXT("value is undefined (null)");
		case papuga_MixedConstruction: return _TXT("incompatible elements starting with one type and shifting to another type of structure");
		case papuga_DuplicateDefinition: return _TXT("duplicate definition of unique key");
		default: return _TXT("unknown error");
	}
}

void papuga_ErrorBuffer_reportError( papuga_ErrorBuffer* self, const char* msg, ...)
{
	va_list ap;
	va_start(ap, msg);
	vsnprintf( self->ptr, self->size, msg, ap);
	va_end(ap);
}

