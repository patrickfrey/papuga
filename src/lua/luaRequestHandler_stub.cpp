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

extern "C" void papuga_init_RequestAttributes( papuga_RequestAttributes* dest, const char* http_accept, const char* html_head, const char* html_base_href, bool beautifiedOutput, bool deterministicOutput)
{
	return;
}
extern "C" bool papuga_copy_RequestAttributes( papuga_Allocator* allocator, papuga_RequestAttributes* dest, papuga_RequestAttributes const* src)
{
	return false;
}
extern "C" papuga_ContentType papuga_http_default_doctype( const papuga_RequestAttributes* attr)
{
	return papuga_ContentType_Unknown;
}

extern "C" const char* papuga_http_linkbase( const papuga_RequestAttributes* attr, char* buf, size_t bufsize)
{
	return nullptr;
}

extern "C" papuga_LuaRequestHandlerScript* papuga_create_LuaRequestHandlerScript(
	const char* name,
	const char* source,
	papuga_ErrorBuffer* errbuf)
{
	papuga_ErrorBuffer_reportError( errbuf, papuga_ErrorCode_tostring( papuga_NotImplemented));
	return 0;
}

extern "C" void papuga_destroy_LuaRequestHandlerScript( papuga_LuaRequestHandlerScript* self)
{}
extern "C" const char* papuga_LuaRequestHandlerScript_options( papuga_LuaRequestHandlerScript const* self)
{
	return "";
}

extern "C" papuga_LuaRequestHandler* papuga_create_LuaRequestHandler(
	const papuga_LuaRequestHandlerScript* script,
	papuga_LuaInitProc* initproc,
	const papuga_SchemaMap* schemamap,
	papuga_RequestContextPool* contextpool,
	papuga_RequestContext* context,
	papuga_TransactionHandler* transactionHandler,
	papuga_Logger* logger,
	const papuga_RequestAttributes* attributes,
	const char* requestmethod,
	const char* contextname,
	const char* requestpath,
	const char* contentstr,
	size_t contentlen,
	papuga_ErrorCode* errcode)
{
	*errcode = papuga_NotImplemented;
	return 0;
}

extern "C" void papuga_destroy_LuaRequestHandler( papuga_LuaRequestHandler* self)
{}

extern "C" bool papuga_run_LuaRequestHandler( papuga_LuaRequestHandler* handler, papuga_ErrorBuffer* errbuf)
{
	papuga_ErrorBuffer_reportError( errbuf, papuga_ErrorCode_tostring( papuga_NotImplemented));
	return false;
}

extern "C" int papuga_LuaRequestHandler_nof_DelegateRequests( const papuga_LuaRequestHandler* handler)
{return 0;}

extern "C" papuga_DelegateRequest const* papuga_LuaRequestHandler_get_delegateRequests( const papuga_LuaRequestHandler* handler)
{return nullptr;}

extern "C" void papuga_LuaRequestHandler_init_result( papuga_LuaRequestHandler* handler, int idx, const char* resultstr, size_t resultlen)
{}

extern "C" void papuga_LuaRequestHandler_init_error( papuga_LuaRequestHandler* handler, int idx, papuga_ErrorCode errcode, const char* errmsg)
{}

extern "C" const papuga_LuaRequestResult* papuga_LuaRequestHandler_get_result( const papuga_LuaRequestHandler* handler)
{return nullptr;}
