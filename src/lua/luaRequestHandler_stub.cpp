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
#include "papuga/luaRequestHandler.h"

papuga_LuaRequestHandlerObject* papuga_create_LuaRequestHandlerObject(
	const char* name,
	const char* source,
	papuga_ErrorBuffer* errbuf)
{
	papuga_ErrorBuffer_reportError( errbuf, papuga_ErrorCode_tostring( papuga_NotImplemented));
	return 0;
}

void papuga_destroy_LuaRequestHandlerObject( papuga_LuaRequestHandlerObject* self)
{}

papuga_LuaRequestHandler* papuga_create_LuaRequestHandler(
	const papuga_LuaRequestHandlerObject* reqobj,
	const papuga_lua_ClassEntryMap* cemap,
	const papuga_SchemaMap* schemamap,
	papuga_RequestContext* context,
	const char* requestmethod,
	const char* contentstr,
	size_t contentlen,
	papuga_ErrorCode* errcode)
{
	*errcode = papuga_NotImplemented;
	return 0;
}

void papuga_destroy_LuaRequestHandler( papuga_LuaRequestHandler* self)
{}

bool papuga_run_LuaRequestHandler( papuga_LuaRequestHandler* handler, papuga_ErrorBuffer* errbuf)
{
	papuga_ErrorBuffer_reportError( errbuf, papuga_ErrorCode_tostring( papuga_NotImplemented));
	return false;
}

int papuga_LuaRequestHandler_nof_DelegateRequests( const papuga_LuaRequestHandler* handler)
{return 0;}

papuga_DelegateRequest const* papuga_LuaRequestHandler_get_delegateRequests( const papuga_LuaRequestHandler* handler)
{return nullptr;}

void papuga_LuaRequestHandler_init_result( papuga_LuaRequestHandler* handler, int idx, const char* resultstr, size_t resultlen)
{}
void papuga_LuaRequestHandler_init_error( papuga_LuaRequestHandler* handler, int idx, papuga_ErrorCode errcode, const char* errmsg)
{}

const char* papuga_LuaRequestHandler_get_result( const papuga_LuaRequestHandler* handler, size_t* resultlen)
{return nullptr;}

