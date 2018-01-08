/*
 * Copyright (c) 2017 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/*
* \brief Automaton to describe and build papuga XML and JSON requests
* \file request.h
*/
#include "papuga/requestHandler.h"
#include "papuga/allocator.h"
#include "papuga/serialization.h"
#include "papuga/valueVariant.h"
#include <cstdlib>
#include <cstring>

template <typename TYPE>
static TYPE* alloc_type( papuga_Allocator* allocator)
{
	TYPE* rt = (TYPE*)papuga_Allocator_alloc( allocator, sizeof(TYPE), 0);
	if (rt) std::memset( rt, 0, sizeof(TYPE));
	return rt;
}
template <typename TYPE>
static TYPE* add_list( TYPE* lst, TYPE* elem)
{
	if (!lst) return elem;
	TYPE* vv = lst;
	while (vv->next) vv=vv->next;
	vv->next = elem;
	return lst;
}

struct RequestContextList
{
	struct RequestContextList* next;
	const char* name;			/*< name of the context to address it as parent of a new context */
	papuga_RequestContext context;
};

struct papuga_RequestHandler
{
	RequestContextList* contexts;
	papuga_Allocator allocator;
	char allocator_membuf[ 1<<14];
};

extern "C" void papuga_init_RequestContext( papuga_RequestContext* self)
{
	papuga_init_Allocator( &self->allocator, self->allocator_membuf, sizeof( self->allocator_membuf));
	self->errcode = papuga_Ok;
	self->variables = NULL;
	self->acl = NULL;
}

extern "C" void papuga_destroy_RequestContext( papuga_RequestContext* self)
{
	papuga_destroy_Allocator( &self->allocator);
}

extern "C" papuga_ErrorCode papuga_RequestContext_last_error( const papuga_RequestContext* self)
{
	return self->errcode;
}

static bool RequestContext_add_variable( papuga_RequestContext* self, const char* name, papuga_ValueVariant* value, bool moveobj)
{
	papuga_RequestVariable* varstruct = alloc_type<papuga_RequestVariable>( &self->allocator);
	if (!varstruct) goto ERROR;
	varstruct->name = papuga_Allocator_copy_charp( &self->allocator, name);
	if (!varstruct->name) goto ERROR;
	if (!papuga_Allocator_deepcopy_value( &self->allocator, &varstruct->value, value, moveobj, &self->errcode)) goto ERROR;
	self->variables = add_list( self->variables, varstruct);
	return true;
ERROR:
	if (self->errcode == papuga_Ok)
	{
		self->errcode = papuga_NoMemError;
	}
	return false;
}

extern "C" bool papuga_RequestContext_add_variable( papuga_RequestContext* self, const char* name, papuga_ValueVariant* value)
{
	return RequestContext_add_variable( self, name, value, true);
}

extern "C" bool papuga_RequestContext_add_access( papuga_RequestContext* self, const char* role)
{
	papuga_RequestAcl* aclitem = alloc_type<papuga_RequestAcl>( &self->allocator);
	if (!aclitem) goto ERROR;
	aclitem->allowed_role = papuga_Allocator_copy_charp( &self->allocator, role);
	if (!aclitem->allowed_role) goto ERROR;
	self->acl = add_list( self->acl, aclitem);
	return true;
ERROR:
	if (self->errcode == papuga_Ok)
	{
		self->errcode = papuga_NoMemError;
	}
	return false;
}

extern "C" papuga_RequestHandler* papuga_create_RequestHandler()
{
	papuga_RequestHandler* rt = (papuga_RequestHandler*) std::malloc( sizeof(papuga_RequestHandler));
	if (!rt) return NULL;
	rt->contexts = NULL;
	papuga_init_Allocator( &rt->allocator, rt->allocator_membuf, sizeof(rt->allocator_membuf));
	return rt;
}

extern "C" void papuga_destroy_RequestHandler( papuga_RequestHandler* self)
{
	papuga_destroy_Allocator( &self->allocator);
	std::free( self);
}

extern "C" bool papuga_RequestHandler_add_context( papuga_RequestHandler* self, const char* name, papuga_RequestContext* ctx, papuga_ErrorCode* errcode)
{
	papuga_RequestAcl const* al;
	papuga_RequestVariable* vl;
	RequestContextList* listitem = alloc_type<RequestContextList>( &self->allocator);
	if (!name || !ctx)
	{
		*errcode = papuga_ValueUndefined;
		goto ERROR;
	}
	if (!listitem) goto ERROR;
	papuga_init_RequestContext( &listitem->context);
	listitem->name = papuga_Allocator_copy_charp( &self->allocator, name);
	if (!listitem->name) goto ERROR;
	al = ctx->acl;
	for (; al; al = al->next)
	{
		if (!papuga_RequestContext_allow_access( &listitem->context, al->allowed_role)) return false;
	}
	vl = ctx->variables;
	for (; vl; vl = vl->next)
	{
		if (vl->name[0] != '_')
		{
			// ... variables starting with an '_' are considered temporary
			if (!papuga_RequestContext_add_variable( &listitem->context, vl->name, &vl->value)) return false;
		}
	}
	self->contexts = add_list( self->contexts, listitem);
	return true;
ERROR:
	if (*errcode == papuga_Ok)
	{
		*errcode = papuga_NoMemError;
	}
	return false;
}

extern "C" bool papuga_init_RequestContext_child( papuga_RequestContext* self, const papuga_RequestHandler* handler, const char* parent, const char* role, papuga_ErrorCode* errcode)
{
	RequestContextList* cl;
	papuga_RequestVariable* vl;
	papuga_RequestAcl const* al;

	if (!handler || !parent || !role)
	{
		*errcode = papuga_ValueUndefined;
		goto ERROR;
	}
	cl = handler->contexts;
	for (; cl; cl=cl->next)
	{
		if (0==std::strcmp( parent, cl->name)) break;
	}
	if (!cl)
	{
		*errcode = papuga_AddressedItemNotFound;
		goto ERROR;
	}
	al = cl->context.acl;
	for (; al; al=al->next)
	{
		if (0==std::strcmp( role, al->allowed_role)) break;
	}
	if (!al)
	{
		*errcode = papuga_NotAllowed;
		goto ERROR;
	}
	papuga_init_RequestContext( self);
	al = cl->context.acl;
	for (; al; al = al->next)
	{
		if (!papuga_RequestContext_allow_access( self, al->allowed_role)) return false;
	}
	vl = cl->context.variables;
	for (; vl; vl = vl->next)
	{
		if (!RequestContext_add_variable( self, vl->name, &vl->value, false)) return false;
	}
	return true;
ERROR:
	if (*errcode == papuga_Ok)
	{
		*errcode = papuga_NoMemError;
	}
	return false;
}

