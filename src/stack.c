/*
 * Copyright (c) 2017 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/*
* @brief Stack implementation for papuga
* @file stack.h
*/
#include "papuga/stack.h"
#include <stdlib.h>
#include <string.h>

void papuga_init_Stack( papuga_Stack* self, size_t elemsize, size_t nodesize, void* buf, size_t bufsize)
{
	if (sizeof(papuga_StackNode) + elemsize > bufsize)
	{
		self->buf = 0;
		self->top = 0;
	}
	else
	{
		self->buf = buf;
		self->top = (papuga_StackNode*)buf;
		self->top->prev = NULL;
		self->top->allocsize = bufsize;
		self->top->arsize = sizeof(papuga_StackNode);
	}
	self->elemsize = elemsize;
	self->nodesize = nodesize ? nodesize : 128;
	self->size = 0;
}

void papuga_destroy_Stack( papuga_Stack* self)
{
	papuga_StackNode* nd = self->top;
	while (nd)
	{
		papuga_StackNode* prev = nd->prev;
		if ((void*)nd != self->buf)
		{
			free( (void*)nd);
		}
		nd = prev;
	}
}

void* papuga_Stack_push( papuga_Stack* self)
{
	void* rt;
	if (!self->top || self->top->arsize + self->elemsize > self->top->allocsize)
	{
		size_t mm = sizeof(papuga_StackNode) + self->nodesize * self->elemsize;
		papuga_StackNode* newnode = (papuga_StackNode*)malloc( mm);
		if (!newnode) return NULL;
		newnode->prev = self->top;
		newnode->allocsize = mm;
		newnode->arsize = sizeof(papuga_StackNode);
		self->top = newnode;
	}
	rt = (void*)(((char*)(void*)self->top) + self->top->arsize);
	self->top->arsize += self->elemsize;
	self->size += 1;
	return rt;
}

void* papuga_Stack_pop( papuga_Stack* self)
{
	if (!self->size) return NULL;
	while (self->top->arsize == sizeof(papuga_StackNode))
	{
		papuga_StackNode* prev = self->top->prev;
		if ((void*)self->top == self->buf)
		{
			return NULL;
		}
		else
		{
			free( (void*)self->top);
			self->top = prev;
		}
	}
	if (!self->top || self->top->arsize <= sizeof(papuga_StackNode) + self->elemsize)
	{
		return NULL;
	}
	void* rt = ((char*)(void*)(self->top)) + self->top->arsize - self->elemsize;
	self->top->arsize -= self->elemsize;
	self->size -= 1;
	return rt;
}

void* papuga_Stack_top( const papuga_Stack* self)
{
	if (!self->size) return NULL;
	papuga_StackNode* nd = self->top;
	if (nd->arsize == sizeof(papuga_StackNode))
	{
		nd = nd->prev;
	}
	if (!nd || nd->arsize <= sizeof(papuga_StackNode) + self->elemsize)
	{
		return NULL;
	}
	return ((char*)(void*)(nd)) + self->top->arsize - self->elemsize;
}

bool papuga_Stack_top_n( const papuga_Stack* self, void** buf, size_t nn)
{
	if (nn > self->size) return false;
	papuga_StackNode* nd = self->top;
	if (!nd) return false;
	size_t arsize = nd->arsize;

	while (nn > 0)
	{
		while (arsize >= sizeof(papuga_StackNode) + self->elemsize)
		{
			buf[ nn-1] = ((char*)(void*)(nd)) + arsize - self->elemsize;
			arsize -= self->elemsize;
			nn -= 1;
		}
		nd = nd->prev;
		if (!nd) break;
		arsize = nd->arsize;
	}
	return nn == 0;
}

