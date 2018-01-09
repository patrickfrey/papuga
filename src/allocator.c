/*
 * Copyright (c) 2017 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/*
* @brief Allocator for memory blocks with ownership returned by papuga language binding functions
* @file allocator.h
*/
#include "papuga/allocator.h"
#include "papuga/serialization.h"
#include "papuga/hostObject.h"
#include "papuga/iterator.h"
#include "papuga/valueVariant.h"
#include "papuga/callResult.h"
#include <string.h>
#include <stdlib.h>

typedef struct papuga_ReferenceHostObject
{
	papuga_ReferenceHeader header;
	papuga_HostObject hostObject;
} papuga_ReferenceHostObject;

typedef struct papuga_ReferenceIterator
{
	papuga_ReferenceHeader header;
	papuga_Iterator iterator;
} papuga_ReferenceIterator;

typedef struct papuga_ReferenceAllocator
{
	papuga_ReferenceHeader header;
	papuga_Allocator allocator;
} papuga_ReferenceAllocator;

void papuga_destroy_ReferenceHeader( papuga_ReferenceHeader* hdritr)
{
	while (hdritr != NULL)
	{
		switch (hdritr->type)
		{
			case papuga_RefTypeHostObject:
			{
				papuga_ReferenceHostObject* obj = (papuga_ReferenceHostObject*)hdritr;
				papuga_destroy_HostObject( &obj->hostObject);
				break;
			}
			case papuga_RefTypeIterator:
			{
				papuga_ReferenceIterator* obj = (papuga_ReferenceIterator*)hdritr;
				papuga_destroy_Iterator( &obj->iterator);
				break;
			}
			case papuga_RefTypeAllocator:
			{
				papuga_ReferenceAllocator* obj = (papuga_ReferenceAllocator*)hdritr;
				papuga_destroy_Allocator( &obj->allocator);
				break;
			}
			default:
			{
				return;
			}
		}
		hdritr = hdritr->next;
	}
}

void papuga_destroy_AllocatorNode( papuga_AllocatorNode* self)
{
	papuga_AllocatorNode* itr;

	if (self->ar != NULL && self->allocated)
	{
		free( self->ar);
		self->ar = 0;
	}
	itr = self->next;
	self->next = 0;
	while (itr != NULL)
	{
		papuga_AllocatorNode* next;

		if (itr->ar != NULL && itr->allocated)
		{
			free( itr->ar);
			itr->ar = 0;
		}
		next = itr->next;
		free( itr);
		itr = next;
	}
}

static int isPowerOfTwo (unsigned int x)
{
	return (((x & (~x + 1)) == x));
}
struct MaxAlignStruct {int _;};
#define MAXALIGN	(sizeof(struct MaxAlignStruct))
#define STDBLOCKSIZE	4096
#define MAXBLOCKSIZE	(1<<31)

void* papuga_Allocator_alloc( papuga_Allocator* self, size_t blocksize, unsigned int alignment)
{
	void* rt;
	papuga_AllocatorNode* next;

	if (alignment == 0) alignment = MAXALIGN;
	if (!isPowerOfTwo( alignment)
		|| alignment > MAXALIGN
		|| blocksize == 0
		|| blocksize >= MAXBLOCKSIZE) return 0;
	if (self->root.ar != NULL)
	{
		unsigned int alignmentofs = (alignment - (self->root.arsize & (alignment-1))) & (MAXALIGN-1);
		if (self->root.allocsize - self->root.arsize >= blocksize + alignmentofs)
		{
			self->root.arsize += alignmentofs;
			rt = self->root.ar + self->root.arsize;
			self->root.arsize += blocksize;
			return rt;
		}
		next = (papuga_AllocatorNode*)calloc( 1, sizeof( papuga_AllocatorNode));
		if (next == NULL) return 0;
		memcpy( next, &self->root, sizeof(self->root));
		memset( &self->root, 0, sizeof(self->root));
		self->root.next = next;
	}
	/* Allocate new block: */
	self->root.allocsize = STDBLOCKSIZE;
	while (self->root.allocsize < blocksize)
	{
		self->root.allocsize *= 2;
	}
	self->root.ar = (char*)malloc( self->root.allocsize);
	if (self->root.ar == NULL) return NULL;
	self->root.allocated = true;
	self->root.arsize = blocksize;
	return self->root.ar;
}

char* papuga_Allocator_copy_string( papuga_Allocator* self, const char* str, size_t len)
{
	char* rt = (char*)papuga_Allocator_alloc( self, len+1, 1);
	if (rt)
	{
		memcpy( rt, str, len);
		rt[ len] = 0;
	}
	return rt;
}

char* papuga_Allocator_copy_string_enc( papuga_Allocator* self, const char* str, size_t len, papuga_StringEncoding enc)
{
	int usize = papuga_StringEncoding_unit_size( enc);
	size_t mm = usize * len;
	char* rt = papuga_Allocator_alloc( self, mm+usize, usize);
	if (rt)
	{
		memcpy( rt, str, mm);
		memset( rt+mm, 0, usize);
	}
	return rt;
}

char* papuga_Allocator_copy_charp( papuga_Allocator* self, const char* str)
{
	return papuga_Allocator_copy_string( self, str, strlen(str));
}

papuga_HostObject* papuga_Allocator_alloc_HostObject( papuga_Allocator* self, int classid_, void* object_, papuga_Deleter destroy_)
{
	papuga_ReferenceHostObject* rt = (papuga_ReferenceHostObject*)papuga_Allocator_alloc( self, sizeof( papuga_ReferenceHostObject), 0);
	if (!rt) return 0;
	rt->header.type = papuga_RefTypeHostObject;
	rt->header.next = self->reflist;
	self->reflist = &rt->header;
	papuga_init_HostObject( &rt->hostObject, classid_, object_, destroy_);
	return &rt->hostObject;
}

papuga_Serialization* papuga_Allocator_alloc_Serialization( papuga_Allocator* self)
{
	papuga_Serialization* rt = (papuga_Serialization*)papuga_Allocator_alloc( self, sizeof( papuga_Serialization), 0);
	if (!rt) return 0;
	papuga_init_Serialization( rt, self);
	return rt;
}

papuga_Iterator* papuga_Allocator_alloc_Iterator( papuga_Allocator* self, void* object_, papuga_Deleter destroy_, papuga_GetNext getNext_)
{
	papuga_ReferenceIterator* rt = (papuga_ReferenceIterator*)papuga_Allocator_alloc( self, sizeof( papuga_ReferenceIterator), 0);
	if (!rt) return 0;
	rt->header.type = papuga_RefTypeIterator;
	rt->header.next = self->reflist;
	self->reflist = &rt->header;
	papuga_init_Iterator( &rt->iterator, object_, destroy_, getNext_);
	return &rt->iterator;
}

papuga_Allocator* papuga_Allocator_alloc_Allocator( papuga_Allocator* self)
{
	papuga_ReferenceAllocator* rt = (papuga_ReferenceAllocator*)papuga_Allocator_alloc( self, sizeof( papuga_ReferenceAllocator), 0);
	if (!rt) return 0;
	rt->header.type = papuga_RefTypeAllocator;
	rt->header.next = self->reflist;
	self->reflist = &rt->header;
	papuga_init_Allocator( &rt->allocator, 0, 0);
	return &rt->allocator;
}

static bool copy_ValueVariant( papuga_ValueVariant* dest, papuga_ValueVariant* orig, papuga_Allocator* allocator, bool moveobj, papuga_ErrorCode* errcode);

static bool serializeValueVariant( papuga_Serialization* dest, papuga_ValueVariant* orig, papuga_Allocator* allocator, bool moveobj, papuga_ErrorCode* errcode)
{
	switch (orig->valuetype)
	{
		case papuga_TypeVoid:
		case papuga_TypeDouble:
		case papuga_TypeInt:
		case papuga_TypeBool:
			if (!papuga_Serialization_pushValue( dest, orig)) goto ERROR;
			break;
		case papuga_TypeString:
		case papuga_TypeHostObject:
		{
			papuga_ValueVariant valuecopy;

			if (!copy_ValueVariant( &valuecopy, orig, allocator, moveobj, errcode)) goto ERROR;
			if (!papuga_Serialization_pushValue( dest, &valuecopy)) goto ERROR;
			break;
		}
		case papuga_TypeSerialization:
		{
			papuga_SerializationIter seritr;

			papuga_init_SerializationIter( &seritr, orig->value.serialization);
			while (!papuga_SerializationIter_eof( &seritr))
			{
				if (papuga_SerializationIter_tag( &seritr) == papuga_TagValue)
				{
					if (!serializeValueVariant( dest, papuga_SerializationIter_value( &seritr), allocator, moveobj, errcode)) goto ERROR;
				}
				else
				{
					papuga_Node node;
					if (!copy_ValueVariant( &node.content, papuga_SerializationIter_value( &seritr), allocator, moveobj, errcode)) goto ERROR;
					node.content._tag = papuga_SerializationIter_tag( &seritr);
					if (!papuga_Serialization_push( dest, &node)) goto ERROR;
				}
				papuga_SerializationIter_skip( &seritr);
			}
			break;
		}
		case papuga_TypeIterator:
		{
			papuga_CallResult result;
			char result_buf[ 1024];
			char error_buf[ 128];
			papuga_Iterator* iterator = orig->value.iterator;

			papuga_init_CallResult( &result, result_buf, sizeof(result_buf), error_buf, sizeof(error_buf));
			while (iterator->getNext( iterator->data, &result))
			{
				if (!papuga_Serialization_pushOpen( dest)) goto ERROR;
				int ri = 0, re = result.nofvalues;
				for (; ri != re; ++ri)
				{
					if (!serializeValueVariant( dest, result.valuear + ri, allocator, moveobj, errcode)) goto ERROR;
				}
				if (!papuga_Serialization_pushClose( dest)) goto ERROR;

				papuga_destroy_CallResult( &result);
				papuga_init_CallResult( &result, result_buf, sizeof(result_buf), error_buf, sizeof(error_buf));
			}
			if (papuga_CallResult_hasError( &result))
			{
				*errcode = papuga_IteratorFailed;
				goto ERROR;
			}
		}
	}
	return true;
ERROR:
	if (*errcode == papuga_Ok)
	{
		*errcode = papuga_NoMemError;
	}
	return false;
}

static bool copy_ValueVariant( papuga_ValueVariant* dest, papuga_ValueVariant* orig, papuga_Allocator* allocator, bool moveobj, papuga_ErrorCode* errcode)
{
	switch (orig->valuetype)
	{
		case papuga_TypeVoid:
		case papuga_TypeDouble:
		case papuga_TypeInt:
		case papuga_TypeBool:
			papuga_init_ValueVariant_copy( dest, orig);
			break;
		case papuga_TypeString:
		{
			char* str = papuga_Allocator_copy_string_enc( allocator, orig->value.string, orig->length, orig->encoding);
			if (!str) goto ERROR;
			papuga_init_ValueVariant_string_enc( dest, orig->encoding, str, orig->length);
			break;
		}
		case papuga_TypeHostObject:
		{
			papuga_HostObject* hobj = orig->value.hostObject;
			papuga_HostObject* hobjcopy = papuga_Allocator_alloc_HostObject( allocator, hobj->classid, hobj->data, (moveobj)?hobj->destroy:NULL);
			if (!hobjcopy) goto ERROR;
			papuga_init_ValueVariant_hostobj( dest, hobjcopy);
			if (moveobj)
			{
				orig->value.hostObject->destroy = 0;
			}
			break;
		}
		case papuga_TypeIterator:
		case papuga_TypeSerialization:
		{
			papuga_Serialization* ser = papuga_Allocator_alloc_Serialization( allocator);
			if (!serializeValueVariant( ser, orig, allocator, moveobj, errcode)) goto ERROR;
			papuga_init_ValueVariant_serialization( dest, ser);
		}
	}
ERROR:
	if (*errcode == papuga_Ok)
	{
		*errcode = papuga_NoMemError;
	}
	return false;
}

bool papuga_Allocator_deepcopy_value( papuga_Allocator* self, papuga_ValueVariant* dest, papuga_ValueVariant* orig, bool moveobj, papuga_ErrorCode* errcode)
{
	return copy_ValueVariant( dest, orig, self, moveobj, errcode);
}

bool papuga_Allocator_used( papuga_Allocator* self)
{
	return (!!self->reflist || !!self->root.next || !!self->root.arsize);
}

bool papuga_Allocator_takeover( papuga_Allocator* dest, papuga_Allocator* oth)
{
	papuga_AllocatorNode* pv;
	papuga_AllocatorNode* nd;

	if (!papuga_Allocator_used( oth)) return true;

	pv = &oth->root;
	if (!pv->allocated) return false;
	nd = pv->next;
	while (nd)
	{
		if (!nd->allocated) return false;
		pv = nd;
		nd = nd->next;
	}
	nd = (papuga_AllocatorNode*)malloc( sizeof( papuga_AllocatorNode));
	if (nd == NULL) return false;

	memcpy( nd, &dest->root, sizeof(dest->root));
	memcpy( &dest->root, &oth->root, sizeof(dest->root));
	pv->next = nd;

	papuga_ReferenceHeader* pl = oth->reflist;
	if (pl != NULL)
	{
		for (; pl->next; pl = pl->next){}
		pl->next = dest->reflist;
		dest->reflist = oth->reflist;
	}
	papuga_init_Allocator( oth, 0, 0);
	return true;
}

