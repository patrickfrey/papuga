/*
 * Copyright (c) 2021 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/*
* \brief Structures for executing a request defined as Lua script
* \file luaRequestHandler.h
*/
#ifndef _PAPUGA_LUA_REQUEST_HANDLER_H_INCLUDED
#define _PAPUGA_LUA_REQUEST_HANDLER_H_INCLUDED
#ifdef __cplusplus
extern "C" {
#endif
#include "papuga/typedefs.h"
#include "papuga/classdef.h"
#include "papuga/errors.h"
#include "papuga/requestHandler.h"
#include "papuga/schema.h"

#ifndef _PAPUGA_LUA_DEV_LIB_H_INCLUDED
typedef struct papuga_lua_ClassEntryMap papuga_lua_ClassEntryMap;
#endif

typedef struct papuga_LuaRequestHandlerFunction papuga_LuaRequestHandlerFunction;
typedef struct papuga_LuaRequestHandler papuga_LuaRequestHandler;

papuga_LuaRequestHandlerFunction* papuga_create_LuaRequestHandlerFunction(
	const char* functionName,
	const char* source,
	papuga_ErrorBuffer* errbuf);

void papuga_delete_LuaRequestHandlerFunction( papuga_LuaRequestHandlerFunction* self);

papuga_LuaRequestHandler* papuga_create_LuaRequestHandler(
	const papuga_LuaRequestHandlerFunction* function,
	const papuga_lua_ClassEntryMap* cemap,
	const papuga_SchemaMap* schemamap,
	papuga_RequestContext* context,
	const char* contentstr,
	size_t contentlen,
	papuga_ErrorCode* errcode);

void papuga_delete_LuaRequestHandler( papuga_LuaRequestHandler* self);

typedef struct papuga_DelegateRequest
{
	const char* requestmethod;
	const char* url;
	const char* contentstr;
	size_t contentlen;
	const char* resultstr;
	size_t resultlen;
	papuga_ErrorCode errcode;
} papuga_DelegateRequest;

bool papuga_run_LuaRequestHandler( papuga_LuaRequestHandler* handler, papuga_ErrorBuffer* errbuf);

int papuga_LuaRequestHandler_nof_DelegateRequests( const papuga_LuaRequestHandler* handler);

papuga_DelegateRequest const* papuga_LuaRequestHandler_get_delegateRequest( const papuga_LuaRequestHandler* handler, int idx);

void papuga_LuaRequestHandler_init_answer( papuga_LuaRequestHandler* handler, int idx, const char* resultstr, size_t resultlen);
void papuga_LuaRequestHandler_init_error( papuga_LuaRequestHandler* handler, int idx, papuga_ErrorCode errcode);

papuga_Serialization* papuga_LuaRequestHandler_get_result( const papuga_LuaRequestHandler* handler);

#ifdef __cplusplus
}
#endif
#endif

