/*
 * Copyright (c) 2021 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/*
* \brief Empty stub (if Lua support is not configured) for structures for executing a request defined as Lua script
* \file luaRequestHandler_stub.cpp
*/
#include "luaRequestHandler.h"

papuga_LuaRequestHandlerFunction* papuga_create_LuaRequestHandlerFunction(
	const char* functionName,
	const char* source,
	papuga_ErrorBuffer* errbuf)
{
	papuga_ErrorBuffer_reportError( errbuf, papuga_ErrorCode_tostring( papuga_NotImplemented));
	return 0;
}

void papuga_delete_LuaRequestHandlerFunction( papuga_LuaRequestHandlerFunction* self)
{}

papuga_LuaRequestHandler* papuga_create_LuaRequestHandler(
	const papuga_LuaRequestHandlerFunction* function,
	papuga_RequestContext* context,
	const papuga_Serialization* input,
	papuga_ErrorCode* errcode)
{
	*errcode = papuga_NotImplemented;
	return 0;
}

void papuga_delete_LuaRequestHandler( papuga_LuaRequestHandler* self)
{}

bool papuga_run_LuaRequestHandler( papuga_LuaRequestHandler* handler, papuga_ErrorCode* errcode)
{
	*errcode = papuga_NotImplemented;
	return false;
}

int papuga_LuaRequestHandler_nof_DelegateRequests( const papuga_LuaRequestHandler* handler)
{
	return 0;
}

papuga_DelegateRequest const* papuga_LuaRequestHandler_get_delegateRequests( const papuga_LuaRequestHandler* handler)
{
	return 0;
}

void papuga_LuaRequestHandler_init_answer( const papuga_LuaRequestHandler* handler, int idx, const papuga_Serialization* output)
{
}

papuga_Serialization* papuga_LuaRequestHandler_get_result( const papuga_LuaRequestHandler* handler)
{
	return 0;
}

