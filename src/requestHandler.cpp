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
#include "papuga/request.h"
#include "papuga/requestResult.h"
#include "papuga/classdef.h"
#include "papuga/allocator.h"
#include "papuga/serialization.h"
#include "papuga/valueVariant.h"
#include "papuga/errors.h"
#include "papuga/callResult.h"
#include <cstdlib>
#include <cstring>

/* @brief Hook for GETTEXT */
#define _TXT(x) x

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

static bool RequestContext_add_variable_shallow_copy( papuga_RequestContext* self, const char* name, papuga_ValueVariant* value)
{
	papuga_RequestVariable* varstruct = alloc_type<papuga_RequestVariable>( &self->allocator);
	if (!varstruct) return false;
	varstruct->name = papuga_Allocator_copy_charp( &self->allocator, name);
	if (!varstruct->name) return false;
	papuga_init_ValueVariant_copy( &varstruct->value, value);
	varstruct->inherited = false;
	self->variables = add_list( self->variables, varstruct);
	return true;
}

static bool RequestContext_add_variable( papuga_RequestContext* self, const char* name, papuga_ValueVariant* value, bool moveobj)
{
	papuga_RequestVariable* varstruct = alloc_type<papuga_RequestVariable>( &self->allocator);
	if (!varstruct) goto ERROR;
	varstruct->name = papuga_Allocator_copy_charp( &self->allocator, name);
	if (!varstruct->name) goto ERROR;
	if (!papuga_Allocator_deepcopy_value( &self->allocator, &varstruct->value, value, moveobj, &self->errcode)) goto ERROR;
	varstruct->inherited = false;
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
		cur->inherited = true;
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

const papuga_ValueVariant* papuga_RequestContext_get_variable( papuga_RequestContext* self, const char* name)
{
	papuga_RequestVariable* var = find_list( self->variables, &papuga_RequestVariable::name, name);
	return (var)?&var->value : NULL;
	
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
	RequestSchemaList* sl = find_list( self->schemas, &RequestSchemaList::name, name);
	if (!sl)
	{
		*errcode = papuga_AddressedItemNotFound;
		return false;
	}
	sl->acl = addRequestAcl( &self->allocator, sl->acl, role);
	if (!sl->acl)
	{
		*errcode = papuga_NoMemError;
		return false;
	}
	return true;
}

extern "C" const papuga_RequestAutomaton* papuga_RequestHandler_get_schema( papuga_RequestHandler* self, const char* name, const char* role, papuga_ErrorCode* errcode)
{
	const RequestSchemaList* sl = 
	sl = find_list( self->schemas, &RequestSchemaList::name, name);
	if (!sl)
	{
		*errcode = papuga_AddressedItemNotFound;
		return NULL;
	}
	if (!find_list( sl->acl, &papuga_RequestAcl::allowed_role, role))
	{
		*errcode = papuga_NotAllowed;
		return NULL;
	}
	return sl->automaton;
}

static void reportMethodCallError( papuga_ErrorBuffer* errorbuf, const papuga_Request* request, const papuga_RequestMethodCall* call, const char* msg)
{
	const papuga_ClassDef* classdef = papuga_Request_classdefs( request);
	const char* classname = classdef[ call->methodid.classid].name;
	if (call->methodid.functionid)
	{
		const char* methodname = classdef[ call->methodid.classid].methodnames[ call->methodid.functionid-1];
		if (call->argcnt >= 0)
		{
			papuga_ErrorBuffer_reportError( errorbuf, _TXT( "error resolving argument %d of the method %s->%s::%s: %s"), call->argcnt+1, call->selfvarname, classname, methodname, msg);
		}
		else
		{
			papuga_ErrorBuffer_reportError( errorbuf, _TXT( "error calling the method %s->%s::%s: %s"), call->selfvarname, classname, methodname, msg);
		}
	}
	else
	{
		if (call->argcnt >= 0)
		{
			papuga_ErrorBuffer_reportError( errorbuf, _TXT( "error resolving argument %d of the constructor of %s: %s"), call->argcnt+1, classname, msg);
		}
		else
		{
			papuga_ErrorBuffer_reportError( errorbuf, _TXT( "error calling the constructor of %s: %s"), classname, msg);
		}
	}
}

extern "C" bool papuga_RequestContext_execute_request( papuga_RequestContext* context, const papuga_Request* request, papuga_ErrorBuffer* errorbuf, int* errorpos)
{
	char membuf_err[ 256];
	papuga_ErrorBuffer errorbuf_call;

	const papuga_ClassDef* classdefs = papuga_Request_classdefs( request);
	papuga_RequestIterator* itr = papuga_create_RequestIterator( &context->allocator, request);
	if (!itr)
	{
		papuga_ErrorBuffer_reportError( errorbuf, _TXT("error handling request: %s"), papuga_ErrorCode_tostring( papuga_NoMemError));
		return false;
	}
	papuga_init_ErrorBuffer( &errorbuf_call, membuf_err, sizeof(membuf_err));
	const papuga_RequestMethodCall* call;
	while (!!(call = papuga_RequestIterator_next_call( itr, context->variables)))
	{
		if (call->methodid.functionid == 0)
		{
			// [1] Call the constructor
			const papuga_ClassConstructor func = classdefs[ call->methodid.classid-1].constructor;
			void* self;

			if (!(self=(*func)( &errorbuf_call, call->args.argc, call->args.argv)))
			{
				reportMethodCallError( errorbuf, request, call, papuga_ErrorBuffer_lastError( &errorbuf_call));
				*errorpos = call->eventcnt;
				return false;
			}
			papuga_HostObject* hobj = papuga_Allocator_alloc_HostObject( &context->allocator, call->methodid.classid, self, classdefs[ call->methodid.classid-1].destructor);
			papuga_ValueVariant result;
			papuga_init_ValueVariant_hostobj( &result, hobj);

			// [2] Assign the result to the result variable:
			papuga_RequestVariable* var = find_list( context->variables, &papuga_RequestVariable::name, call->resultvarname);
			if (var)
			{
				// ... overwrite if already defined
				papuga_init_ValueVariant_copy( &var->value, &result);
			}
			else
			{
				// ... add create it if not, because we get ownership of the allocator context of the call result, a shallow copy is enough
				if (!RequestContext_add_variable_shallow_copy( context, call->resultvarname, &result)) 
				{
					reportMethodCallError( errorbuf, request, call, papuga_ErrorCode_tostring( papuga_NoMemError));
					*errorpos = call->eventcnt;
					return false;
				}
			}
		}
		else
		{
			// [1] Get the method and the object of the method to call:
			const papuga_ClassMethod func = classdefs[ call->methodid.classid-1].methodtable[ call->methodid.functionid-1];
			papuga_RequestVariable* var = find_list( context->variables, &papuga_RequestVariable::name, call->selfvarname);
			if (!var)
			{
				reportMethodCallError( errorbuf, request, call, papuga_ErrorCode_tostring( papuga_MissingSelf));
				*errorpos = call->eventcnt;
				return false;
			}
			if (var->value.valuetype != papuga_TypeHostObject || var->value.value.hostObject->classid != call->methodid.classid)
			{
				reportMethodCallError( errorbuf, request, call, papuga_ErrorCode_tostring( papuga_TypeError));
				*errorpos = call->eventcnt;
				return false;
			}
			// [2] Call the method and report an error on failure:
			void* self = var->value.value.hostObject->data;
			papuga_CallResult retval;
			papuga_init_CallResult( &retval, 0, 0, membuf_err, sizeof(membuf_err));
			if (!(*func)( self, &retval, call->args.argc, call->args.argv))
			{
				reportMethodCallError( errorbuf, request, call, papuga_ErrorBuffer_lastError( &retval.errorbuf));
				*errorpos = call->eventcnt;
				return false;
			}
			// [3] Fetch the result(s) if required (stored as variable):
			if (call->resultvarname)
			{
				// [3.1] We takeover the allocator context of the called function 
				//	and build a shallow copy of the result:
				if (!papuga_Allocator_takeover( &context->allocator, &retval.allocator))
				{
					reportMethodCallError( errorbuf, request, call, papuga_ErrorCode_tostring( papuga_NoMemError));
					*errorpos = call->eventcnt;
					return false;
				}
				papuga_ValueVariant result;
				if (retval.nofvalues == 0)
				{
					papuga_init_ValueVariant( &result);
				}
				else if (retval.nofvalues == 1)
				{
					papuga_init_ValueVariant_copy( &result, &retval.valuear[0]);
				}
				else
				{
					// ... handle multiple return values as a serialization:
					int vi = 0, ve = retval.nofvalues;
					papuga_Serialization* ser = papuga_Allocator_alloc_Serialization( &context->allocator);
					bool sc = true;
					if (ser)
					{
						sc &= papuga_Serialization_pushOpen( ser);
						for (; vi < ve; ++vi)
						{
							sc &= papuga_Serialization_pushValue( ser, retval.valuear + vi);
						}
						sc &= papuga_Serialization_pushClose( ser);
					}
					if (!ser || !sc)
					{
						reportMethodCallError( errorbuf, request, call, papuga_ErrorCode_tostring( papuga_NoMemError));
						*errorpos = call->eventcnt;
						return false;
					}
				}
				// [3.2] Assign the result to the result variable:
				var = find_list( context->variables, &papuga_RequestVariable::name, call->resultvarname);
				if (var)
				{
					// ... overwrite if already defined
					papuga_init_ValueVariant_copy( &var->value, &result);
				}
				else
				{
					// ... add create it if not, because we get ownership of the allocator context of the call result, a shallow copy is enough
					if (!RequestContext_add_variable_shallow_copy( context, call->resultvarname, &result)) 
					{
						reportMethodCallError( errorbuf, request, call, papuga_ErrorCode_tostring( papuga_NoMemError));
						*errorpos = call->eventcnt;
						return false;
					}
				}
			}
			else
			{
				// ... not requested, then destroy
				papuga_destroy_CallResult( &retval);
			}
		}
	}
	// Report error if we could not resolve all parts of a method call:
	papuga_ErrorCode errcode = papuga_RequestIterator_get_last_error( itr, &call);
	if (errcode != papuga_Ok)
	{
		reportMethodCallError( errorbuf, request, call, papuga_ErrorCode_tostring( errcode));
		*errorpos = call->eventcnt;
		return false;
	}
	return true;
}

extern "C" bool papuga_set_RequestResult( papuga_RequestResult* self, papuga_RequestContext* context, const papuga_Request* request)
{
	self->name = papuga_Request_resultname( request);
	self->structdefs = papuga_Request_struct_descriptions( request);
	self->nodes = NULL;
	papuga_RequestResultNode rootnode;
	rootnode.next = NULL;
	papuga_RequestResultNode* curnode = &rootnode;
	papuga_RequestVariable const* vi = context->variables;
	for (; vi; vi = vi->next)
	{
		if (vi->name[0] == '_' || vi->inherited || vi->value.valuetype == papuga_TypeHostObject) continue;
		curnode->next = alloc_type<papuga_RequestResultNode>( &context->allocator);
		curnode = curnode->next;
		if (!curnode) return false;
		curnode->next = NULL;
		curnode->name = vi->name;
		papuga_init_ValueVariant_copy( &curnode->value, &vi->value);
	}
	self->nodes = rootnode.next;
	return true;
}


