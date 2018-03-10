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
template <typename TYPE, typename MEMBERTYPE>
static void foreach_list( TYPE* lst, MEMBERTYPE TYPE::*member, void (*action)( MEMBERTYPE* self))
{
	for (; lst; lst = lst->next) (*action)( &(lst->*member));
}

struct RequestContextList
{
	struct RequestContextList* next;
	const char* name;				/*< name of the context to address it as parent of a new context */
	papuga_RequestContext context;
};

struct RequestSchemeList
{
	struct RequestSchemeList* next;
	const char* type;				/*< type name of the context this scheme is valid for */
	const char* name;				/*< name of the scheme */
	const papuga_RequestAutomaton* automaton;	/*< automaton of the scheme */
};

struct papuga_RequestHandler
{
	RequestContextList* contexts;
	RequestSchemeList* schemes;
	papuga_RequestLogger* logger;
	papuga_Allocator allocator;
	char allocator_membuf[ 1<<14];
};

extern "C" void papuga_init_RequestContext( papuga_RequestContext* self, papuga_Allocator* allocator, papuga_RequestLogger* logger)
{
	self->allocator = allocator;
	self->errcode = papuga_Ok;
	self->variables = NULL;
	self->logger = logger;
	self->type = "";
}

extern "C" papuga_ErrorCode papuga_RequestContext_last_error( const papuga_RequestContext* self)
{
	return self->errcode;
}

static bool RequestContext_add_variable_shallow_copy( papuga_RequestContext* self, const char* name, papuga_ValueVariant* value)
{
	papuga_RequestVariable* varstruct = alloc_type<papuga_RequestVariable>( self->allocator);
	if (!varstruct) return false;
	varstruct->name = papuga_Allocator_copy_charp( self->allocator, name);
	if (!varstruct->name) return false;
	papuga_init_ValueVariant_value( &varstruct->value, value);
	varstruct->inheritcnt = 0;
	self->variables = add_list( self->variables, varstruct);
	return true;
}

static bool RequestContext_add_variable( papuga_RequestContext* self, const char* name, papuga_ValueVariant* value, bool moveobj)
{
	papuga_RequestVariable* varstruct = alloc_type<papuga_RequestVariable>( self->allocator);
	if (!varstruct) goto ERROR;
	varstruct->name = papuga_Allocator_copy_charp( self->allocator, name);
	if (!varstruct->name) goto ERROR;
	if (!papuga_Allocator_deepcopy_value( self->allocator, &varstruct->value, value, moveobj, &self->errcode)) goto ERROR;
	varstruct->inheritcnt = 0;
	self->variables = add_list( self->variables, varstruct);
	return true;
ERROR:
	if (self->errcode == papuga_Ok)
	{
		self->errcode = papuga_NoMemError;
	}
	return false;
}

static papuga_RequestVariable* copyRequestVariables( papuga_Allocator* allocator, papuga_RequestVariable* variables, int inheritincr, bool moveobj, papuga_ErrorCode* errcode)
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
		cur->inheritcnt = vl->inheritcnt + inheritincr;
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

extern "C" const papuga_ValueVariant* papuga_RequestContext_get_variable( const papuga_RequestContext* self, const char* name)
{
	const papuga_RequestVariable* var = find_list( self->variables, &papuga_RequestVariable::name, name);
	return (var)?&var->value : NULL;
}

extern "C" const char** papuga_RequestContext_list_variables( const papuga_RequestContext* self, int max_inheritcnt, char const** buf, size_t bufsize)
{
	size_t bufpos = 0;
	papuga_RequestVariable const* vl = self->variables;
	for (; vl; vl = vl->next)
	{
		if (bufpos >= bufsize) return NULL;
		if (max_inheritcnt >= 0 && vl->inheritcnt > max_inheritcnt) continue;
		buf[ bufpos++] = vl->name;
	}
	if (bufpos >= bufsize) return NULL;
	buf[ bufpos] = NULL;
	return buf;
}

extern "C" papuga_RequestHandler* papuga_create_RequestHandler( papuga_RequestLogger* logger)
{
	papuga_RequestHandler* rt = (papuga_RequestHandler*) std::malloc( sizeof(papuga_RequestHandler));
	if (!rt) return NULL;
	rt->contexts = NULL;
	rt->schemes = NULL;
	rt->logger = logger;
	papuga_init_Allocator( &rt->allocator, rt->allocator_membuf, sizeof(rt->allocator_membuf));
	return rt;
}

extern "C" void papuga_destroy_RequestHandler( papuga_RequestHandler* self)
{
	papuga_destroy_Allocator( &self->allocator);
	std::free( self);
}

extern "C" bool papuga_RequestHandler_add_context( papuga_RequestHandler* self, const char* type, const char* name, papuga_RequestContext* ctx, papuga_ErrorCode* errcode)
{
	RequestContextList* listitem = alloc_type<RequestContextList>( &self->allocator);
	if (!listitem) goto ERROR;
	papuga_init_RequestContext( &listitem->context, &self->allocator, self->logger);
	listitem->context.type = papuga_Allocator_copy_charp( &self->allocator, type);
	listitem->name = papuga_Allocator_copy_charp( &self->allocator, name);
	listitem->context.variables = copyRequestVariables( &self->allocator, ctx->variables, 0, true, errcode);
	if (!listitem->name
		|| (!listitem->context.variables && ctx->variables)
		|| (!listitem->context.type && ctx->type)) goto ERROR;
	self->contexts = add_list( self->contexts, listitem);
	return true;
ERROR:
	if (*errcode == papuga_Ok)
	{
		*errcode = papuga_NoMemError;
	}
	return false;
}

extern "C" const char** papuga_RequestHandler_list_contexts( const papuga_RequestHandler* self, const char* type, char const** buf, size_t bufsize)
{
	size_t bufpos = 0;
	RequestContextList const* cl = self->contexts;
	for (; cl; cl = cl->next)
	{
		if (!type || 0==std::strcmp(type, cl->context.type))
		{
			if (bufpos >= bufsize) return NULL;
			buf[ bufpos++] = cl->name;
		}
	}
	if (bufpos >= bufsize) return NULL;
	buf[ bufpos] = NULL;
	return buf;
}

extern "C" const char** papuga_RequestHandler_list_context_types( const papuga_RequestHandler* self, char const** buf, size_t bufsize)
{
	size_t bufpos = 0;
	RequestContextList const* cl = self->contexts;
	for (; cl; cl = cl->next)
	{
		size_t bi = 0;
		for (; bi < bufpos && !!std::strcmp(buf[bi],cl->context.type); ++bi){}
		if (bi < bufpos) continue;
		if (bufpos >= bufsize) return NULL;
		buf[ bufpos++] = cl->context.type;
	}
	if (bufpos >= bufsize) return NULL;
	buf[ bufpos] = NULL;
	return buf;
}

static const papuga_RequestContext* find_context( const RequestContextList* clst, const char* type, const char* name)
{
	RequestContextList const* cl = clst;
	for (; cl && (0!=std::strcmp( cl->context.type, type) || 0!=std::strcmp( cl->name, name)); cl = cl->next){}
	return cl ? &cl->context : NULL;
}

extern "C" bool papuga_init_RequestContext_child( papuga_RequestContext* self, papuga_Allocator* allocator, const papuga_RequestHandler* handler, const char* type, const char* name, papuga_ErrorCode* errcode)
{
	const papuga_RequestContext* parent_context;
	if (type && name)
	{
		parent_context = find_context( handler->contexts, type, name);
		if (!parent_context)
		{
			*errcode = papuga_AddressedItemNotFound;
			goto ERROR;
		}
		papuga_init_RequestContext( self, allocator, handler->logger);
		self->variables = copyRequestVariables( self->allocator, parent_context->variables, 1, false, errcode);
		self->type = papuga_Allocator_copy_charp( self->allocator, parent_context->type);
		if (!self->variables || !self->type) goto ERROR;
	}
	else if (!type && !name)
	{
		papuga_init_RequestContext( self, allocator, handler->logger);
	}
	else
	{
		*errcode = papuga_AddressedItemNotFound;
		goto ERROR;
	}
	return true;
ERROR:
	if (*errcode == papuga_Ok)
	{
		*errcode = papuga_NoMemError;
	}
	return false;
}

extern "C" const papuga_RequestContext* papuga_RequestHandler_find_context( const papuga_RequestHandler* handler, const char* type, const char* name)
{
	if (type && name)
	{
		return find_context( handler->contexts, type, name);
	}
	return NULL;
}

extern "C" bool papuga_RequestHandler_add_scheme( papuga_RequestHandler* self, const char* type, const char* name, const papuga_RequestAutomaton* automaton)
{
	RequestSchemeList* listitem = alloc_type<RequestSchemeList>( &self->allocator);
	if (!listitem) return false;
	listitem->type = papuga_Allocator_copy_charp( &self->allocator, type);
	listitem->name = papuga_Allocator_copy_charp( &self->allocator, name);
	if (!listitem->type || !listitem->name) return false;
	listitem->automaton = automaton;
	self->schemes = add_list( self->schemes, listitem);
	return true;
}

static RequestSchemeList* find_scheme( RequestSchemeList* ll, const char* type, const char* name)
{
	for (; ll && (0!=std::strcmp( ll->name, name) || 0!=std::strcmp( ll->type, type)); ll = ll->next){}
	return ll;
}

extern "C" bool papuga_RequestHandler_has_scheme( const papuga_RequestHandler* self, const char* contextType, const char* scheme)
{
	return !!find_scheme( self->schemes, contextType, scheme);
}

static const char** RequestHandler_list_all_schemes( const papuga_RequestHandler* self, char const** buf, size_t bufsize)
{
	size_t bufpos = 0;
	RequestSchemeList const* sl = self->schemes;
	for (; sl; sl = sl->next)
	{
		size_t bi = 0;
		for (; bi < bufpos && !!std::strcmp(buf[bi],sl->type); ++bi){}
		if (bi < bufpos) continue;
		if (bufpos >= bufsize) return NULL;
		buf[ bufpos++] = sl->name;
	}
	if (bufpos >= bufsize) return NULL;
	buf[ bufpos] = NULL;
	return buf;
}

extern "C" const char** papuga_RequestHandler_list_schemes( const papuga_RequestHandler* self, const char* type, char const** buf, size_t bufsize)
{
	size_t bufpos = 0;
	RequestSchemeList const* sl = self->schemes;

	if (!type)
	{
		return RequestHandler_list_all_schemes( self, buf, bufsize);
	}
	for (; sl; sl = sl->next)
	{
		if (!type || 0==std::strcmp(type, sl->type))
		{
			if (bufpos >= bufsize) return NULL;
			buf[ bufpos++] = sl->name;
		}
	}
	if (bufpos >= bufsize) return NULL;
	buf[ bufpos] = NULL;
	return buf;
}

extern "C" const papuga_RequestAutomaton* papuga_RequestHandler_get_scheme( const papuga_RequestHandler* self, const char* type, const char* name, papuga_ErrorCode* errcode)
{
	const RequestSchemeList* sl = find_scheme( self->schemes, type?type:"", name);
	if (!sl)
	{
		*errcode = papuga_AddressedItemNotFound;
		return NULL;
	}
	return sl->automaton;
}

static void reportMethodCallError( papuga_ErrorBuffer* errorbuf, const papuga_Request* request, const papuga_RequestMethodCall* call, const char* msg)
{
	const papuga_ClassDef* classdef = papuga_Request_classdefs( request);
	const char* classname = classdef[ call->methodid.classid-1].name;
	if (call->methodid.functionid)
	{
		const char* methodname = classdef[ call->methodid.classid-1].methodnames[ call->methodid.functionid-1];
		if (call->argcnt >= 0)
		{
			papuga_ErrorBuffer_reportError( errorbuf, _TXT( "error resolving argument %d of the method %s->%s::%s: %s"), call->argcnt+1, call->selfvarname, classname, methodname, msg);
		}
		else
		{
			papuga_ErrorBuffer_reportError( errorbuf, _TXT( "error for object '%s': %s"), call->selfvarname, msg);
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
			papuga_ErrorBuffer_reportError( errorbuf, _TXT( "error creating '%s': %s"), call->resultvarname, msg);
		}
	}
}

static bool RequestContext_add_result( papuga_RequestContext* context, const char* resultvarname, papuga_ValueVariant& resultvalue, bool append, papuga_ErrorCode& errcode)
{
	papuga_RequestVariable* var = find_list( context->variables, &papuga_RequestVariable::name, resultvarname);
	if (var)
	{
		if (append)
		{
			if (var->value.valuetype == papuga_TypeSerialization)
			{
				return papuga_Serialization_pushValue( var->value.value.serialization, &resultvalue);
			}
			else
			{
				errcode = papuga_MixedConstruction;
				return false;
			}
		}
		else
		{
			// ... overwrite if already defined
			papuga_init_ValueVariant_value( &var->value, &resultvalue);
			return true;
		}
	}
	else
	{
		if (append)
		{
			papuga_Serialization* ser = papuga_Allocator_alloc_Serialization( context->allocator);
			if (!ser || !papuga_Serialization_pushValue( ser, &resultvalue))
			{
				errcode = papuga_NoMemError;
				return false;
			}
			papuga_ValueVariant rv;
			papuga_init_ValueVariant_serialization( &rv, ser);
			return RequestContext_add_variable_shallow_copy( context, resultvarname, &rv);
			// ... shallow copy, because we get ownership of the allocator context of the call result
		}
		else
		{
			return RequestContext_add_variable_shallow_copy( context, resultvarname, &resultvalue);
			// ... shallow copy, because we get ownership of the allocator context of the call result
		}
	}
}

extern "C" bool papuga_RequestContext_execute_request( papuga_RequestContext* context, const papuga_Request* request, papuga_ErrorBuffer* errorbuf, int* errorpos)
{
	char membuf_err[ 1024];
	papuga_ErrorBuffer errorbuf_call;
	papuga_ErrorCode errcode = papuga_Ok;

	const papuga_ClassDef* classdefs = papuga_Request_classdefs( request);
	papuga_RequestIterator* itr = papuga_create_RequestIterator( context->allocator, request);
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
				if (context->logger->logMethodCall)
				{
					(*context->logger->logMethodCall)( context->logger->self, 3,
						papuga_LogItemClassName, classdefs[ call->methodid.classid-1].name,
						papuga_LogItemArgc, call->args.argc,
						papuga_LogItemArgv, &call->args.argv[0]);
				}
				reportMethodCallError( errorbuf, request, call, papuga_ErrorBuffer_lastError( &errorbuf_call));
				*errorpos = call->eventcnt;
				papuga_destroy_RequestIterator( itr);
				return false;
			}
			papuga_HostObject* hobj = papuga_Allocator_alloc_HostObject( context->allocator, call->methodid.classid, self, classdefs[ call->methodid.classid-1].destructor);
			papuga_ValueVariant result;
			papuga_init_ValueVariant_hostobj( &result, hobj);

			// [2] Assign the result to the result variable:
			if (!RequestContext_add_result( context, call->resultvarname, result, call->appendresult, errcode))
			{
				break;
			}

			// [3] Log the call:
			if (context->logger->logMethodCall)
			{
				(*context->logger->logMethodCall)( context->logger->self, 4,
					papuga_LogItemClassName, classdefs[ call->methodid.classid-1].name,
					papuga_LogItemArgc, call->args.argc,
					papuga_LogItemArgv, &call->args.argv[0],
					papuga_LogItemResult, &result);
			}
		}
		else
		{
			// [1] Get the method and the object of the method to call:
			const papuga_ClassMethod func = classdefs[ call->methodid.classid-1].methodtable[ call->methodid.functionid-1];
			papuga_RequestVariable* var = find_list( context->variables, &papuga_RequestVariable::name, call->selfvarname);
			if (!var)
			{
				errcode = papuga_MissingSelf;
				break;
			}
			if (var->value.valuetype != papuga_TypeHostObject || var->value.value.hostObject->classid != call->methodid.classid)
			{
				errcode = papuga_TypeError;
				break;
			}
			// [2] Call the method and report an error on failure:
			void* self = var->value.value.hostObject->data;
			papuga_CallResult retval;
			papuga_init_CallResult( &retval, context->allocator, false, membuf_err, sizeof(membuf_err));
			if (!(*func)( self, &retval, call->args.argc, call->args.argv))
			{
				if (context->logger->logMethodCall)
				{
					(*context->logger->logMethodCall)( context->logger->self, 4,
							papuga_LogItemClassName, classdefs[ call->methodid.classid-1].name,
							papuga_LogItemMethodName, classdefs[ call->methodid.classid-1].methodnames[ call->methodid.functionid-1],
							papuga_LogItemArgc, call->args.argc,
							papuga_LogItemArgv, &call->args.argv[0]);
				}
				reportMethodCallError( errorbuf, request, call, papuga_ErrorBuffer_lastError( &retval.errorbuf));
				*errorpos = call->eventcnt;
				papuga_destroy_RequestIterator( itr);
				return false;
			}
			// [3] Fetch the result(s) if required (stored as variable):
			if (call->resultvarname)
			{
				// [3.1] We build a shallow copy of the result, because the allocator of the result is the one of the context:
				papuga_ValueVariant result;
				if (retval.nofvalues == 0)
				{
					papuga_init_ValueVariant( &result);
				}
				else if (retval.nofvalues == 1)
				{
					papuga_init_ValueVariant_value( &result, &retval.valuear[0]);
				}
				else
				{
					// ... handle multiple return values as a serialization:
					int vi = 0, ve = retval.nofvalues;
					papuga_Serialization* ser = papuga_Allocator_alloc_Serialization( context->allocator);
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
						errcode = papuga_NoMemError;
						break;
					}
					papuga_init_ValueVariant_serialization( &result, ser);
				}
				// [3.2] Assign the result to the result variable:
				if (!RequestContext_add_result( context, call->resultvarname, result, call->appendresult, errcode))
				{
					break;
				}
				// [3.3] Log the call
				if (context->logger->logMethodCall)
				{
					(*context->logger->logMethodCall)( context->logger->self, 5, 
							papuga_LogItemClassName, classdefs[ call->methodid.classid-1].name,
							papuga_LogItemMethodName, classdefs[ call->methodid.classid-1].methodnames[ call->methodid.functionid-1],
							papuga_LogItemArgc, call->args.argc,
							papuga_LogItemArgv, &call->args.argv[0],
							papuga_LogItemResult, &result);
				}
			}
			else
			{
				// Log the call
				if (context->logger->logMethodCall)
				{
					(*context->logger->logMethodCall)( context->logger->self, 4, 
							papuga_LogItemClassName, classdefs[ call->methodid.classid-1].name,
							papuga_LogItemMethodName, classdefs[ call->methodid.classid-1].methodnames[ call->methodid.functionid-1],
							papuga_LogItemArgc, call->args.argc,
							papuga_LogItemArgv, &call->args.argv[0]);
				}
				// ... not requested, then destroy
				papuga_destroy_CallResult( &retval);
			}
		}
	}
	// Report error if we could not resolve all parts of a method call:
	if (errcode == papuga_Ok)
	{
		errcode = papuga_RequestIterator_get_last_error( itr, &call);
	}
	if (errcode != papuga_Ok)
	{
		reportMethodCallError( errorbuf, request, call, papuga_ErrorCode_tostring( errcode));
		*errorpos = call->eventcnt;
		papuga_destroy_RequestIterator( itr);
		return false;
	}
	papuga_destroy_RequestIterator( itr);
	return true;
}

extern "C" bool papuga_set_RequestResult( papuga_RequestResult* self, papuga_RequestContext* context, const papuga_Request* request)
{
	self->allocator = context->allocator;
	self->name = papuga_Request_resultname( request);
	self->structdefs = papuga_Request_struct_descriptions( request);
	self->nodes = NULL;
	papuga_RequestResultNode rootnode;
	rootnode.next = NULL;
	papuga_RequestResultNode* curnode = &rootnode;
	papuga_RequestVariable const* vi = context->variables;
	for (; vi; vi = vi->next)
	{
		if (vi->name[0] == '_' || vi->inheritcnt > 0 || vi->value.valuetype == papuga_TypeHostObject) continue;
		curnode->next = alloc_type<papuga_RequestResultNode>( self->allocator);
		curnode = curnode->next;
		if (!curnode) return false;
		curnode->next = NULL;
		curnode->name = vi->name;
		curnode->name_optional = false;
		papuga_init_ValueVariant_value( &curnode->value, &vi->value);
	}
	self->nodes = rootnode.next;
	return true;
}


