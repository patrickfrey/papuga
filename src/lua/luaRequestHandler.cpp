/*
 * Copyright (c) 2021 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/*
* \brief Structures for executing a request defined as Lua script
* \file luaRequestHandler.cpp
*/
#include "papuga/luaRequestHandler.h"
#include "papuga/requestParser.h"
#include "papuga/errors.h"
#include "papuga/allocator.h"
#include "papuga/serialization.h"
#include "papuga/serialization.hpp"
#include "papuga/valueVariant.h"
#include "papuga/valueVariant.hpp"
#include "papuga/encoding.h"
#include "papuga/lib/lua_dev.h"
#include <string>
#include <stdexcept>
#include <new>
#include <cstring>
#include <iostream>

extern "C" {
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
}

static papuga_ErrorCode errorFromLuaErrcode( int luares)
{
	if (luares == LUA_ERRSYNTAX)
	{
		return papuga_SyntaxError;
	}
	else if (luares == LUA_ERRMEM)
	{
		return papuga_NoMemError;
	}
	else if (luares == LUA_ERRRUN)
	{
		return papuga_ServiceImplementationError;
	}
	else
	{
		return papuga_LogicError;
	}
}

static int luaDumpWriter( lua_State* ls, const void* p, size_t sz, void* ud)
{
	std::string* dump = reinterpret_cast<std::string*>(ud);
	try
	{
		dump->append( (const char*)p, sz);
		return papuga_Ok;
	}
	catch (...)
	{
		return papuga_NoMemError;
	}
}

struct LuaDumpReader
{
	char const* m_content;
	std::size_t m_size;

	LuaDumpReader( const std::string& contentstr)
		:m_content(contentstr.c_str()),m_size(contentstr.size()){}

	const char* read( std::size_t* size_)
	{
		*size_ = m_size;
		m_size = 0;
		return m_content;
	}
};

static const char* luaDumpReader( lua_State* ls, void* ud, size_t* size)
{
	LuaDumpReader* dump = reinterpret_cast<LuaDumpReader*>(ud);
	return dump->read( size);
}

static void createMetatable( lua_State* ls, const char* metatableName, const struct luaL_Reg* metatableMethods)
{
	luaL_newmetatable( ls, metatableName);
	lua_pushvalue( ls, -1);
	lua_setfield( ls, -2, "__index");
	luaL_setfuncs( ls, metatableMethods, 0);
}

static int papuga_lua_doctype( lua_State* ls);
static int papuga_lua_encoding( lua_State* ls);

static int papuga_lua_yield( lua_State* ls);
static int papuga_lua_send( lua_State* ls);
static int papuga_lua_schema( lua_State* ls);
static int papuga_lua_document( lua_State* ls);

static int papuga_lua_new_request( lua_State* ls, papuga_DelegateRequest* req);
static int papuga_lua_destroy_request( lua_State* ls);
static int papuga_lua_request_result( lua_State* ls);
static int papuga_lua_request_error( lua_State* ls);

static int papuga_lua_new_context( lua_State* ls, papuga_RequestHandler* hnd, papuga_RequestContext* ctx, const papuga_lua_ClassEntryMap* cemap);
static int papuga_lua_destroy_context( lua_State* ls);
static int papuga_lua_context_get( lua_State* ls);
static int papuga_lua_context_set( lua_State* ls);
static int papuga_lua_context_inherit( lua_State* ls);

static const struct luaL_Reg g_functionlib [] = {
	{"doctype", papuga_lua_doctype},
	{"encoding", papuga_lua_encoding},
	{nullptr, nullptr} /* end of array */
};

static const struct luaL_Reg g_requestlib [] = {
	{"yield", papuga_lua_yield},
	{"send", papuga_lua_send},
	{"document", papuga_lua_document},
	{nullptr, nullptr} /* end of array */
};

static const char* g_request_metatable_name = "papuga_request";
static const struct luaL_Reg g_request_methods[] = {
	{ "__gc",		papuga_lua_destroy_request },
	{ "result",		papuga_lua_request_result },
	{ "error",		papuga_lua_request_error },
	{ nullptr,		nullptr }
};

static const char* g_context_metatable_name = "papuga_context";
static const struct luaL_Reg g_context_methods[] = {
	{ "__gc",		papuga_lua_destroy_context },
	{ "get",		papuga_lua_context_get },
	{ "set",		papuga_lua_context_set },
	{ "inherit",		papuga_lua_context_inherit },
	{ nullptr,		nullptr }
};



struct papuga_LuaRequestHandlerObject
{
public:
	papuga_LuaRequestHandlerObject( std::string&& name_, std::string&& dump_, std::string&& source_)
		:m_name(std::move(name_)),m_dump(std::move(dump_)),m_source(std::move(source_)){}

	const std::string& name() const 		{return m_name;}
	const std::string& dump() const 		{return m_dump;}
	const std::string& source() const 		{return m_source;}

private:
	std::string m_name;
	std::string m_dump;
	std::string m_source;
};

extern "C" papuga_LuaRequestHandlerObject* papuga_create_LuaRequestHandlerObject(
	const char* name,
	const char* sourcestr,
	papuga_ErrorBuffer* errbuf)
{
	lua_State* ls = luaL_newstate();
	if (!ls) 
	{
		papuga_ErrorBuffer_reportError( errbuf, "failed to create lua state");
		return nullptr;
	}
	createMetatable( ls, g_request_metatable_name, g_request_methods);
	createMetatable( ls, g_context_metatable_name, g_context_methods);
	std::string source( sourcestr);
	if (luaL_loadbuffer( ls, source.c_str(), source.size(), name))
	{
		const char* msg = lua_tostring( ls, -1);
		papuga_ErrorBuffer_reportError( errbuf, "failed to load Lua request handler object '%s': %s", name, msg);
		lua_close( ls);
		return nullptr;
	}
	#ifdef NDEBUG
	enum {Strip=0};
	#else
	enum {Strip=1};
	#endif
	std::string dump;
	papuga_ErrorCode errcode = (papuga_ErrorCode)lua_dump( ls, luaDumpWriter, &dump, Strip);
	lua_close( ls);
	if (errcode != papuga_Ok)
	{
		papuga_ErrorBuffer_reportError( errbuf, papuga_ErrorCode_tostring( errcode));
		return nullptr;
	}
	papuga_LuaRequestHandlerObject* rt = nullptr;
	try
	{
		rt = new papuga_LuaRequestHandlerObject( std::string(name), std::move(dump), std::move(source));
	}
	catch (...)
	{
		papuga_ErrorBuffer_reportError( errbuf, papuga_ErrorCode_tostring( papuga_NoMemError));
		rt = nullptr;
	}
	return rt;
}

extern "C" void papuga_destroy_LuaRequestHandlerObject( papuga_LuaRequestHandlerObject* self)
{
	delete self;
}

static void lippincottFunction( lua_State* ls)
{
	try
	{
		throw;
	}
	catch (const std::runtime_error& err)
	{
		lua_pushstring( ls, err.what());
		lua_error( ls);
	}
	catch (const std::bad_alloc&)
	{
		lua_pushstring( ls, papuga_ErrorCode_tostring( papuga_NoMemError));
		lua_error( ls);
	}
	catch (...)
	{
		lua_pushstring( ls, papuga_ErrorCode_tostring( papuga_UncaughtException));
		lua_error( ls);
	}
}

struct papuga_LuaRequestHandler
{
	enum {MaxNofDelegates=256};
	enum State {StateInit,StateRunning,StateTerminated};

	lua_State* m_ls;
	lua_State* m_thread;
	int m_threadref;
	bool m_running;
	bool m_beautifiedOutput;
	bool m_deterministicOutput;
	papuga_RequestHandler* m_handler;
	papuga_RequestContext* m_context;
	papuga_Allocator m_allocator;
	papuga_DelegateRequest m_delegate[ MaxNofDelegates];
	int m_nof_delegates;
	int m_start_delegates;
	const char* m_resultstr;
	size_t m_resultlen;
	papuga_ContentType m_doctype;
	papuga_StringEncoding m_encoding;
	int m_contentDefined;
	int m_allocatormem[ 1<<14];

	papuga_LuaRequestHandler(
			papuga_RequestHandler* handler_,
			papuga_RequestContext* context_, 
			const papuga_SchemaMap* schemamap,
			bool beautifiedOutput_,
			bool deterministicOutput_)
		:m_ls(nullptr),m_thread(nullptr),m_threadref(0),m_running(false)
		,m_beautifiedOutput(beautifiedOutput_),m_deterministicOutput(deterministicOutput_)
		,m_handler(handler_),m_context(context_),m_nof_delegates(0),m_start_delegates(0)
		,m_resultstr(nullptr),m_resultlen(0)
		,m_doctype(papuga_ContentType_Unknown),m_encoding(papuga_Binary),m_contentDefined(0)
	{
		papuga_init_Allocator( &m_allocator, m_allocatormem, sizeof(m_allocatormem));
		m_ls = lua_newstate( customAllocFunction, this);
		luaL_openlibs( m_ls);

		lua_getglobal( m_ls, "_G");
		lua_pushlightuserdata( m_ls, this);
		luaL_setfuncs( m_ls, g_requestlib, 1/*number of closure elements*/);
		luaL_setfuncs( m_ls, g_functionlib, 0/*number of closure elements*/);
		lua_pushlightuserdata( m_ls, this);
		lua_pushlightuserdata( m_ls, const_cast<papuga_SchemaMap*>(schemamap));
		lua_pushcclosure( m_ls, papuga_lua_schema, 2/*number of closure elements*/);
		lua_setglobal( m_ls, "schema");
		lua_pop( m_ls, 1);
	}

	void dumpScriptFunctions( const char* prefix, std::ostream& out)
	{
		lua_rawgeti( m_ls, LUA_REGISTRYINDEX, LUA_RIDX_GLOBALS);
		lua_pushnil( m_ls);
		while (lua_next( m_ls, -2))
		{
			if (lua_isfunction( m_ls, -1) && lua_type( m_ls, -2) == LUA_TSTRING)
			{
				out << prefix << lua_tostring( m_ls, -2) << std::endl;
			}
			lua_pop( m_ls, 1);
		}
		lua_pop( m_ls, 1);
	}

	bool init(
			const papuga_LuaRequestHandlerObject* reqobj,
			const char* requestmethod,
			const char* contentstr,
			std::size_t contentlen,
			const papuga_lua_ClassEntryMap* cemap,
			papuga_ErrorCode* errcode)
	{
		LuaDumpReader reader( reqobj->dump());
		int res = lua_load( m_ls, luaDumpReader, (void*)&reader, reqobj->name().c_str(), "b"/*mode binary*/);
		if (res != LUA_OK)
		{
			*errcode = errorFromLuaErrcode( res);
			return false;
		}
		res = lua_pcall( m_ls, 0, LUA_MULTRET, 0);
		if (res != LUA_OK)
		{
			*errcode = errorFromLuaErrcode( res);
			return false;
		}
		m_thread = lua_newthread( m_ls);
		lua_pushvalue( m_ls, -1);
		m_threadref = luaL_ref( m_ls, LUA_REGISTRYINDEX);
		lua_getglobal( m_thread, requestmethod);
		if (lua_type( m_thread, -1) != LUA_TFUNCTION)
		{
			*errcode = papuga_AddressedItemNotFound;
			lua_pop( m_thread, 1);
			return false;
		}
		papuga_lua_new_context( m_thread, m_handler, m_context, cemap);
		lua_pushlstring( m_thread, contentstr, contentlen);
		return true;
	}

	int resume( int nof_args, int& nresults)
	{
#if LUA_VERSION_NUM <= 501
		nresults = 1;
		return lua_resume( m_thread, nof_args);
#elif LUA_VERSION_NUM <= 503
		nresults = 1;
		return lua_resume( m_thread, nullptr, nof_args);
#else
		return lua_resume( m_thread, nullptr, nof_args, &nresults);
#endif				
	}

	bool run( papuga_ErrorBuffer* errbuf)
	{
		char const* msg;
		int nof_args = m_running ? 0 : 2;
		m_running = true;
		papuga_ErrorCode errcode = papuga_Ok;
		papuga_ValueVariant resultval;
		papuga_Serialization detser;
		int nresults = 0;
		switch (resume( nof_args, nresults))
		{
			case LUA_YIELD:
				return false;
			case LUA_OK:
				if (nresults > 1)
				{
					papuga_ErrorBuffer_reportError( errbuf, papuga_ErrorCode_tostring( papuga_ServiceImplementationError));
					return false;
				}
				else if (nresults == 1)
				{
					if (papuga_lua_value( m_thread, &resultval, &m_allocator, -1, &errcode))
					{
						if (m_deterministicOutput && resultval.valuetype == papuga_TypeSerialization)
						{
							papuga_init_Serialization( &detser, &m_allocator);
							if (!papuga_Serialization_copy_deterministic( &detser, resultval.value.serialization, &errcode))
							{
								papuga_ErrorBuffer_reportError( errbuf, papuga_ErrorCode_tostring( errcode));
								return false;
							}
							resultval.value.serialization = &detser;
						}
						m_resultstr = getOutputString( resultval, &m_resultlen, &errcode);
						if (!m_resultstr && errcode != papuga_Ok)
						{
							papuga_ErrorBuffer_reportError( errbuf, papuga_ErrorCode_tostring( errcode));
							return false;
						}
					}
					else
					{
						papuga_ErrorBuffer_reportError( errbuf, papuga_ErrorCode_tostring( errcode));
						return false;
					}
				}
				return true;
			default:
				msg = lua_tostring( m_thread, -1);
				papuga_ErrorBuffer_reportError( errbuf, msg);
				return false;
		}
	}

	papuga_DelegateRequest* send( const char* requestmethod, const char* url, papuga_ValueVariant* content)
	{
		if (m_nof_delegates >= MaxNofDelegates)
		{
			throw std::runtime_error( papuga_ErrorCode_tostring( papuga_NoMemError));
		}
		papuga_DelegateRequest* req = &m_delegate[ m_nof_delegates];
		std::memset( req, 0, sizeof(papuga_DelegateRequest));
		req->requestmethod = papuga_Allocator_copy_charp( &m_allocator, requestmethod);
		req->url = papuga_Allocator_copy_charp( &m_allocator, url);
		if (!req->requestmethod || !req->url)
		{
			throw std::bad_alloc();
		}
		req->errcode = papuga_Ok;
		req->errmsg = nullptr;
		req->resultstr = nullptr;
		req->resultlen = 0;
		req->contentstr = (char*)papuga_ValueVariant_tojson(
					content, &m_allocator, nullptr/*structdefs*/,
					papuga_UTF8, m_beautifiedOutput, nullptr/*rooname*/, "item"/*elemname*/, 
					&req->contentlen, &req->errcode);
		if (!req->contentstr)
		{
			throw std::runtime_error( papuga_ErrorCode_tostring( req->errcode));
		}
		m_nof_delegates += 1;
		return req;
	}
	~papuga_LuaRequestHandler()
	{
		if (m_ls) {lua_close( m_ls); m_ls = nullptr;}
		papuga_destroy_Allocator( &m_allocator);
	}

	void setDocumentType( papuga_ContentType doctype_, papuga_StringEncoding encoding_)
	{
		if (m_contentDefined == 0)
		{
			m_doctype = doctype_;
			m_encoding = encoding_;
			m_contentDefined = 1;
		}
		else if (m_contentDefined == 1)
		{
			if (m_doctype != doctype_ || m_encoding != encoding_)
			{
				m_contentDefined = 2;
			}
		}
	}

	const char* getOutputString( const papuga_ValueVariant& result, size_t* resultlen, papuga_ErrorCode* errcode)
	{
		if (!papuga_ValueVariant_defined( &result))
		{
			*errcode = papuga_Ok;
			*resultlen = 0;
			return nullptr;
		}
		else if (papuga_ValueVariant_isstring( &result))
		{
			*errcode = papuga_Ok;
			*resultlen = result.length;
			return result.value.string;
		}
		else if (m_contentDefined == 1)
		{
			papuga_StringEncoding encoding = (m_encoding == papuga_Binary) ? papuga_UTF8 : m_encoding;
			switch (m_doctype)
			{
				case papuga_ContentType_Unknown:
				case papuga_ContentType_JSON:
					return (char*)papuga_ValueVariant_tojson(
							&result, &m_allocator, nullptr/*structdefs*/,
							encoding, m_beautifiedOutput, nullptr/*rooname*/, "item"/*elemname*/, 
							resultlen, errcode);
					break;
				case papuga_ContentType_XML:
					return (char*)papuga_ValueVariant_toxml(
							&result, &m_allocator, nullptr/*structdefs*/,
							encoding, m_beautifiedOutput, nullptr/*rooname*/, "item"/*elemname*/, 
							resultlen, errcode);
					break;
			}
		}
		*errcode = papuga_UnknownContentType;
		*resultlen = 0;
		return nullptr;
	}

	enum AllocClass {AllocClassNone=-1,AllocClassNothing=0,AllocClassTiny=8,AllocClassSmall=64,AllocClassMedium=256};
	static AllocClass getAllocClass( std::size_t memsize)
	{
		return  (memsize <= (size_t)AllocClassTiny)
			? (memsize == 0 ? AllocClassNothing : AllocClassTiny)
			: ((memsize <= (size_t)AllocClassSmall)
				? AllocClassSmall
				: ((memsize <= (size_t)AllocClassMedium)
					? AllocClassMedium
					: AllocClassNone
				)
			);
	}
	void* customAllocFunction( void *ptr, size_t osize, size_t nsize)
	{
		void* rt = 0;
		if (nsize == 0)
		{
			return nullptr; //... AllocClassNothing
		}
		AllocClass oc = getAllocClass( osize);
		AllocClass nc = getAllocClass( nsize);
		if (nc == AllocClassNone)
		{
			if (nsize <= osize)
			{
				rt = ptr;
			}
			else
			{
				rt = papuga_Allocator_alloc( &m_allocator, nsize, 0);
				if (rt && ptr)
				{
					std::memcpy( rt, ptr, osize);
				}
			}
		}
		else if (nc == oc && ptr)
		{
			rt = ptr;
		}
		else
		{
			rt = papuga_Allocator_alloc( &m_allocator, nc, 0);
			if (rt && ptr)
			{
				std::memcpy( rt, ptr, (osize < nsize) ? osize : nsize);
			}
		}
		return rt;
	}
	static void* mallocAllocFunction( void *ud, void *ptr, size_t osize, size_t nsize)
	{
		return ::realloc( ptr, nsize);
	}
	static void* customAllocFunction( void *ud, void *ptr, size_t osize, size_t nsize)
	{
		return ((papuga_LuaRequestHandler*)ud)->customAllocFunction( ptr, osize, nsize);
	}
};

static int papuga_lua_new_request( lua_State* ls, papuga_DelegateRequest* req)
{
	papuga_DelegateRequest** td = (papuga_DelegateRequest**)lua_newuserdata( ls, sizeof(papuga_DelegateRequest*));
	*td = req;
	luaL_getmetatable( ls, g_request_metatable_name);
	lua_setmetatable( ls, -2);
	return 1;
}

static int papuga_lua_destroy_request( lua_State* ls)
{
	return 0;
}

static int papuga_lua_request_result( lua_State* ls)
{
	papuga_DelegateRequest** td = (papuga_DelegateRequest**)luaL_checkudata( ls, 1, g_request_metatable_name);
	papuga_DelegateRequest* req = *td;
	try
	{
		int nargs = lua_gettop( ls);
		if (nargs != 0)
		{
			luaL_error( ls, papuga_ErrorCode_tostring( papuga_NofArgsError));
		}
		if (req->errcode != papuga_Ok)
		{
			return 0;
		}
		int allocatormem[ 4096];
		papuga_Allocator allocator;
		papuga_Serialization resultser;
		papuga_ValueVariant resultval; 

		papuga_init_Allocator( &allocator, allocatormem, sizeof(allocatormem));
		papuga_init_Serialization( &resultser, &allocator);
		papuga_ErrorCode errcode = papuga_Ok;

		if (papuga_Serialization_append_json( &resultser, req->resultstr, req->resultlen, papuga_UTF8, false, &errcode))
		{
			papuga_destroy_Allocator( &allocator);
			luaL_error( ls, papuga_ErrorCode_tostring( errcode));
		}
		papuga_init_ValueVariant_serialization( &resultval, &resultser);
		if (!papuga_lua_push_value_plain( ls, &resultval, &errcode))
		{
			papuga_destroy_Allocator( &allocator);
			luaL_error( ls, papuga_ErrorCode_tostring( errcode));
		}
		return 1;
	}
	catch (...) { lippincottFunction( ls); }
	return 0;
}

static int papuga_lua_request_error( lua_State* ls)
{
	papuga_DelegateRequest** td = (papuga_DelegateRequest**)luaL_checkudata( ls, 1, g_request_metatable_name);
	try
	{
		int nargs = lua_gettop( ls);
		if (nargs != 0)
		{
			luaL_error( ls, papuga_ErrorCode_tostring( papuga_NofArgsError));
		}
		if ((*td)->errcode != papuga_Ok)
		{
			lua_pushstring( ls, papuga_ErrorCode_tostring( (*td)->errcode));
			return 1;
		}
	}
	catch (...) { lippincottFunction( ls); }
	return 0;
}

struct LuaRequestContext
{
	papuga_RequestHandler* hnd;
	papuga_RequestContext* ctx;
	const papuga_lua_ClassEntryMap* cemap;
};

static int papuga_lua_new_context( lua_State* ls, papuga_RequestHandler* hnd, papuga_RequestContext* ctx, const papuga_lua_ClassEntryMap* cemap)
{
	LuaRequestContext* td = (LuaRequestContext*)lua_newuserdata( ls, sizeof(LuaRequestContext));
	td->hnd = hnd;
	td->ctx = ctx;
	td->cemap = cemap;
	luaL_getmetatable( ls, g_context_metatable_name);
	lua_setmetatable( ls, -2);
	return 1;
}

static int papuga_lua_destroy_context( lua_State* ls)
{
	return 0;
}

static int papuga_lua_context_get( lua_State* ls)
{
	LuaRequestContext* td = (LuaRequestContext*)luaL_checkudata( ls, 1, g_context_metatable_name);
	try
	{
		int nargs = lua_gettop( ls);
		if (nargs != 2)
		{
			luaL_error( ls, papuga_ErrorCode_tostring( papuga_NofArgsError));
		}
		if (lua_type( ls, 2) != LUA_TSTRING)
		{
			luaL_error( ls, papuga_ErrorCode_tostring( papuga_InvalidAccess));
		}
		char const* name = lua_tostring( ls, 2);
		if (name)
		{
			const papuga_ValueVariant* value = papuga_RequestContext_get_variable( td->ctx, name);
			if (value)
			{
				papuga_ErrorCode errcode = papuga_Ok;
				if (!papuga_lua_push_value( ls, value, td->cemap, &errcode))
				{
					luaL_error( ls, papuga_ErrorCode_tostring( errcode));
				}
				return 1;
			}
		}
	}
	catch (...) { lippincottFunction( ls); }	
	return 0;
}

static int papuga_lua_context_set( lua_State* ls)
{
	LuaRequestContext* td = (LuaRequestContext*)luaL_checkudata( ls, 1, g_context_metatable_name);
	try
	{
		int nargs = lua_gettop( ls);
		if (nargs != 3)
		{
			luaL_error( ls, papuga_ErrorCode_tostring( papuga_NofArgsError));
		}
		if (lua_type( ls, 2) != LUA_TSTRING)
		{
			luaL_error( ls, papuga_ErrorCode_tostring( papuga_InvalidAccess));
		}
		char const* name = lua_tostring( ls, 2);
		if (name)
		{
			papuga_Allocator allocator;
			int allocatormem[ 2048];
			papuga_init_Allocator( &allocator, &allocatormem, sizeof(allocatormem));
			papuga_ErrorCode errcode = papuga_Ok;
			papuga_ValueVariant value;
			if (!papuga_lua_value( ls, &value, &allocator, 3, &errcode))
			{
				papuga_destroy_Allocator( &allocator);
				luaL_error( ls, papuga_ErrorCode_tostring( errcode));
			}
			if (!papuga_RequestContext_define_variable( td->ctx, name, &value))
			{
				papuga_destroy_Allocator( &allocator);
				luaL_error( ls, papuga_ErrorCode_tostring( papuga_NoMemError));
			}
		}
	}
	catch (...) { lippincottFunction( ls); }	
	return 0;
}

static int papuga_lua_context_inherit( lua_State* ls)
{
	LuaRequestContext* td = (LuaRequestContext*)luaL_checkudata( ls, 1, g_context_metatable_name);
	try
	{
		int nargs = lua_gettop( ls);
		if (nargs != 3)
		{
			luaL_error( ls, papuga_ErrorCode_tostring( papuga_NofArgsError));
		}
		if (lua_type( ls, 2) != LUA_TSTRING || lua_type( ls, 3) != LUA_TSTRING)
		{
			luaL_error( ls, papuga_ErrorCode_tostring( papuga_InvalidAccess));
		}
		char const* inheritContextType = lua_tostring( ls, 2);
		char const* inheritContextName = lua_tostring( ls, 3);
		if (!inheritContextType || !inheritContextName)
		{
			luaL_error( ls, papuga_ErrorCode_tostring( papuga_InvalidAccess));
		}
		if (!papuga_RequestContext_inherit( td->ctx, td->hnd, inheritContextType, inheritContextName))
		{
			luaL_error( ls, papuga_ErrorCode_tostring( papuga_NoMemError));
		}
		return 0;
	}
	catch (...) { lippincottFunction( ls); }	
	return 0;
}

static int papuga_lua_yield( lua_State* ls)
{
	papuga_LuaRequestHandler* reqhnd = (papuga_LuaRequestHandler*)lua_touserdata(ls, lua_upvalueindex(1));
	if (!reqhnd)
	{
		luaL_error( ls, papuga_ErrorCode_tostring( papuga_LogicError));
	}
	try
	{
		lua_yield( ls, 0);
	}
	catch (...) {lippincottFunction( ls);}
	return 0;
}

static int papuga_lua_send( lua_State* ls)
{
	papuga_LuaRequestHandler* reqhnd = (papuga_LuaRequestHandler*)lua_touserdata(ls, lua_upvalueindex(1));
	if (!reqhnd)
	{
		luaL_error( ls, papuga_ErrorCode_tostring( papuga_LogicError));
	}
	try
	{
		int nn = lua_gettop( ls);
		if (nn != 3)
		{
			luaL_error( ls, papuga_ErrorCode_tostring( papuga_NofArgsError));
		}
		if (lua_type( ls, 1) != LUA_TSTRING || lua_type( ls, 2) != LUA_TSTRING)
		{
			luaL_error( ls, papuga_ErrorCode_tostring( papuga_TypeError));
		}
		papuga_ValueVariant value;
		papuga_ErrorCode errcode = papuga_Ok;

		const char* requestmethod = lua_tostring( ls, 1);
		const char* url = lua_tostring( ls, 2);
		if (!papuga_lua_value( ls, &value, &reqhnd->m_allocator, 3, &errcode))
		{
			luaL_error( ls, papuga_ErrorCode_tostring( errcode));
		}		
		papuga_DelegateRequest* req = reqhnd->send( requestmethod, url, &value);
		return papuga_lua_new_request( ls, req);
	}
	catch (...) {lippincottFunction( ls);}
	return 0;
}

static int papuga_lua_document( lua_State* ls)
{
	papuga_LuaRequestHandler* reqhnd = (papuga_LuaRequestHandler*)lua_touserdata(ls, lua_upvalueindex(1));	
	int nn = lua_gettop( ls);
	if (nn != 3)
	{
		luaL_error( ls, papuga_ErrorCode_tostring( papuga_NofArgsError));
	}
	if (lua_type( ls, 1) != LUA_TSTRING || lua_type( ls, 2) != LUA_TSTRING)
	{
		luaL_error( ls, papuga_ErrorCode_tostring( papuga_TypeError));
	}
	papuga_ContentType doctype = papuga_contentTypeFromName( lua_tostring( ls, 1));
	if (doctype == papuga_ContentType_Unknown)
	{
		luaL_error( ls, papuga_ErrorCode_tostring( papuga_UnknownContentType));
	}
	papuga_StringEncoding encoding;
	if (!papuga_getStringEncodingFromName( &encoding, lua_tostring( ls, 2)))
	{
		luaL_error( ls, papuga_ErrorCode_tostring( papuga_EncodingError));
	}
	papuga_ErrorCode errcode = papuga_Ok;
	papuga_ValueVariant doc;
	if (!papuga_lua_value( ls, &doc, &reqhnd->m_allocator, 3, &errcode))
	{
		luaL_error( ls, papuga_ErrorCode_tostring( errcode));
	}
	size_t doclen = 0;
	char* docstr = nullptr;
	switch (doctype)
	{
		case papuga_ContentType_Unknown:
		case papuga_ContentType_JSON:
			docstr = (char*)papuga_ValueVariant_tojson(
					&doc, &reqhnd->m_allocator, nullptr/*structdefs*/,
					encoding, reqhnd->m_beautifiedOutput, nullptr/*rooname*/, "item"/*elemname*/, 
					&doclen, &errcode);
			break;
		case papuga_ContentType_XML:
			docstr = (char*)papuga_ValueVariant_toxml(
					&doc, &reqhnd->m_allocator, nullptr/*structdefs*/,
					encoding, reqhnd->m_beautifiedOutput, nullptr/*rooname*/, "item"/*elemname*/, 
					&doclen, &errcode);
			break;
	}
	if (!docstr)
	{
		luaL_error( ls, papuga_ErrorCode_tostring( errcode));
	}
	lua_pushlstring( ls, docstr, doclen);
	return 1;
}

static void lua_schema_error( lua_State* ls, const papuga_SchemaError& err)
{
	if (err.line)
	{
		if (err.item[0])
		{
			luaL_error( ls, "%s at line %d: item '%s'", papuga_ErrorCode_tostring( err.code), err.line, err.item);
		}
		else
		{
			luaL_error( ls, "%s at line %d", papuga_ErrorCode_tostring( err.code), err.line);
		}
	}
	else
	{
		if (err.item[0])
		{
			luaL_error( ls, "%s, item '%s'", papuga_ErrorCode_tostring( err.code), err.item);
		}
		else
		{
			luaL_error( ls, "%s", papuga_ErrorCode_tostring( err.code));
		}
	}
}

static int papuga_lua_doctype( lua_State* ls)
{
	int nn = lua_gettop( ls);
	if (nn != 1)
	{
		luaL_error( ls, papuga_ErrorCode_tostring( papuga_NofArgsError));
	}
	if (lua_type( ls, 1) != LUA_TSTRING)
	{
		luaL_error( ls, papuga_ErrorCode_tostring( papuga_TypeError));
	}
	std::size_t contentlen;
	const char* contentstr = lua_tolstring( ls, 1, &contentlen);
	papuga_ContentType doctype = papuga_guess_ContentType( contentstr, contentlen);
	const char* doctypestr = papuga_ContentType_name( doctype);
	if (doctypestr)
	{
		lua_pushstring( ls, doctypestr);
		return 1;
	}
	return 0;
}

static int papuga_lua_encoding( lua_State* ls)
{
	int nn = lua_gettop( ls);
	if (nn != 1)
	{
		luaL_error( ls, papuga_ErrorCode_tostring( papuga_NofArgsError));
	}
	if (lua_type( ls, 1) != LUA_TSTRING)
	{
		luaL_error( ls, papuga_ErrorCode_tostring( papuga_TypeError));
	}
	std::size_t contentlen;
	const char* contentstr = lua_tolstring( ls, 1, &contentlen);
	papuga_StringEncoding encoding = papuga_guess_StringEncoding( contentstr, contentlen);
	const char* encodingstr = papuga_StringEncoding_name( encoding);
	if (encodingstr)
	{
		lua_pushstring( ls, encodingstr);
		return 1;
	}
	return 0;
}

static int papuga_lua_schema( lua_State* ls)
{
	papuga_LuaRequestHandler* reqhnd = (papuga_LuaRequestHandler*)lua_touserdata(ls, lua_upvalueindex(1));	
	const papuga_SchemaMap* schemamap = (papuga_SchemaMap*)lua_touserdata(ls, lua_upvalueindex(2));
	if (!schemamap || !reqhnd)
	{
		luaL_error( ls, papuga_ErrorCode_tostring( papuga_LogicError));
	}
	try
	{
		int nn = lua_gettop( ls);
		if (nn != 2)
		{
			luaL_error( ls, papuga_ErrorCode_tostring( papuga_NofArgsError));
		}
		if (lua_type( ls, 1) != LUA_TSTRING || lua_type( ls, 2) != LUA_TSTRING)
		{
			luaL_error( ls, papuga_ErrorCode_tostring( papuga_TypeError));
		}
		const char* schemaname = lua_tostring( ls, 1);
		std::size_t contentlen;
		const char* contentstr = lua_tolstring( ls, 2, &contentlen);
		papuga_ErrorCode errcode;

		papuga_ContentType doctype = papuga_guess_ContentType( contentstr, contentlen);
		papuga_StringEncoding encoding = papuga_guess_StringEncoding( contentstr, contentlen);
		if (doctype == papuga_ContentType_Unknown)
		{
			luaL_error( ls, papuga_ErrorCode_tostring( papuga_UnknownContentType));
		}
		if (encoding == papuga_Binary)
		{
			luaL_error( ls, papuga_ErrorCode_tostring( papuga_EncodingError));
		}
		reqhnd->setDocumentType( doctype, encoding);
		papuga_Schema const* schema = papuga_SchemaMap_get( schemamap, schemaname);
		if (!schema)
		{
			luaL_error( ls, papuga_ErrorCode_tostring( papuga_UnknownSchema));
		}
		papuga_Serialization contentser;
		papuga_Allocator allocator;
		int allocatormem[ 4096];
		papuga_init_Allocator( &allocator, allocatormem, sizeof(allocatormem));

		papuga_SchemaError schemaerr;
		papuga_init_SchemaError( &schemaerr);
		papuga_init_Serialization( &contentser, &allocator);
		if (!papuga_schema_parse( &contentser, schema, doctype, encoding, contentstr, contentlen, &schemaerr))
		{
			papuga_destroy_Allocator( &allocator);
			lua_schema_error( ls, schemaerr);
		}
		bool serr = papuga_lua_push_serialization( ls, &contentser, NULL/*cemap*/, &errcode);
		papuga_destroy_Allocator( &allocator);		
		if (!serr)
		{
			luaL_error( ls, papuga_ErrorCode_tostring( errcode));
		}
		return 1;
	}
	catch (...) {lippincottFunction( ls);}
	return 1;
}

extern "C" papuga_LuaRequestHandler* papuga_create_LuaRequestHandler(
	const papuga_LuaRequestHandlerObject* reqobj,
	const papuga_lua_ClassEntryMap* cemap,
	const papuga_SchemaMap* schemamap,
	papuga_RequestHandler* requesthandler,
	papuga_RequestContext* requestcontext,
	const char* requestmethod,
	const char* contentstr,
	std::size_t contentlen,
	bool beautifiedOutput,
	bool deterministicOutput,
	papuga_ErrorCode* errcode)
{
	papuga_LuaRequestHandler* rt = 0;
	rt = new (std::nothrow) papuga_LuaRequestHandler( requesthandler, requestcontext, schemamap, beautifiedOutput, deterministicOutput);
	if (!rt)
	{
		*errcode = papuga_NoMemError;
		return 0;
	}
	if (!rt->init( reqobj, requestmethod, contentstr, contentlen, cemap, errcode))
	{
		delete rt;
		rt = 0;
	}
	return rt;
}

extern "C" void papuga_destroy_LuaRequestHandler( papuga_LuaRequestHandler* self)
{
	delete self;
}

extern "C" bool papuga_run_LuaRequestHandler( papuga_LuaRequestHandler* handler, papuga_ErrorBuffer* errbuf)
{
	handler->m_start_delegates = handler->m_nof_delegates;
	return handler->run( errbuf);
}

extern "C" int papuga_LuaRequestHandler_nof_DelegateRequests( const papuga_LuaRequestHandler* handler)
{
	return handler->m_nof_delegates - handler->m_start_delegates;
}

extern "C" papuga_DelegateRequest const* papuga_LuaRequestHandler_get_delegateRequest( const papuga_LuaRequestHandler* handler, int idx)
{
	if (handler->m_start_delegates + idx > handler->m_nof_delegates) return nullptr;
	return &handler->m_delegate[ handler->m_start_delegates + idx];
}

extern "C" void papuga_LuaRequestHandler_init_result( papuga_LuaRequestHandler* handler, int idx, const char* resultstr, size_t resultlen)
{
	if (handler->m_start_delegates + idx < handler->m_nof_delegates)
	{
		papuga_DelegateRequest* req = &handler->m_delegate[ handler->m_start_delegates + idx];
		req->resultstr = papuga_Allocator_copy_string( &handler->m_allocator, resultstr, resultlen);
		if (!req->resultstr)
		{
			req->errcode = papuga_NoMemError;
			req->errmsg = nullptr;
			req->resultlen = 0;
		}
		else
		{
			req->resultlen = resultlen;
		}
	}
}

extern "C" void papuga_LuaRequestHandler_init_error( papuga_LuaRequestHandler* handler, int idx, papuga_ErrorCode errcode, const char* errmsg)
{
	if (handler->m_start_delegates + idx < handler->m_nof_delegates)
	{
		papuga_DelegateRequest* req = &handler->m_delegate[ handler->m_start_delegates + idx];
		req->errcode = errcode;
		if (errmsg && errmsg[0])
		{
			req->errmsg = papuga_Allocator_copy_charp( &handler->m_allocator, errmsg);
		}
	}
}

extern "C" const char* papuga_LuaRequestHandler_get_result( const papuga_LuaRequestHandler* handler, size_t* resultlen)
{
	*resultlen = handler->m_resultlen;
	return handler->m_resultstr;
}

