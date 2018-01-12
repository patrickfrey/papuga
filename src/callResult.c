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
#include "papuga/callResult.h"
#include "papuga/valueVariant.h"
#include "papuga/serialization.h"
#include "papuga/hostObject.h"
#include "papuga/iterator.h"
#include "papuga/allocator.h"
#include "papuga/errors.h"
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

void papuga_destroy_CallResult( papuga_CallResult* self)
{
	papuga_destroy_Allocator( &self->allocator);
}

void papuga_init_CallResult( papuga_CallResult* self, void* allocbuf, size_t allocbufsize, char* errbuf, size_t errbufsize)
{
	papuga_init_Allocator( &self->allocator, allocbuf, allocbufsize);
	papuga_init_ErrorBuffer( &self->errorbuf, errbuf, errbufsize);
	self->nofvalues = 0;
}

bool papuga_add_CallResult_int( papuga_CallResult* self, papuga_Int val)
{
	if (self->nofvalues >= papuga_MAX_NOF_RETURNS) return false;
	papuga_init_ValueVariant_int( &self->valuear[ self->nofvalues++], val);
	return true;
}

bool papuga_add_CallResult_double( papuga_CallResult* self, double val)
{
	if (self->nofvalues >= papuga_MAX_NOF_RETURNS) return false;
	papuga_init_ValueVariant_double( &self->valuear[ self->nofvalues++], val);
	return true;
}

bool papuga_add_CallResult_bool( papuga_CallResult* self, bool val)
{
	if (self->nofvalues >= papuga_MAX_NOF_RETURNS) return false;
	papuga_init_ValueVariant_bool( &self->valuear[ self->nofvalues++], val);
	return true;
}

bool papuga_add_CallResult_string_copy( papuga_CallResult* self, const char* val, size_t valsize)
{
	char* val_copy;
	if (self->nofvalues >= papuga_MAX_NOF_RETURNS) return false;
	val_copy = papuga_Allocator_copy_string( &self->allocator, val, valsize);
	if (!val_copy) return false;
	papuga_init_ValueVariant_string( &self->valuear[ self->nofvalues++], val_copy, valsize);
	return true;
}

bool papuga_add_CallResult_string( papuga_CallResult* self, const char* val, size_t valsize)
{
	if (self->nofvalues >= papuga_MAX_NOF_RETURNS) return false;
	papuga_init_ValueVariant_string( &self->valuear[ self->nofvalues++], val, valsize);
	return true;
}

bool papuga_add_CallResult_blob_copy( papuga_CallResult* self, const void* val, size_t valsize)
{
	char* val_copy;
	if (self->nofvalues >= papuga_MAX_NOF_RETURNS) return false;
	val_copy = papuga_Allocator_alloc( &self->allocator, valsize, 0);
	if (!val_copy) return false;
	memcpy( val_copy, val, valsize);
	papuga_init_ValueVariant_blob( &self->valuear[ self->nofvalues++], val_copy, valsize);
	return true;
}

bool papuga_add_CallResult_blob( papuga_CallResult* self, const void* val, size_t valsize)
{
	if (self->nofvalues >= papuga_MAX_NOF_RETURNS) return false;
	papuga_init_ValueVariant_blob( &self->valuear[ self->nofvalues++], val, valsize);
	return true;
}

bool papuga_add_CallResult_charp_copy( papuga_CallResult* self, const char* val)
{
	if (self->nofvalues >= papuga_MAX_NOF_RETURNS) return false;
	if (val)
	{
		char* val_copy = papuga_Allocator_copy_charp( &self->allocator, val);
		if (!val_copy) return false;
		papuga_init_ValueVariant_charp( &self->valuear[ self->nofvalues++], val_copy);
	}
	else
	{
		papuga_init_ValueVariant( &self->valuear[ self->nofvalues++]);
	}
	return true;
}

bool papuga_add_CallResult_charp( papuga_CallResult* self, const char* val)
{
	if (self->nofvalues >= papuga_MAX_NOF_RETURNS) return false;
	if (val)
	{
		papuga_init_ValueVariant_charp( &self->valuear[ self->nofvalues++], val);
	}
	else
	{
		papuga_init_ValueVariant( &self->valuear[ self->nofvalues++]);
	}
	return true;
}

bool papuga_add_CallResult_string_enc( papuga_CallResult* self, papuga_StringEncoding enc, const void* val, size_t valsize)
{
	if (self->nofvalues >= papuga_MAX_NOF_RETURNS) return false;
	papuga_init_ValueVariant_string_enc( &self->valuear[ self->nofvalues++], enc, val, valsize);
	return true;
}

bool papuga_add_CallResult_serialization( papuga_CallResult* self)
{
	papuga_Serialization* ser;
	if (self->nofvalues >= papuga_MAX_NOF_RETURNS) return false;
	ser = papuga_Allocator_alloc_Serialization( &self->allocator);
	if (!ser) return false;
	papuga_init_ValueVariant_serialization( &self->valuear[ self->nofvalues++], ser);
	return true;
}

bool papuga_add_CallResult_hostobject( papuga_CallResult* self, int classid, void* data, papuga_Deleter destroy)
{
	papuga_HostObject* obj;
	if (self->nofvalues >= papuga_MAX_NOF_RETURNS) return false;
	obj = papuga_Allocator_alloc_HostObject( &self->allocator, classid, data, destroy);
	if (!obj) return false;
	papuga_init_ValueVariant_hostobj( &self->valuear[ self->nofvalues++], obj);
	return true;
}

bool papuga_add_CallResult_iterator( papuga_CallResult* self, void* data, papuga_Deleter destroy, papuga_GetNext getNext)
{
	papuga_Iterator* itr;
	if (self->nofvalues >= papuga_MAX_NOF_RETURNS) return false;
	itr = papuga_Allocator_alloc_Iterator( &self->allocator, data, destroy, getNext);
	if (!itr) return false;
	papuga_init_ValueVariant_iterator( &self->valuear[ self->nofvalues++], itr);
	return true;
}

bool papuga_add_CallResult_value( papuga_CallResult* self, const papuga_ValueVariant* value)
{
	if (self->nofvalues >= papuga_MAX_NOF_RETURNS) return false;
	papuga_init_ValueVariant_copy( &self->valuear[ self->nofvalues++], value);
	return true;
}

void papuga_CallResult_reportError( papuga_CallResult* self, const char* msg, ...)
{
	size_t nn;
	va_list ap;
	va_start( ap, msg);
	nn = vsnprintf( self->errorbuf.ptr, self->errorbuf.size, msg, ap);
	if (nn >= self->errorbuf.size-1)
	{
		self->errorbuf.ptr[ self->errorbuf.size-1] = 0;
	}
	va_end( ap);
}




