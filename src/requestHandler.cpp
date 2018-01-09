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
template <typename TYPE>
static TYPE* find_list( TYPE* lst, const char* TYPE::*member, const char* name)
{
	TYPE* ll = lst;
	for (; ll && 0!=std::strcmp( ll->*member, name); ll = ll->next){}
	return ll;
}
template <typename TYPE>
static const TYPE* find_list( const TYPE* lst, const char* TYPE::*member, const char* name)
{
	TYPE const* ll = lst;
	for (; ll && 0!=std::strcmp( ll->*member, name); ll = ll->next){}
	return ll;
}

struct RequestContextList
{
	struct RequestContextList* next;
	const char* name;			/*< name of the context to address it as parent of a new context */
	papuga_RequestContext context;
};

struct papuga_RequestAcl
{
	struct papuga_RequestAcl* next;			/*< next role allowed */
	const char* allowed_role;			/*< role allowed for accessing this item */
};

struct RequestSchemaList
{
	struct RequestSchemaList* next;
	const char* name;				/*< name of the context to address it as parent of a new context */
	papuga_RequestAcl* acl;				/*< access control list */
	papuga_RequestAutomaton* automaton;		/*< automaton */
};

struct papuga_RequestVariable
{
	struct papuga_RequestVariable* next;		/*< next variable */
	const char* name;				/*< name of variable associated with this value */
	papuga_ValueVariant value;			/*< variable value associated with this name */
};

struct papuga_RequestHandler
{
	RequestContextList* contexts;
	RequestSchemaList* schemas;
	papuga_Allocator allocator;
	char allocator_membuf[ 1<<14];
};

static papuga_RequestAcl* addRequestAcl( papuga_Allocator* allocator, papuga_RequestAcl* acl, const char* role)
{
	papuga_RequestAcl* aclitem = alloc_type<papuga_RequestAcl>( allocator);
	if (!aclitem) return NULL;
	aclitem->allowed_role = papuga_Allocator_copy_charp( allocator, role);
	if (!aclitem->allowed_role) return NULL;
	return add_list( acl, aclitem);
}

static papuga_RequestAcl* copyRequestAcl( papuga_Allocator* allocator, const papuga_RequestAcl* acl)
{
	papuga_RequestAcl root;
	root.next = 0;
	papuga_RequestAcl* cur = &root;
	papuga_RequestAcl const* al = acl;
	for (; al != 0; al = al->next)
	{
		cur->next = alloc_type<papuga_RequestAcl>( allocator);
		cur = cur->next;
		if (!cur) return NULL;
		cur->allowed_role = papuga_Allocator_copy_charp( allocator, al->allowed_role);
		cur->next = 0;
		if (!cur->allowed_role) return NULL;
	}
	return root.next;
}

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

static papuga_RequestVariable* copyRequestVariables( papuga_Allocator* allocator, papuga_RequestVariable* variables, bool moveobj, papuga_ErrorCode* errcode)
{
	papuga_RequestVariable root;
	root.next = NULL;
	papuga_RequestVariable* cur = &root;
	papuga_RequestVariable* vl = variables;
	for (; vl != 0; vl = vl->next)
	{
		cur->next = alloc_type<papuga_RequestVariable>( allocator);
		cur = cur->next;
		if (!cur) goto ERROR;
		cur->name = papuga_Allocator_copy_charp( allocator, vl->name);
		if (!cur->name) goto ERROR;
		cur->next = 0;
		if (!papuga_Allocator_deepcopy_value( allocator, &cur->value, &vl->value, moveobj, errcode)) goto ERROR;
	}
	return root.next;
ERROR:
	if (*errcode == papuga_Ok)
	{
		*errcode = papuga_NoMemError;
	}
	return NULL;
}

extern "C" bool papuga_RequestContext_add_variable( papuga_RequestContext* self, const char* name, papuga_ValueVariant* value)
{
	return RequestContext_add_variable( self, name, value, true);
}

extern "C" bool papuga_RequestContext_allow_access( papuga_RequestContext* self, const char* role)
{
	self->acl = addRequestAcl( &self->allocator, self->acl, role);
	return (!!self->acl);
}

extern "C" papuga_RequestHandler* papuga_create_RequestHandler()
{
	papuga_RequestHandler* rt = (papuga_RequestHandler*) std::malloc( sizeof(papuga_RequestHandler));
	if (!rt) return NULL;
	rt->contexts = NULL;
	rt->schemas = NULL;
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
	RequestContextList* listitem = alloc_type<RequestContextList>( &self->allocator);
	if (!listitem) goto ERROR;
	papuga_init_RequestContext( &listitem->context);
	listitem->name = papuga_Allocator_copy_charp( &self->allocator, name);
	listitem->context.acl = copyRequestAcl( &listitem->context.allocator, ctx->acl);
	listitem->context.variables = copyRequestVariables( &listitem->context.allocator, ctx->variables, true, errcode);
	if (!listitem->name || listitem->context.acl || !listitem->context.variables) goto ERROR;
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
	if (!parent || !role)
	{
		*errcode = papuga_ValueUndefined;
		goto ERROR;
	}
	cl = find_list( handler->contexts, &RequestContextList::name, parent);
	if (!cl)
	{
		*errcode = papuga_AddressedItemNotFound;
		goto ERROR;
	}
	if (!find_list( cl->context.acl, &papuga_RequestAcl::allowed_role, role))
	{
		*errcode = papuga_NotAllowed;
		goto ERROR;
	}
	papuga_init_RequestContext( self);
	self->acl = copyRequestAcl( &self->allocator, cl->context.acl);
	self->variables = copyRequestVariables( &self->allocator, cl->context.variables, false, errcode);
	if (!self->variables || !self->acl) goto ERROR;
	return true;
ERROR:
	if (*errcode == papuga_Ok)
	{
		*errcode = papuga_NoMemError;
	}
	return false;
}

extern "C" bool papuga_RequestHandler_add_schema( papuga_RequestHandler* self, const char* name, papuga_RequestAutomaton* automaton)
{
	RequestSchemaList* listitem = alloc_type<RequestSchemaList>( &self->allocator);
	if (!listitem) return false;
	listitem->name = papuga_Allocator_copy_charp( &self->allocator, name);
	if (!listitem->name) return false;
	listitem->automaton = automaton;
	self->schemas = add_list( self->schemas, listitem);
	return true;
}

extern "C" bool papuga_RequestHandler_schema_allow_access( papuga_RequestHandler* self, const char* name, const char* role, papuga_ErrorCode* errcode)
{
	RequestSchemaList* cl;
	cl = find_list( self->schemas, &RequestSchemaList::name, name);
	if (!cl)
	{
		*errcode = papuga_AddressedItemNotFound;
		return false;
	}
	cl->acl = addRequestAcl( &self->allocator, cl->acl, role);
	if (!cl->acl)
	{
		*errcode = papuga_NoMemError;
		return false;
	}
	return true;
}

extern "C" bool papuga_RequestContext_execute_request( papuga_RequestContext* context, papuga_Request* request)
{
	
}

