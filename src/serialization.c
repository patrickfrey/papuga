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
		if (self->freelist)
		{
			next = self->freelist;
			self->freelist = self->freelist->next;
			next->next = 0;
		}
		else
		{
			next = (papuga_NodeChunk*)papuga_Allocator_alloc( self->allocator, sizeof(papuga_NodeChunk), 0);
			if (next == NULL) return NULL;
		}
		init_NodeChunk( next);
		self->current->next = next;
		self->current = next;
	}
	return &self->current->ar[ self->current->size++];
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

bool papuga_Serialization_push_node( papuga_Serialization* self, const papuga_Node* nd)
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

bool papuga_Serialization_push( papuga_Serialization* self, papuga_Tag tag, const papuga_ValueVariant* value)
	{PUSH_NODE_1(self,tag,papuga_init_ValueVariant_value,value)}

bool papuga_Serialization_pushName_void( papuga_Serialization* self)
	{PUSH_NODE_0(self,papuga_TagName,papuga_init_ValueVariant)}

bool papuga_Serialization_pushName( papuga_Serialization* self, const papuga_ValueVariant* name)
	{PUSH_NODE_1(self,papuga_TagName,papuga_init_ValueVariant_value,name)}

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
	{PUSH_NODE_1(self,papuga_TagValue,papuga_init_ValueVariant_value,value)}

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


void papuga_Serialization_release_tail( papuga_Serialization* self, papuga_SerializationIter* seriter)
{
	if (!seriter->chunk) return;
	if (seriter->chunk->next)
	{
		/* Add freed blocks to freelist: */
		if (self->freelist)
		{
			papuga_NodeChunk* nd = self->freelist;
			while (nd->next) nd = nd->next;
			nd->next = seriter->chunk->next;
		}
		else
		{
			self->freelist = seriter->chunk->next;
		}
	}
	/* Adjust current position in serialization: */
	self->current = (papuga_NodeChunk*)seriter->chunk;
	self->current->size = seriter->chunkpos;
	self->current->next = 0;
}

static bool hasInnerSerialization( const papuga_Serialization* ser)
{
	papuga_SerializationIter itr;
	papuga_init_SerializationIter( &itr, ser);

	while (!papuga_SerializationIter_eof( &itr))
	{
		const papuga_ValueVariant* val = papuga_SerializationIter_value( &itr);
		if (val && val->valuetype == papuga_TypeSerialization)
		{
			return true;
		}
		papuga_SerializationIter_skip( &itr);
	}
	return false;
}

static bool Serialization_flatten( papuga_Serialization* dest, const papuga_Serialization* ser)
{
	bool rt = true;
	papuga_SerializationIter itr;
	papuga_init_SerializationIter( &itr, ser);

	while (rt && !papuga_SerializationIter_eof( &itr))
	{
		const papuga_ValueVariant* val = papuga_SerializationIter_value( &itr);
		if (val && val->valuetype == papuga_TypeSerialization)
		{
			if (val->value.serialization->structid)
			{
				rt &= papuga_Serialization_pushOpen_struct( dest, val->value.serialization->structid);
			}
			else
			{
				rt &= papuga_Serialization_pushOpen( dest);
			}
			rt &= Serialization_flatten( dest, val->value.serialization);
			rt &= papuga_Serialization_pushClose( dest);
		}
		else
		{
			rt &= papuga_Serialization_push_node( dest, papuga_SerializationIter_node( &itr));
		}
		papuga_SerializationIter_skip( &itr);
	}
	return rt;
}

bool papuga_Serialization_flatten( papuga_Serialization* ser)
{
	if (hasInnerSerialization( ser))
	{
		papuga_Serialization res;
		papuga_SerializationIter itr;

		papuga_init_Serialization( &res, ser->allocator);
		papuga_init_SerializationIter( &itr, ser);
		if (!Serialization_flatten( &res, ser)) return false;
		memcpy( ser, &res, sizeof(papuga_Serialization));
	}
	return true;
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

void papuga_init_SerializationIter_end( papuga_SerializationIter* self, const papuga_Serialization* ser)
{
	self->chunk = ser->current;
	self->chunkpos = self->chunk->size;
	self->tag = papuga_TagClose;
	self->value = NULL;
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

const papuga_Node* papuga_SerializationIter_node( const papuga_SerializationIter* self)
{
	if (self->value)
	{
		return &self->chunk->ar[ self->chunkpos];
	}
	return NULL;
}

papuga_Tag papuga_SerializationIter_follow_tag( const papuga_SerializationIter* self)
{
	int chunkpos = self->chunkpos + 1;
	if (chunkpos >= self->chunk->size)
	{
		if (self->chunk->next)
		{
			const papuga_NodeChunk* chunk = self->chunk->next;
			const papuga_ValueVariant* value = &chunk->ar[0].content;
			return (papuga_Tag)value->_tag;
		}
		else
		{
			return papuga_TagClose;
		}
	}
	else
	{
		const papuga_ValueVariant* value = &self->chunk->ar[ chunkpos].content;
		return (papuga_Tag)value->_tag;
	}
}

static bool SerializationIter_skip_structure_open( papuga_SerializationIter* self)
{
	int taglevel = 1;
	while (taglevel > 0 && !papuga_SerializationIter_eof( self))
	{
		papuga_Tag tg = papuga_SerializationIter_tag( self);
		papuga_SerializationIter_skip( self);
		switch (tg)
		{
			case papuga_TagValue:
				break;
			case papuga_TagOpen:
				++taglevel;
				break;
			case papuga_TagClose:
				--taglevel;
				break;
			case papuga_TagName:
				break;
			default:
				return false;
		}
	}
	return taglevel == 0;
}

bool papuga_SerializationIter_skip_structure( papuga_SerializationIter* self)
{
	papuga_Tag tg = papuga_SerializationIter_tag( self);
	if (papuga_SerializationIter_eof( self)) return false;
	switch (tg)
	{
		case papuga_TagValue:
			papuga_SerializationIter_skip( self);
			return true;
		case papuga_TagOpen:
			papuga_SerializationIter_skip( self);
			return SerializationIter_skip_structure_open( self);
		case papuga_TagClose:
			return false;
		case papuga_TagName:
			papuga_SerializationIter_skip( self);
			tg = papuga_SerializationIter_tag( self);
			switch (tg)
			{
				case papuga_TagValue:
					papuga_SerializationIter_skip( self);
					return true;
				case papuga_TagOpen:
					papuga_SerializationIter_skip( self);
					return SerializationIter_skip_structure_open( self);
				case papuga_TagClose:
					return false;
				case papuga_TagName:
					return false;
			}
			return false;
	}
	return false;
}

