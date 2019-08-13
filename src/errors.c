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
#include <string.h>

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
		case papuga_ValueUndefined: return _TXT("value is undefined");
		case papuga_MixedConstruction: return _TXT("incompatible elements starting with one type and shifting to another type of structure");
		case papuga_DuplicateDefinition: return _TXT("duplicate definition of unique key");
		case papuga_SyntaxError: return _TXT("syntax error");
		case papuga_UncaughtException: return _TXT("uncaught exception");
		case papuga_ExecutionOrder: return _TXT("violated a required order of execution");
		case papuga_AtomicValueExpected: return _TXT("atomic value expected");
		case papuga_NotAllowed: return _TXT("operation not permitted");
		case papuga_IteratorFailed: return _TXT("call of iterator failed, details not available");
		case papuga_AddressedItemNotFound: return _TXT("the addressed item was not found");
		case papuga_HostObjectError: return _TXT("error executing host object method");
		case papuga_AmbiguousReference: return _TXT("ambiguous reference");
		case papuga_MaxRecursionDepthReached: return _TXT("maximum recursion depth reached");
		case papuga_ComplexityOfProblem: return _TXT("refused processing because of its complexity");
		case papuga_InvalidRequest: return _TXT("valid syntax but unable to interprete request in the addressed context");
		default: return _TXT("unknown error");
	}
}

void papuga_ErrorBuffer_reportError( papuga_ErrorBuffer* self, const char* msg, ...)
{
	size_t msglen;
	va_list ap;
	va_start(ap, msg);
	msglen = vsnprintf( self->ptr, self->size, msg, ap);
	if (msglen >= self->size && self->size)
	{
		msglen = self->size-1;
		self->ptr[ msglen] = 0;
	}
	va_end(ap);
}

void papuga_ErrorBuffer_appendMessage( papuga_ErrorBuffer* self, const char* msg, ...)
{
	size_t len,msglen;
	va_list ap;
	va_start(ap, msg);
	len = strlen( self->ptr);
	msglen = vsnprintf( self->ptr+len, self->size-len, msg, ap) + len;
	if (msglen >= self->size && self->size)
	{
		msglen = self->size-1;
		self->ptr[ msglen] = 0;
	}
	va_end(ap);
}


