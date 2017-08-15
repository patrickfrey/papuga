/*
 * Copyright (c) 2017 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/// \brief Stack implementation for papuga
/// \file stack.h
#include "papuga/stack.h"
#include <stdlib.h>
#include <string.h>

void papuga_init_Stack( papuga_Stack* self, size_t elemsize, void* buf, size_t bufsize)
{
	self->elemsize = elemsize;
	self->arsize = 0;
	if (buf && bufsize/elemsize > 0) {
		self->allocated = false;
		self->ar = buf;
		self->allocsize = bufsize/elemsize;
	} else {
		self->allocated = true;
		self->ar = NULL;
		self->allocsize = 0;
	}
}

void papuga_destroy_Stack( papuga_Stack* self)
{
	if (self->ar && self->allocated)
	{
		free( self->ar);
		self->ar = NULL;
	}
}

static bool alloc_nodes( papuga_Stack* self, size_t addsize)
{
	size_t newsize = self->arsize + addsize;
	if (newsize < self->arsize) return false;
	if (newsize > self->allocsize)
	{
		size_t mm = self->allocsize ? (self->allocsize * 2) : 256;
		while (mm > self->allocsize && mm < newsize) mm *= 2;
		size_t newallocsize = mm;
		mm *= self->elemsize;
		if (mm < newsize) return false;
		if (self->allocated)
		{
			void* newmem = realloc( self->ar, mm);
			if (newmem == NULL) return false;
			self->ar = newmem;
		}
		else
		{
			void* newmem = malloc( mm);
			if (newmem == NULL) return false;
			memcpy( newmem, self->ar, self->arsize * self->elemsize);
			self->allocated = true;
			self->ar = newmem;
		}
		self->allocsize = newallocsize;
	}
	return true;
}

bool papuga_Stack_push( papuga_Stack* self, void* elem)
{
	if (!alloc_nodes( self, 1)) return false;
	memcpy( (char*)self->ar + (self->arsize * self->elemsize), elem, self->elemsize);
	++self->arsize;
	return true;
}

void* papuga_Stack_pop( papuga_Stack* self)
{
	if (self->arsize == 0) return NULL;
	return (void*)((char*)self->ar + ((--self->arsize) * self->elemsize));
}

void* papuga_Stack_top( const papuga_Stack* self)
{
	if (self->arsize == 0) return NULL;
	return (void*)((char*)self->ar + (self->arsize * self->elemsize));
}

