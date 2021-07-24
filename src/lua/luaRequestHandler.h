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

typedef struct papuga_LuaRequestHandlerFunction papuga_LuaRequestHandlerFunction;
typedef struct papuga_LuaRequestHandler papuga_LuaRequestHandler;

papuga_LuaRequestHandlerFunction* papuga_create_LuaRequestHandlerFunction(
	const char* functionName,
	const char* source,
	papuga_ErrorBuffer* errcode);

void papuga_delete_LuaRequestHandlerFunction( papuga_LuaRequestHandlerFunction* self);

papuga_LuaRequestHandler* papuga_create_LuaRequestHandler(
	const papuga_LuaRequestHandlerFunction* function,
	papuga_RequestContext* context,
	const papuga_Serialization* input,
	papuga_ErrorCode* errcode);

typedef struct papuga_DelegateRequest
{
	const char* name;
	const char* schema;
	const char* requestmethod;
	const char* address;
	const char* path;
	papuga_Serialization serialization;
} papuga_DelegateRequest;


bool papuga_run_LuaRequestHandler( papuga_LuaRequestHandler* handler, papuga_ErrorCode* errcode);

papuga_DelegateRequest const* papuga_LuaRequestHandler_get_delegateRequests( const papuga_LuaRequestHandler* handler);

void papuga_LuaRequestHandler_init_answer( const papuga_LuaRequestHandler* handler, int idx, const papuga_Serialization* output);

papuga_Serialization* papuga_LuaRequestHandler_get_result( const papuga_LuaRequestHandler* handler);

#ifdef __cplusplus
}
#endif
#endif

