/*
 * Copyright (c) 2021 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/*
* \brief Structures for executing a request defined as Lua script, an empty stub is linked if Lua is not defined (WITH_LUA="NO")
* \file luaRequestHandler.h
*/
#ifndef _PAPUGA_LUA_REQUEST_HANDLER_H_INCLUDED
#define _PAPUGA_LUA_REQUEST_HANDLER_H_INCLUDED
#ifdef __cplusplus
extern "C" {
#endif
#include "papuga/typedefs.h"
#include "papuga/errors.h"
#include "papuga/requestContext.h"
#include "papuga/schema.h"

typedef struct papuga_LuaRequestHandlerScript papuga_LuaRequestHandlerScript;
typedef struct papuga_LuaRequestHandler papuga_LuaRequestHandler;

typedef const char* (*papuga_CreateTransaction)( void* self, const char* type, papuga_RequestContext* context, papuga_Allocator* allocator);
typedef bool (*papuga_DoneTransaction)( void* self);

typedef struct papuga_TransactionHandler
{
	void* self;
	papuga_CreateTransaction create;
	papuga_DoneTransaction done;
} papuga_TransactionHandler;

typedef struct papuga_RequestAttributes
{
	int accepted_encoding_set;
	int accepted_doctype_set;
	const char* html_head;
	const char* html_base_href;
	bool beautifiedOutput;
	bool deterministicOutput;
} papuga_RequestAttributes;

void papuga_init_RequestAttributes( papuga_RequestAttributes* dest, const char* http_accept, const char* html_head, const char* html_base_href, bool beautifiedOutput, bool deterministicOutput);
bool papuga_copy_RequestAttributes( papuga_Allocator* allocator, papuga_RequestAttributes* dest, papuga_RequestAttributes const* src);
papuga_ContentType papuga_http_default_doctype( papuga_RequestAttributes* attr);

papuga_LuaRequestHandlerScript* papuga_create_LuaRequestHandlerScript(
	const char* name,
	const char* source,
	papuga_ErrorBuffer* errbuf);

void papuga_destroy_LuaRequestHandlerScript( papuga_LuaRequestHandlerScript* self);

const char* papuga_LuaRequestHandlerScript_options( papuga_LuaRequestHandlerScript const* self);
const char* papuga_LuaRequestHandlerScript_name( papuga_LuaRequestHandlerScript const* self);

papuga_LuaRequestHandler* papuga_create_LuaRequestHandler(
	const papuga_LuaRequestHandlerScript* script,
	const papuga_SchemaMap* schemamap,
	papuga_RequestContextPool* contextpool,
	papuga_RequestContext* context,
	papuga_TransactionHandler* transactionHandler,
	const papuga_RequestAttributes* attributes,
	const char* requestmethod,
	const char* contextname,
	const char* requestpath,
	const char* contentstr,
	size_t contentlen,
	papuga_ErrorCode* errcode);

void papuga_destroy_LuaRequestHandler( papuga_LuaRequestHandler* self);

typedef struct papuga_DelegateRequest
{
	const char* requestmethod;
	const char* requesturl;
	const char* contentstr;
	size_t contentlen;
	const char* resultstr;
	size_t resultlen;
	papuga_ErrorCode errcode;
	const char* errmsg;
} papuga_DelegateRequest;

bool papuga_run_LuaRequestHandler( papuga_LuaRequestHandler* handler, papuga_ErrorBuffer* errbuf);

int papuga_LuaRequestHandler_nof_DelegateRequests( const papuga_LuaRequestHandler* handler);

papuga_DelegateRequest const* papuga_LuaRequestHandler_get_delegateRequest( const papuga_LuaRequestHandler* handler, int idx);

void papuga_LuaRequestHandler_init_result( papuga_LuaRequestHandler* handler, int idx, const char* resultstr, size_t resultlen);
void papuga_LuaRequestHandler_init_error( papuga_LuaRequestHandler* handler, int idx, papuga_ErrorCode errcode, const char* errmsg);

const char* papuga_LuaRequestHandler_get_result( const papuga_LuaRequestHandler* handler, size_t* resultlen);

#ifdef __cplusplus
}
#endif
#endif

