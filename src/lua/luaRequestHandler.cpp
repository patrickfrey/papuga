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
#include "luaRequestHandler.h"
#include "papuga/allocator.h"
#include "papuga/serialization.h"
#include "papuga/lib/lua_dev.h"
#include <string>
#include <stdexcept>
#include <new>
#include <cstring>

extern "C" {
#include <lua.h>
#include <lauxlib.h>
}

static int luaDumpWriter( lua_State* ls, const void* p, size_t sz, void* ud)
{
	std::string* dump = reinterpret_cast<std::string*>(ud);
	try
	{
		dump->append( (const char*)p, sz);
		return 0;
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


struct papuga_LuaRequestHandlerFunction
{
public:
	papuga_LuaRequestHandlerFunction( std::string&& name_, std::string&& dump_)
		:m_name(std::move(name_)),m_dump(std::move(dump_)){}

	const std::string& name() const {return m_name;}
	const std::string& dump() const {return m_dump;}

private:
	std::string m_name;
	std::string m_dump;
};

papuga_LuaRequestHandlerFunction* papuga_create_LuaRequestHandlerFunction(
	const char* functionName,
	const char* source,
	papuga_ErrorBuffer* errbuf)
{
	lua_State* ls = luaL_newstate();
	if (!ls) 
	{
		papuga_ErrorBuffer_reportError( errbuf, "failed to create lua state");
		return 0;
	}
	if (luaL_loadbuffer( ls, source, std::strlen(source), functionName))
	{
		const char* msg = lua_tostring( ls, -1);
		papuga_ErrorBuffer_reportError( errbuf, "failed to load function '%s': %s", functionName, msg);
		lua_close( ls);
		return 0;
	}
	#ifdef NDEBUG
	enum {Strip=0};
	#else
	enum {Strip=1};
	#endif
	std::string name;
	std::string dump;
	papuga_ErrorCode errcode = (papuga_ErrorCode)lua_dump( ls, luaDumpWriter, &dump, Strip);
	lua_close( ls);
	if (errcode != papuga_Ok)
	{
		papuga_ErrorBuffer_reportError( errbuf, papuga_ErrorCode_tostring( errcode));
		return 0;
	}
	papuga_LuaRequestHandlerFunction* rt = 0;
	try
	{
		name.append( functionName);
		rt = new papuga_LuaRequestHandlerFunction( std::move(name), std::move(dump));
	}
	catch (...)
	{
		papuga_ErrorBuffer_reportError( errbuf, papuga_ErrorCode_tostring( papuga_NoMemError));
		rt = 0;
	}
	return rt;
}

void papuga_delete_LuaRequestHandlerFunction( papuga_LuaRequestHandlerFunction* self)
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

static int papuga_lua_yield( lua_State* ls);
static int papuga_lua_send( lua_State* ls);

static const struct luaL_Reg g_requestlib [] = {
	{"yield", papuga_lua_yield},
	{"send", papuga_lua_send},
	{nullptr, nullptr} /* end of array */
};

struct papuga_LuaRequestHandler
{
	enum {MaxNofDelegates=256};

	lua_State* m_ls;
	papuga_RequestContext* m_context;
	papuga_Allocator m_allocator;
	papuga_DelegateRequest m_delegate[ MaxNofDelegates];
	int m_nof_delegates;
	int m_allocatormem[ 1<<14];

	explicit papuga_LuaRequestHandler( papuga_RequestContext* context_)
		:m_ls(0)
		,m_context(context_)
		,m_nof_delegates(0)
	{
		papuga_init_Allocator( &m_allocator, m_allocatormem, sizeof(m_allocatormem));
		m_ls = lua_newstate( allocFunction, this);
		lua_getglobal( m_ls, "_G");
		lua_pushlightuserdata( m_ls, this);
		luaL_setfuncs( m_ls, g_requestlib, 1/*number of closure elements*/);
		lua_pop( m_ls, 1);
	}
	void send( const char* requestmethod, const char* url, papuga_Serialization* content)
	{
		if (m_nof_delegates >= MaxNofDelegates)
		{
			throw std::runtime_error( papuga_ErrorCode_tostring( papuga_NoMemError));
		}
		papuga_DelegateRequest* req = &m_delegate[ m_nof_delegates];
		req->requestmethod = papuga_Allocator_copy_charp( &m_allocator, requestmethod);
		req->url = papuga_Allocator_copy_charp( &m_allocator, url);
		if (!req->requestmethod || !req->url)
		{
			throw std::bad_alloc();
		}
		std::memcpy( &req->content, content, sizeof( req->content));
		m_nof_delegates += 1;
	}
	~papuga_LuaRequestHandler()
	{
		if (m_ls) {lua_close( m_ls);}
		papuga_destroy_Allocator( &m_allocator);
	}
	enum AllocClass {AllocClassNone=0,AllocClassTiny=16,AllocClassSmall=64,AllocClassMedium=256};
	static AllocClass getAllocClass( std::size_t memsize)
	{
		return  (memsize <= (size_t)AllocClassTiny)
			? AllocClassTiny
			: ((memsize <= (size_t)AllocClassSmall)
				? AllocClassSmall
				: ((memsize <= (size_t)AllocClassMedium)
					? AllocClassMedium
					: AllocClassNone
				)
			);
	}
	void* thisAllocFunction( void *ptr, size_t osize, size_t nsize)
	{
		void* rt = 0;
		if (ptr)
		{
			AllocClass oc = getAllocClass( osize);
			AllocClass nc = getAllocClass( nsize);
			if (nc == oc)
			{
				rt = ptr;
			}
			else
			{
				rt = papuga_Allocator_alloc( &m_allocator, nsize, 0);
				if (rt)
				{
					std::memcpy( rt, ptr, (osize < nsize) ? osize : nsize);
				}
			}
		}
		else
		{
			rt = papuga_Allocator_alloc( &m_allocator, nsize, 0);
			
		}
		return rt;
	}
	static void* allocFunction( void *ud, void *ptr, size_t osize, size_t nsize)
	{
		return ((papuga_LuaRequestHandler*)ud)->thisAllocFunction( ptr, osize, nsize);
	}
};

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
		const char* requestmethod = lua_tostring( ls, 1);
		const char* url = lua_tostring( ls, 2);
		papuga_Serialization content;
		papuga_init_Serialization( &content, &reqhnd->m_allocator);
		papuga_ErrorCode errcode = papuga_Ok;
		if (!papuga_lua_serialize( ls, &content, 3, &errcode))
		{
			luaL_error( ls, papuga_ErrorCode_tostring( errcode));
		}
		reqhnd->send( requestmethod, url, &content);
	}
	catch (...) {lippincottFunction( ls);}
	return 0;
}

papuga_LuaRequestHandler* papuga_create_LuaRequestHandler(
	const papuga_LuaRequestHandlerFunction* function,
	papuga_RequestContext* context,
	const papuga_Serialization* input,
	papuga_ErrorCode* errcode)
{
	papuga_LuaRequestHandler* rt = 0;
	rt = new (std::nothrow) papuga_LuaRequestHandler( context);
	if (!rt)
	{
		*errcode = papuga_NoMemError;
		return 0;
	}
	LuaDumpReader reader( function->dump());
	int res = lua_load( rt->m_ls, luaDumpReader, (void*)&reader, function->name().c_str(), "b"/*mode binary*/);
	if (res)
	{
		if (res == LUA_ERRSYNTAX)
		{
			*errcode = papuga_SyntaxError;
		}
		else if (res == LUA_ERRMEM)
		{
			*errcode = papuga_NoMemError;
		}
		else
		{
			*errcode = papuga_LogicError;
		}
	}
	return rt;
}

void papuga_delete_LuaRequestHandler( papuga_LuaRequestHandler* self)
{
	delete self;
}

bool papuga_run_LuaRequestHandler( papuga_LuaRequestHandler* handler, papuga_ErrorCode* errcode)
{
	*errcode = papuga_NotImplemented;
	return false;
}

int papuga_LuaRequestHandler_nof_DelegateRequests( const papuga_LuaRequestHandler* handler)
{
	return 0;
}

papuga_DelegateRequest const* papuga_LuaRequestHandler_get_delegateRequest( const papuga_LuaRequestHandler* handler, int idx)
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

