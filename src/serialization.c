/*
 * Copyright (c) 2017 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/*
* @brief Serialization of structures for papuga language bindings
* @file serialization.c
*/
#include "papuga/serialization.h"
#include "papuga/allocator.h"
#include "papuga/valueVariant.h"
#include <stdlib.h>
#include <string.h>

static void init_NodeChunk( papuga_NodeChunk* chunk)
{
	chunk->next=NULL;
	chunk->size=0;
}

static inline papuga_Node* alloc_node( papuga_Serialization* self)
{
	if (self->current->size == papuga_NodeChunkSize)
	{
		papuga_NodeChunk* next;
		next = (papuga_NodeChunk*)papuga_Allocator_alloc( self->allocator, sizeof(papuga_NodeChunk), 0);
		if (next == NULL) return NULL;
		init_NodeChunk( next);
		self->current->next = next;
		self->current = next;
	}
	return &self->current->ar[ self->current->size++];
}

static bool insertChunk( papuga_NodeChunk* chunk, papuga_Node* ar, int arsize, papuga_Allocator* allocator)
{
	papuga_NodeChunk* next;
	next = (papuga_NodeChunk*)papuga_Allocator_alloc( allocator, sizeof(papuga_NodeChunk), 0);
	if (next == NULL) return false;
	init_NodeChunk( next);
	if (arsize > papuga_NodeChunkSize) return false;
	memcpy( next->ar, ar, arsize * sizeof(papuga_NodeChunk));
	next->size = arsize;
	next->next = chunk->next;
	chunk->next = next;
	return true;
}

static papuga_NodeChunk* insertNodeArray( papuga_NodeChunk* chunk, papuga_Node* ar, int arsize, papuga_Allocator* allocator)
{
	int nidx = arsize;
	if (chunk->size + nidx > papuga_NodeChunkSize)
	{
		nidx = papuga_NodeChunkSize - chunk->size;
	}
	memcpy( &chunk->ar[ chunk->size], ar, nidx * sizeof(papuga_Node));
	chunk->size += nidx;
	while (nidx < arsize)
	{
		int chunksize = arsize - nidx;
		if (chunksize > papuga_NodeChunkSize)
		{
			chunksize = papuga_NodeChunkSize;
		}
		if (!insertChunk( chunk, ar+nidx, chunksize, allocator)) return NULL;
		nidx += chunksize;
		chunk = chunk->next;
	}
	return chunk;
}

#define PUSH_NODE_0(self,TAG,CONV)\
	papuga_Node* nd = alloc_node( self);\
	if (!nd) return false;\
	CONV( &nd->content);\
	nd->content._tag = TAG;\
	return true;

#define PUSH_NODE_1(self,TAG,CONV,p1)\
	papuga_Node* nd = alloc_node( self);\
	if (!nd) return false;\
	CONV( &nd->content, p1);\
	nd->content._tag = TAG;\
	return true;

#define PUSH_NODE_2(self,TAG,CONV,p1,p2)\
	papuga_Node* nd = alloc_node( self);\
	if (!nd) return false;\
	CONV( &nd->content, p1, p2);\
	nd->content._tag = TAG;\
	return true;

#define PUSH_NODE_3(self,TAG,CONV,p1,p2,p3)\
	papuga_Node* nd = alloc_node( self);\
	if (!nd) return false;\
	CONV( &nd->content, p1, p2, p3);\
	nd->content._tag = TAG;\
	return true;

bool papuga_Serialization_push( papuga_Serialization* self, papuga_Node* nd)
{
	papuga_Node* new_nd = alloc_node( self);
	if (!new_nd) return false;
	memcpy( new_nd, nd, sizeof(papuga_Node));
	return true;
}

bool papuga_Serialization_pushOpen( papuga_Serialization* self)
{
	papuga_Node* nd = alloc_node( self);
	if (!nd) return false;
	papuga_init_ValueVariant( &nd->content);
	nd->content._tag = papuga_TagOpen;
	return true;
}

bool papuga_Serialization_pushClose( papuga_Serialization* self)
{
	papuga_Node* nd = alloc_node( self);
	if (!nd) return false;
	papuga_init_ValueVariant( &nd->content);
	nd->content._tag = papuga_TagClose;
	return true;
}

bool papuga_Serialization_pushName_void( papuga_Serialization* self)
	{PUSH_NODE_0(self,papuga_TagName,papuga_init_ValueVariant)}

bool papuga_Serialization_pushName( papuga_Serialization* self, const papuga_ValueVariant* name)
	{PUSH_NODE_1(self,papuga_TagName,papuga_init_ValueVariant_copy,name)}

bool papuga_Serialization_pushName_string( papuga_Serialization* self, const char* name, int namelen)
	{PUSH_NODE_2(self,papuga_TagName,papuga_init_ValueVariant_string,name,namelen)}

bool papuga_Serialization_pushName_charp( papuga_Serialization* self, const char* name)
	{PUSH_NODE_1(self,papuga_TagName,papuga_init_ValueVariant_charp,name)}

bool papuga_Serialization_pushName_langstring( papuga_Serialization* self, papuga_StringEncoding enc, const char* name, int namelen)
	{PUSH_NODE_3(self,papuga_TagName,papuga_init_ValueVariant_langstring,enc,name,namelen)}

bool papuga_Serialization_pushName_int( papuga_Serialization* self, int64_t name)
	{PUSH_NODE_1(self,papuga_TagName,papuga_init_ValueVariant_int,name)}

bool papuga_Serialization_pushName_uint( papuga_Serialization* self, uint64_t name)
	{PUSH_NODE_1(self,papuga_TagName,papuga_init_ValueVariant_uint,name)}

bool papuga_Serialization_pushName_double( papuga_Serialization* self, double name)
	{PUSH_NODE_1(self,papuga_TagName,papuga_init_ValueVariant_double,name)}

bool papuga_Serialization_pushName_bool( papuga_Serialization* self, bool name)
	{PUSH_NODE_1(self,papuga_TagName,papuga_init_ValueVariant_bool,name)}


bool papuga_Serialization_pushValue_void( papuga_Serialization* self)
	{PUSH_NODE_0(self,papuga_TagValue,papuga_init_ValueVariant)}

bool papuga_Serialization_pushValue( papuga_Serialization* self, const papuga_ValueVariant* value)
	{PUSH_NODE_1(self,papuga_TagValue,papuga_init_ValueVariant_copy,value)}

bool papuga_Serialization_pushValue_string( papuga_Serialization* self, const char* value, int valuelen)
	{PUSH_NODE_2(self,papuga_TagValue,papuga_init_ValueVariant_string,value,valuelen)}

bool papuga_Serialization_pushValue_charp( papuga_Serialization* self, const char* value)
	{PUSH_NODE_1(self,papuga_TagValue,papuga_init_ValueVariant_charp,value)}

bool papuga_Serialization_pushValue_langstring( papuga_Serialization* self, papuga_StringEncoding enc, const char* value, int valuelen)
	{PUSH_NODE_3(self,papuga_TagValue,papuga_init_ValueVariant_langstring,enc,value,valuelen)}

bool papuga_Serialization_pushValue_int( papuga_Serialization* self, int64_t value)
	{PUSH_NODE_1(self,papuga_TagValue,papuga_init_ValueVariant_int,value)}

bool papuga_Serialization_pushValue_uint( papuga_Serialization* self, uint64_t value)
	{PUSH_NODE_1(self,papuga_TagValue,papuga_init_ValueVariant_uint,value)}

bool papuga_Serialization_pushValue_double( papuga_Serialization* self, double value)
	{PUSH_NODE_1(self,papuga_TagValue,papuga_init_ValueVariant_double,value)}

bool papuga_Serialization_pushValue_bool( papuga_Serialization* self, bool value)
	{PUSH_NODE_1(self,papuga_TagValue,papuga_init_ValueVariant_bool,value)}
bool papuga_Serialization_pushValue_hostobject( papuga_Serialization* self, papuga_HostObject* value)
	{PUSH_NODE_1(self,papuga_TagValue,papuga_init_ValueVariant_hostobj,value)}
bool papuga_Serialization_pushValue_serialization( papuga_Serialization* self, papuga_Serialization* value)
	{PUSH_NODE_1(self,papuga_TagValue,papuga_init_ValueVariant_serialization,value)}


static void NodeArray_push_Open( papuga_Node* nar, int* naridx)
{
	papuga_init_ValueVariant( &nar[ *naridx].content);
	nar[ *naridx].content._tag = papuga_TagOpen;
	++*naridx;
}
static void NodeArray_push_Name_index( papuga_Node* nar, int* naridx, unsigned int index)
{
	papuga_init_ValueVariant_uint( &nar[ *naridx].content, index);
	nar[ *naridx].content._tag = papuga_TagOpen;
	++*naridx;
}
static void NodeArray_push_Node( papuga_Node* nar, int* naridx, papuga_Node* nd)
{
	papuga_init_ValueVariant_copy( &nar[ *naridx].content, &nd->content);
	++*naridx;
}

bool papuga_Serialization_convert_array_assoc( papuga_Serialization* self, papuga_SerializationIter* seriter, unsigned int countfrom, papuga_ErrorCode* errcode)
{
	int bcnt = 0;
	papuga_Node nar[ papuga_NodeChunkSize*4];
	int naridx = 0;
	int seridx = seriter->chunkpos;

	if (papuga_SerializationIter_eof( seriter)) return true;
	for (;;)
	{
		if (seriter->chunkpos+1 == seriter->chunk->size)
		{
			papuga_NodeChunk* chunk = (papuga_NodeChunk*)seriter->chunk;

			/* At end of the current chunk check if the buffer is also full: */
			int restsize = papuga_NodeChunkSize - seridx;
			if (naridx >= restsize)
			{
				memcpy( &chunk->ar[ seridx], nar, restsize * sizeof(papuga_Node));
				chunk->size = papuga_NodeChunkSize;
				seriter->chunkpos = papuga_NodeChunkSize-1;

				if (naridx >= papuga_NodeChunkSize + restsize)
				{
					if (insertChunk( chunk, &nar[ restsize], papuga_NodeChunkSize, self->allocator))
					{
						seriter->chunk = chunk->next;
					}
					else
					{
						*errcode = papuga_NoMemError;
						return false;
					}
					naridx -= papuga_NodeChunkSize + restsize;
					memmove( nar, &nar[ papuga_NodeChunkSize + restsize], naridx * sizeof(papuga_Node));
				}
				else
				{
					naridx -= restsize;
					memmove( nar, &nar[ restsize], naridx * sizeof(papuga_Node));
				}
			}
			else
			{
				memcpy( &chunk->ar[ seridx], nar, naridx * sizeof(papuga_Node));
				chunk->size = seridx + naridx;
				seriter->chunkpos = chunk->size-1;
				naridx = 0;
			}
			seridx = 0;
			papuga_SerializationIter_skip( seriter);
		}
		switch (papuga_SerializationIter_tag( seriter))
		{
			case papuga_TagClose:
			{
				if (papuga_SerializationIter_eof( seriter))
				{
					papuga_NodeChunk* chunk = (papuga_NodeChunk*)seriter->chunk;
					chunk = insertNodeArray( chunk, nar, naridx, self->allocator);
					seriter->chunk = chunk;
					seriter->chunkpos = chunk->size-1;
					self->current = chunk;
					papuga_SerializationIter_skip( seriter);
					return true;
				}
				else
				{
					*errcode = papuga_TypeError;
					return false;
				}
			}
			case papuga_TagOpen:
			{
				papuga_NodeChunk* chunk = (papuga_NodeChunk*)seriter->chunk;
				NodeArray_push_Name_index( nar, &naridx, countfrom++);
				NodeArray_push_Open( nar, &naridx);

				papuga_SerializationIter_skip( seriter);
				for (bcnt=1; bcnt; papuga_SerializationIter_skip( seriter))
				{
					if (papuga_SerializationIter_tag( seriter) == papuga_TagClose)
					{
						if (papuga_SerializationIter_eof( seriter))
						{
							*errcode = papuga_UnexpectedEof;
							return false;
						}
						--bcnt;
					}
					else if (papuga_SerializationIter_tag( seriter) == papuga_TagOpen)
					{
						++bcnt;
					}
					NodeArray_push_Node( nar, &naridx, &chunk->ar[ seriter->chunkpos]);
				}
				break;
			}
			case papuga_TagName:
				*errcode = papuga_TypeError;
				return false;
			case papuga_TagValue:
			{
				papuga_NodeChunk* chunk = (papuga_NodeChunk*)seriter->chunk;
				NodeArray_push_Name_index( nar, &naridx, countfrom++);
				NodeArray_push_Node( nar, &naridx, &chunk->ar[ seriter->chunkpos]);
				break;
			}
		}
	}
}

void papuga_init_SerializationIter( papuga_SerializationIter* self, const papuga_Serialization* ser)
{
	self->chunk = &ser->head;
	self->chunkpos = 0;
	if (self->chunk->size==0)
	{
		self->tag = papuga_TagClose;
		self->value = NULL;
	}
	else
	{
		self->tag = (papuga_Tag)self->chunk->ar[0].content._tag;
		self->value = &self->chunk->ar[0].content;
	}
}

void papuga_init_SerializationIter_last( papuga_SerializationIter* self, const papuga_Serialization* ser)
{
	self->chunk = ser->current;
	if (self->chunk->size == 0)
	{
		self->chunkpos = 0;
		self->tag = papuga_TagClose;
		self->value = NULL;
	}
	else
	{
		self->chunkpos = self->chunk->size-1;
		self->tag = (papuga_Tag)self->chunk->ar[ self->chunkpos].content._tag;
		self->value = &self->chunk->ar[ self->chunkpos].content;
	}
}

void papuga_SerializationIter_skip( papuga_SerializationIter* self)
{
	if (++self->chunkpos >= self->chunk->size)
	{
		if (self->chunk->next)
		{
			self->chunk = self->chunk->next;
			self->chunkpos = 0;
			self->value = &self->chunk->ar[0].content;
			self->tag = (papuga_Tag)self->value->_tag;
		}
		else
		{
			self->chunkpos = self->chunk->size;
			self->tag = papuga_TagClose;
			self->value = NULL;
		}
	}
	else
	{
		self->value = &self->chunk->ar[ self->chunkpos].content;
		self->tag = (papuga_Tag)self->value->_tag;
	}
}

bool papuga_SerializationIter_more_than_one( const papuga_SerializationIter* self)
{
	return (self->chunkpos+1 < self->chunk->size || (self->chunkpos+1 == self->chunk->size && self->chunk->next));
}

