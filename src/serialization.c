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

static bool insertChunk( papuga_NodeChunk* chunk, papuga_Allocator* allocator)
{
	papuga_NodeChunk* next;
	next = (papuga_NodeChunk*)papuga_Allocator_alloc( allocator, sizeof(papuga_NodeChunk), 0);
	if (next == NULL) return false;
	init_NodeChunk( next);
	next->next = chunk->next;
	chunk->next = next;
	return true;
}

static bool insertChunk_filled( papuga_NodeChunk* chunk, int arsize, papuga_Allocator* allocator)
{
	papuga_NodeChunk* next;
	if (arsize > papuga_NodeChunkSize) return false;
	if (!insertChunk( chunk, allocator)) return false;
	next = chunk->next;
	memset( next->ar, 0, arsize * sizeof(next->ar[0]));
	next->size = arsize;
	return true;
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

bool papuga_Serialization_pushOpen_struct( papuga_Serialization* self, int structid)
{
	papuga_Node* nd = alloc_node( self);
	if (!nd) return false;
	papuga_init_ValueVariant_int( &nd->content, structid);
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

bool papuga_Serialization_pushName_string_enc( papuga_Serialization* self, papuga_StringEncoding enc, const void* name, int namelen)
	{PUSH_NODE_3(self,papuga_TagName,papuga_init_ValueVariant_string_enc,enc,name,namelen)}

bool papuga_Serialization_pushName_int( papuga_Serialization* self, int64_t name)
	{PUSH_NODE_1(self,papuga_TagName,papuga_init_ValueVariant_int,name)}

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

bool papuga_Serialization_pushValue_string_enc( papuga_Serialization* self, papuga_StringEncoding enc, const void* value, int valuelen)
	{PUSH_NODE_3(self,papuga_TagValue,papuga_init_ValueVariant_string_enc,enc,value,valuelen)}

bool papuga_Serialization_pushValue_int( papuga_Serialization* self, int64_t value)
	{PUSH_NODE_1(self,papuga_TagValue,papuga_init_ValueVariant_int,value)}

bool papuga_Serialization_pushValue_double( papuga_Serialization* self, double value)
	{PUSH_NODE_1(self,papuga_TagValue,papuga_init_ValueVariant_double,value)}

bool papuga_Serialization_pushValue_bool( papuga_Serialization* self, bool value)
	{PUSH_NODE_1(self,papuga_TagValue,papuga_init_ValueVariant_bool,value)}
bool papuga_Serialization_pushValue_hostobject( papuga_Serialization* self, papuga_HostObject* value)
	{PUSH_NODE_1(self,papuga_TagValue,papuga_init_ValueVariant_hostobj,value)}
bool papuga_Serialization_pushValue_serialization( papuga_Serialization* self, papuga_Serialization* value)
	{PUSH_NODE_1(self,papuga_TagValue,papuga_init_ValueVariant_serialization,value)}


enum {ChunkStackInitSize = 128};
typedef struct ChunkStack
{
	papuga_NodeChunk** ar;
	int allocsize;
	int size;
	papuga_NodeChunk* mem[ ChunkStackInitSize];
} ChunkStack;

static void init_ChunkStack( ChunkStack* stk)
{
	stk->ar = stk->mem;
	stk->size = 0;
	stk->allocsize = ChunkStackInitSize;
}

static void destroy_ChunkStack( ChunkStack* stk)
{
	if (stk->ar != stk->mem)
	{
		free( stk->ar);
	}
}

static bool ChunkStack_push( ChunkStack* stk, papuga_NodeChunk* chunk)
{
	if (stk->size == stk->allocsize)
	{
		int mm = stk->allocsize * 2 * sizeof( papuga_NodeChunk*);
		if (mm < stk->allocsize) return false;
		if (stk->ar == stk->mem)
		{
			papuga_NodeChunk** newar = (papuga_NodeChunk**)malloc( mm);
			if (!newar) return false;
			memcpy( newar, stk->ar, stk->allocsize * sizeof( papuga_NodeChunk*));
			stk->ar = newar;
		}
		else
		{
			papuga_NodeChunk** newar = (papuga_NodeChunk**)realloc( stk->ar, mm);
			if (!newar) return false;
			stk->ar = newar;
		}
		stk->allocsize *= 2;
	}
	stk->ar[ stk->size++] = chunk;
	return true;
}

static bool ChunkStack_fill_elements( ChunkStack* stk, int count, papuga_Serialization* ser)
{
	papuga_NodeChunk* last = stk->ar[ stk->size-1];
	int restcount = papuga_NodeChunkSize - last->size;
	if (restcount >= count)
	{
		memset( last->ar + last->size, 0, count * sizeof(last->ar[0]));
		last->size += count;
	}
	else
	{
		memset( last->ar + last->size, 0, restcount * sizeof(last->ar[0]));
		last->size += restcount;
		count -= restcount;

		while (count > 0)
		{
			int fillsize = (count >= papuga_NodeChunkSize) ? papuga_NodeChunkSize : count;
			if (!insertChunk_filled( last, fillsize, ser->allocator)) return false;
			count -= fillsize;
			last = last->next;
			ser->current = last;
			ChunkStack_push( stk, last);
		}
	}
	return true;
}

#define skipPrevChunkPos( chunkstk, chunkidx, chunkptr, chunkpos) \
	if (chunkpos == 0)\
	{\
		if (chunkidx)\
		{\
			chunkptr = chunkstk.ar[ --chunkidx];\
			chunkpos = chunkptr->size - 1;\
		}\
		else\
		{\
			chunkptr = NULL;\
			chunkidx = -1;\
			chunkpos = -1;\
		}\
	}\
	else\
	{\
		--chunkpos;\
	}

bool papuga_Serialization_convert_array_assoc( papuga_Serialization* self, const papuga_SerializationIter* seritersrc, unsigned int countfrom, papuga_ErrorCode* errcode)
{
	papuga_SerializationIter seriter;
	int bcnt = 0; /* open brackets */
	int acnt = 0; /* count of array elements */
	papuga_Tag lasttag = papuga_TagValue;
	ChunkStack chunkstk;
	papuga_NodeChunk* chunk;
	int chunkidx1;
	papuga_NodeChunk* chunk1;
	int pos1;
	int chunkidx2;
	papuga_NodeChunk* chunk2;
	int pos2;

	/* Initialization of source iterator: */
	if (papuga_SerializationIter_eof( seritersrc)) return true;
	papuga_init_SerializationIter_copy( &seriter, seritersrc);

	/* Register all chunks touched: */
	init_ChunkStack( &chunkstk);
	chunk = (papuga_NodeChunk*)seriter.chunk;
	for (; chunk; chunk=chunk->next)
	{
		ChunkStack_push( &chunkstk, chunk);
	}

	/* Count number of elements inserted (acnt): */
	for (; !papuga_SerializationIter_eof( &seriter); papuga_SerializationIter_skip( &seriter))
	{
		switch (papuga_SerializationIter_tag( &seriter))
		{
			case papuga_TagClose:
				--bcnt;
				break;
			case papuga_TagOpen:
				if (bcnt == 0)
				{
					if (lasttag == papuga_TagName)
					{
						*errcode = papuga_TypeError;
						goto ERROR;
					}
					++acnt;
				}
				++bcnt;
				break;
			case papuga_TagValue:
				if (bcnt == 0)
				{
					if (lasttag == papuga_TagName)
					{
						*errcode = papuga_TypeError;
						goto ERROR;
					}
					++acnt;
				}
				break;
			case papuga_TagName: break;
		}
		lasttag = papuga_SerializationIter_tag( &seriter);
	}

	/* Define iterator pointing to last element of the original serialization: */
	chunkidx1 = chunkstk.size -1;
	chunk1 = self->current;
	pos1 = chunk1->size - 1;

	/* Fill new empty nodes used at end of serialization: */
	if (!ChunkStack_fill_elements( &chunkstk, acnt, self))
	{
		*errcode = papuga_NoMemError;
		goto ERROR;
	}

	/* Define iterator pointing to last element of the destination serialization (at end of all new empty elements filled): */
	chunkidx2 = chunkstk.size -1;
	chunk2 = self->current;
	pos2 = chunk2->size - 1;

	/* Iterate from end of destination serialization to start and insert all new tags: */
	bcnt = 0; /* open brackets */
	while (pos1 != pos2 || chunk1 != chunk2)
	{
		papuga_Tag tag = (papuga_Tag)chunk1->ar[pos1].content._tag;
		memcpy( chunk2->ar + pos2, chunk1->ar + pos1, sizeof(papuga_Node));

		/* Get previous chunk of the source if we got to the start: */
		skipPrevChunkPos( chunkstk, chunkidx1, chunk1, pos1);
		/* Get previous chunk of the destination if we got to the start: */
		skipPrevChunkPos( chunkstk, chunkidx2, chunk2, pos2);

		switch (tag)
		{
			case papuga_TagClose:
				++bcnt;
				break;
			case papuga_TagOpen:
				--bcnt;
				/*no break here!*/
			case papuga_TagValue:
				if (bcnt == 0)
				{
					--acnt;
					/* Insert key tag into the destination serialization: */
					papuga_init_ValueVariant_int( &chunk2->ar[ pos2].content, acnt + countfrom);
					chunk2->ar[ pos2].content._tag = papuga_TagName;
					skipPrevChunkPos( chunkstk, chunkidx2, chunk2, pos2)
				}
				break;
			case papuga_TagName:
				break;
		}
	}
	destroy_ChunkStack( &chunkstk);
	return true;
ERROR:
	destroy_ChunkStack( &chunkstk);
	return false;
}

void papuga_init_SerializationIter( papuga_SerializationIter* self, papuga_Serialization* ser)
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

