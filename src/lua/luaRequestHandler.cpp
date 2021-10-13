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
#include <csetjmp>

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

static bool isRestHttpRequestMethod( const char* name) noexcept
{
	if (name[0] == 'P' && (0==std::strcmp( name, "POST") || 0==std::strcmp( name, "PUT") || 0==std::strcmp( name, "PATCH"))) return true;
	if (name[0] == 'G' && 0==std::strcmp( name, "GET")) return true;
	if (name[0] == 'D' && 0==std::strcmp( name, "DELETE")) return true;
	return false;
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

static int papuga_lua_http_error( lua_State* ls);
static int papuga_lua_yield( lua_State* ls);
static int papuga_lua_send( lua_State* ls);
static int papuga_lua_document( lua_State* ls);
static int papuga_lua_log( lua_State* ls);
static int papuga_lua_transaction( lua_State* ls);
static int papuga_lua_counter( lua_State* ls);
static int papuga_lua_link( lua_State* ls);
static int papuga_lua_http_accept( lua_State* ls);

static int papuga_lua_schema( lua_State* ls);
static int papuga_lua_content( lua_State* ls);

static int papuga_lua_new_request( lua_State* ls, papuga_DelegateRequest* req);
static int papuga_lua_destroy_request( lua_State* ls);
static int papuga_lua_request_result( lua_State* ls);
static int papuga_lua_request_error( lua_State* ls);

static int papuga_lua_new_context( lua_State* ls, papuga_RequestContextPool* hnd, papuga_RequestContext* ctx);
static int papuga_lua_destroy_context( lua_State* ls);
static int papuga_lua_context_get( lua_State* ls);
static int papuga_lua_context_set( lua_State* ls);
static int papuga_lua_context_inherit( lua_State* ls);


static const struct luaL_Reg g_functionlib [] = {
	{ "doctype", 		papuga_lua_doctype},
	{ "encoding", 		papuga_lua_encoding},
	{nullptr, nullptr} /* end of array */
};

static const struct luaL_Reg g_requestlib [] = {
	{ "http_error", 	papuga_lua_http_error},
	{ "yield", 		papuga_lua_yield},
	{ "send", 		papuga_lua_send},
	{ "document", 		papuga_lua_document},
	{ "log",		papuga_lua_log},
	{ "transaction",	papuga_lua_transaction},
	{ "counter", 		papuga_lua_counter},
	{ "link",		papuga_lua_link},
	{ "http_accept",	papuga_lua_http_accept},
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

struct papuga_LuaRequestHandlerScript
{
public:
	papuga_LuaRequestHandlerScript( std::string&& name_, std::string&& dump_, std::string&& source_, std::string&& options_, std::string&& methods_)
		:m_name(std::move(name_)),m_dump(std::move(dump_)),m_source(std::move(source_)),m_options(std::move(options_)),m_methods(std::move(methods_)){}

	const std::string& name() const noexcept		{return m_name;}
	const std::string& dump() const noexcept 		{return m_dump;}
	const std::string& source() const noexcept 		{return m_source;}
	const std::string& options() const noexcept 		{return m_options;}

	bool hasMethod( const char* name) const noexcept
	{
		size_t namelen = std::strlen( name);
		const char* mt = std::strstr( m_methods.c_str(), name);
		return mt && (mt[ namelen] == ',' || mt[ namelen] == '\n');
	}

private:
	std::string m_name;
	std::string m_dump;
	std::string m_source;
	std::string m_options;
	std::string m_methods;
};

static bool isUppercaseString( char const* si) noexcept
{
	while (*si && *si >= 'A' && *si <= 'Z') ++si;
	return !*si;
}

extern "C" papuga_LuaRequestHandlerScript* papuga_create_LuaRequestHandlerScript(
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
	std::string source( sourcestr);
	if (luaL_loadbuffer( ls, source.c_str(), source.size(), name))
	{
		const char* msg = lua_tostring( ls, -1);
		papuga_ErrorBuffer_reportError( errbuf, "failed to load Lua request handler object '%s': %s", name, msg);
		lua_close( ls);
		return nullptr;
	}
	std::string options; //... comma separated list of uppercase name functions in script
	std::string methods; //... comma separated list of uppercase name functions in script
	lua_rawgeti( ls, LUA_REGISTRYINDEX, LUA_RIDX_GLOBALS);
	lua_pushnil(ls);
	while (lua_next( ls, -2))
	{
		if (lua_isfunction( ls, -1) && lua_isstring( ls, -2))
		{
			const char* fn = lua_tostring( ls, -2);
			if (isUppercaseString( fn))
			{
				if (!methods.empty()) methods.push_back(',');
				methods.append( fn);
			}
			if (isRestHttpRequestMethod( fn))
			{
				if (!options.empty()) options.push_back(',');
				options.append( fn);
			}
		}
		lua_pop( ls, 1);
	}
	lua_pop( ls, 1);
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
	papuga_LuaRequestHandlerScript* rt = nullptr;
	try
	{
		rt = new papuga_LuaRequestHandlerScript( std::string(name), std::move(dump), std::move(source), std::move(options), std::move(methods));
	}
	catch (...)
	{
		papuga_ErrorBuffer_reportError( errbuf, papuga_ErrorCode_tostring( papuga_NoMemError));
		rt = nullptr;
	}
	return rt;
}

extern "C" void papuga_destroy_LuaRequestHandlerScript( papuga_LuaRequestHandlerScript* self)
{
	delete self;
}

extern "C" const char* papuga_LuaRequestHandlerScript_options( papuga_LuaRequestHandlerScript const* self)
{
	return self->options().c_str();
}

extern "C" const char* papuga_LuaRequestHandlerScript_name( papuga_LuaRequestHandlerScript const* self)
{
	return self->name().c_str();
}

extern "C" bool papuga_LuaRequestHandlerScript_implements( papuga_LuaRequestHandlerScript const* self, const char* methodname)
{
	return self->hasMethod( methodname);
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

extern "C" bool papuga_copy_RequestAttributes( papuga_Allocator* allocator, papuga_RequestAttributes* dest, papuga_RequestAttributes const* src)
{
	if (src)
	{
		dest->accepted_encoding_set = src->accepted_encoding_set;
		dest->accepted_doctype_set = src->accepted_doctype_set;
		size_t html_base_href_len = src->html_base_href ? std::strlen( src->html_base_href) : 0;
		while (html_base_href_len > 0 && (src->html_base_href[html_base_href_len-1] == '/' || src->html_base_href[html_base_href_len-1] == '*'))
		{
			html_base_href_len -= 1;
		}
		if (src->html_head)
		{
			dest->html_head = papuga_Allocator_copy_charp( allocator, src->html_head);
			if (!dest->html_head) return false;
		}
		else
		{
			dest->html_head = "";
		}
		if (src->html_base_href)
		{
			dest->html_base_href = papuga_Allocator_copy_string( allocator, src->html_base_href, html_base_href_len);
			if (!dest->html_base_href) return false;
		}
		else
		{
			dest->html_base_href = "";
		}
		dest->beautifiedOutput = src->beautifiedOutput;
		dest->deterministicOutput = src->deterministicOutput;
	}
	else
	{
		dest->accepted_encoding_set = 0x1FF;
		dest->accepted_doctype_set = 0xFF;
		dest->html_head = "";
		dest->html_base_href = "";
		dest->beautifiedOutput = true;
		dest->deterministicOutput = true;
	}
	return true;
}

static papuga_ContentType firstContentTypeFromSet( int doctype_set)
{
	if ((doctype_set & (1<<papuga_ContentType_HTML)) != 0) return papuga_ContentType_HTML;
	else if ((doctype_set & (1<<papuga_ContentType_TEXT)) != 0) return papuga_ContentType_TEXT;
	else if ((doctype_set & (1<<papuga_ContentType_JSON)) != 0) return papuga_ContentType_JSON;
	else if ((doctype_set & (1<<papuga_ContentType_XML)) != 0) return papuga_ContentType_XML;
	else return papuga_ContentType_JSON;
}

extern "C" papuga_ContentType papuga_http_default_doctype( papuga_RequestAttributes* attr)
{
	return firstContentTypeFromSet( attr->accepted_doctype_set);
}

static int parseContentType( const char* tp)
{
	if (0==std::strcmp( tp, "application/octet-stream")) return (1<<papuga_ContentType_Unknown);
	if (0==std::strcmp( tp, "application/json")) return (1<<papuga_ContentType_JSON);
	if (0==std::strcmp( tp, "application/xml")) return (1<<papuga_ContentType_XML);
	if (0==std::strcmp( tp, "application/xhtml+xml")) return (1<<papuga_ContentType_XML)|(1<<papuga_ContentType_HTML);
	if (0==std::strcmp( tp, "application/json+xml")) return (1<<papuga_ContentType_JSON)|(1<<papuga_ContentType_XML);
	if (0==std::strcmp( tp, "application/xml+json")) return (1<<papuga_ContentType_JSON)|(1<<papuga_ContentType_XML);
	if (0==std::strcmp( tp, "application/xhtml+xml+json")) return (1<<papuga_ContentType_JSON)|(1<<papuga_ContentType_XML)|(1<<papuga_ContentType_HTML);
	if (0==std::strcmp( tp, "text/html")) return (1<<papuga_ContentType_HTML);
	if (0==std::strcmp( tp, "text/plain")) return (1<<papuga_ContentType_TEXT);
	if (0==std::strcmp( tp, "text/html+plain")) return (1<<papuga_ContentType_HTML)|(1<<papuga_ContentType_TEXT);
	if (0==std::strcmp( tp, "text/plain+html")) return (1<<papuga_ContentType_HTML)|(1<<papuga_ContentType_TEXT);
	return (1<<papuga_ContentType_JSON);
}

static int parseHttpAccept( char const* si)
{
	int rt = 0;
	char buf[ 128];
	buf[ 0] = 0;
	while (si && *si)
	{
		for (;*si && (unsigned char)*si <= 32; ++si){}
		char const* start = si;
		char const* next = nullptr;
		for (;*si && *si != ',' && *si != ';'; ++si){}
		if (*si == ',' || *si == ';')
		{
			if (*si == ';')
			{
				next = std::strchr( si, ',');
				if (next) ++next;
			}
			else
			{
				next = si+1;
			}
			for (;si != start && (unsigned char)*(si-1) <= 32; --si){}
			if (si - start < sizeof(buf))
			{
				std::memcpy( buf, start, si-start);
				buf[ si-start] = 0;
			}
			for (char* lp = buf; *lp; ++lp){*lp = *lp | 32;}
			rt |= parseContentType( buf);
			si = next;
		}
		else
		{
			rt |= parseContentType( buf);
		}
	}
	return rt;
}

extern "C" void papuga_init_RequestAttributes( papuga_RequestAttributes* dest, const char* http_accept, const char* html_head, const char* html_base_href, bool beautifiedOutput, bool deterministicOutput)
{
	dest->accepted_encoding_set = 0xFFff;
	dest->accepted_doctype_set = parseHttpAccept( http_accept);
	dest->html_head = html_head;
	dest->html_base_href = html_base_href;
	dest->beautifiedOutput = beautifiedOutput;
	dest->deterministicOutput = deterministicOutput;
}

static void copyTransactionHandler( papuga_TransactionHandler* dest, const papuga_TransactionHandler* src)
{
	if (src)
	{
		std::memcpy( dest, src, sizeof(papuga_TransactionHandler));
	}
	else
	{
		std::memset( dest, 0, sizeof(papuga_TransactionHandler));
	}
}

static void copyLogger( papuga_Logger* dest, const papuga_Logger* src)
{
	if (src)
	{
		std::memcpy( dest, src, sizeof(papuga_Logger));
	}
	else
	{
		std::memset( dest, 0, sizeof(papuga_Logger));
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
	papuga_RequestContextPool* m_handler;
	papuga_RequestContext* m_context;
	const char* m_contextName;
	papuga_TransactionHandler m_transactionHandler;
	papuga_Logger m_logger;
	papuga_RequestAttributes m_attributes;
	papuga_Allocator m_allocator;
	papuga_DelegateRequest m_delegate[ MaxNofDelegates];
	int m_nof_delegates;
	int m_start_delegates;
	papuga_LuaRequestResult m_result;
	papuga_ContentType m_doctype;
	papuga_StringEncoding m_encoding;
	int m_contentDefined;
	int m_allocatormem[ 1<<14];
	std::jmp_buf m_jump_buffer_LuaRequestHandler_init;
	lua_CFunction m_oldpanic;

	#define REQUESTHANDLER_VARIABLE "__requestHandler"
	static int atPanic( lua_State* ls)
	{
		lua_getglobal( ls, REQUESTHANDLER_VARIABLE);
		papuga_LuaRequestHandler* reqhnd = (papuga_LuaRequestHandler*)lua_touserdata(ls, -1);
		if (!reqhnd)
		{
			throw std::logic_error("No panic function context found");
		}
		longjmp( reqhnd->m_jump_buffer_LuaRequestHandler_init, 1); 
		return 0;
	}

	papuga_LuaRequestHandler(
			papuga_LuaInitProc* initproc,
			papuga_RequestContextPool* handler_,
			papuga_RequestContext* context_, 
			const char* contextName_,
			papuga_TransactionHandler* transactionHandler_,
			papuga_Logger* logger_,
			const papuga_RequestAttributes* attributes_,
			const papuga_SchemaMap* schemamap)
		:m_ls(nullptr),m_thread(nullptr),m_threadref(0),m_running(false)
		,m_handler(handler_),m_context(context_),m_contextName(nullptr),m_nof_delegates(0),m_start_delegates(0)
		,m_doctype(papuga_ContentType_Unknown),m_encoding(papuga_Binary),m_contentDefined(0)
	{
		m_result.contentlen = 0;
		m_result.contentstr = nullptr;
		m_result.doctype = papuga_ContentType_Unknown;
		m_result.encoding = papuga_Binary;
		papuga_init_Allocator( &m_allocator, m_allocatormem, sizeof(m_allocatormem));
		m_ls = lua_newstate( customAllocFunction, this);
		if (setjmp( m_jump_buffer_LuaRequestHandler_init) != 0)
		{
			lua_close( m_ls);
			papuga_destroy_Allocator( &m_allocator);
			throw std::runtime_error( papuga_ErrorCode_tostring( papuga_BindingLanguageError));
		}
		lua_pushlightuserdata( m_ls, this);
		lua_setglobal( m_ls, REQUESTHANDLER_VARIABLE);
		m_oldpanic = lua_atpanic( m_ls, &atPanic);
		luaL_openlibs( m_ls);
		createMetatable( m_ls, g_request_metatable_name, g_request_methods);
		createMetatable( m_ls, g_context_metatable_name, g_context_methods);
		copyTransactionHandler( &m_transactionHandler, transactionHandler_);
		copyLogger( &m_logger, logger_);
		if (!papuga_copy_RequestAttributes( &m_allocator, &m_attributes, attributes_))
		{
			luaL_error( m_ls, papuga_ErrorCode_tostring( papuga_NoMemError));
		}
		lua_getglobal( m_ls, "_G");
		lua_pushlightuserdata( m_ls, this);
		luaL_setfuncs( m_ls, g_requestlib, 1/*number of closure elements*/);
		luaL_setfuncs( m_ls, g_functionlib, 0/*number of closure elements*/);
		lua_pushlightuserdata( m_ls, this);
		lua_pushlightuserdata( m_ls, const_cast<papuga_SchemaMap*>(schemamap));
		lua_pushcclosure( m_ls, papuga_lua_schema, 2/*number of closure elements*/);
		lua_setglobal( m_ls, "schema");
		lua_pushlightuserdata( m_ls, this);
		lua_pushcclosure( m_ls, papuga_lua_content, 1/*number of closure elements*/);
		lua_setglobal( m_ls, "content");
		if (contextName_)
		{
			m_contextName = papuga_Allocator_copy_charp( &m_allocator, contextName_);
		}
		lua_pop( m_ls, 1);
		if (initproc)
		{
			(*initproc)( m_ls);
		}
		lua_atpanic( m_ls, m_oldpanic);
	}

	bool init(
			const papuga_LuaRequestHandlerScript* script,
			const char* requestmethod,
			const char* requestpath,
			const char* contentstr,
			std::size_t contentlen,
			papuga_ErrorCode* errcode) noexcept
	{
		LuaDumpReader reader( script->dump());
		int res = lua_load( m_ls, luaDumpReader, (void*)&reader, script->name().c_str(), "b"/*mode binary*/);
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
		if (!m_thread)
		{
			*errcode = errorFromLuaErrcode( res);
			return false;
		}
		lua_pushvalue( m_ls, -1);
		m_threadref = luaL_ref( m_ls, LUA_REGISTRYINDEX);
		lua_getglobal( m_thread, requestmethod);
		if (lua_type( m_thread, -1) != LUA_TFUNCTION)
		{
			*errcode = papuga_AddressedItemNotFound;
			lua_pop( m_thread, 1);
			return false;
		}
		papuga_lua_new_context( m_thread, m_handler, m_context);
		if (contentlen > 0)
		{
			lua_pushlstring( m_thread, contentstr, contentlen);
		}
		else
		{
			lua_pushnil( m_thread);
		}
		if (requestpath && requestpath[0] != '\0')
		{
			lua_pushstring( m_thread, requestpath);
		}
		else
		{
			lua_pushnil( m_thread);
		}
		if (m_contextName)
		{
			lua_pushstring( m_thread, m_contextName);
		}
		else
		{
			lua_pushnil( m_thread);
		}
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
		int nof_args = m_running ? 0 : 4;
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
					const char* rootname = nullptr;
					papuga_init_ValueVariant( &resultval);

					if (lua_type( m_thread, -1) == LUA_TTABLE)
					{
						lua_pushvalue( m_thread, -1);
						lua_pushnil( m_thread);
						int nofRows = 0;

						while (lua_next( m_thread, -2))
						{
							if (++nofRows > 1)
							{
								papuga_ErrorBuffer_reportError( errbuf, papuga_ErrorCode_tostring( papuga_InvalidOutput));
								return false;
							}
							if (lua_type( m_thread, -2) == LUA_TSTRING)
							{
								if (lua_type( m_thread, -1) == LUA_TTABLE)
								{
									rootname = lua_tostring( m_thread, -2);
									if (!papuga_lua_value( m_thread, &resultval, &m_allocator, -1, &errcode))
									{
										papuga_ErrorBuffer_reportError( errbuf, papuga_ErrorCode_tostring( errcode));
										return false;
									}
								}
								else
								{
									if (!papuga_lua_value( m_thread, &resultval, &m_allocator, -3, &errcode))
									{
										papuga_ErrorBuffer_reportError( errbuf, papuga_ErrorCode_tostring( errcode));
										return false;
									}
								}
							}
							else
							{
								papuga_ErrorBuffer_reportError( errbuf, papuga_ErrorCode_tostring( errcode));
								return false;
							}
							lua_pop( m_thread, 1);
						}
						lua_pop( m_thread, 1);
					}
					else if (!papuga_lua_value( m_thread, &resultval, &m_allocator, -1, &errcode))
					{
						papuga_ErrorBuffer_reportError( errbuf, papuga_ErrorCode_tostring( errcode));
						return false;
					}
					if (m_attributes.deterministicOutput && resultval.valuetype == papuga_TypeSerialization)
					{
						papuga_init_Serialization( &detser, &m_allocator);
						if (!papuga_Serialization_copy_deterministic( &detser, resultval.value.serialization, &errcode))
						{
							papuga_ErrorBuffer_reportError( errbuf, papuga_ErrorCode_tostring( errcode));
							return false;
						}
						resultval.value.serialization = &detser;
					}
					if (!initResult( rootname, resultval, &errcode))
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

	papuga_DelegateRequest* send( const char* requestmethod, const char* requesturl, papuga_ValueVariant* content)
	{
		const char* rootname = nullptr;
		const char* elemname = nullptr;
		if (m_nof_delegates >= MaxNofDelegates)
		{
			throw std::runtime_error( papuga_ErrorCode_tostring( papuga_NoMemError));
		}
		papuga_DelegateRequest* req = &m_delegate[ m_nof_delegates];
		std::memset( req, 0, sizeof(papuga_DelegateRequest));
		req->requestmethod = papuga_Allocator_copy_charp( &m_allocator, requestmethod);
		req->requesturl = papuga_Allocator_copy_charp( &m_allocator, requesturl);
		if (!req->requestmethod || !req->requesturl)
		{
			throw std::bad_alloc();
		}
		req->errcode = papuga_Ok;
		req->errmsg = nullptr;
		req->resultstr = nullptr;
		req->resultlen = 0;
		if (content->valuetype == papuga_TypeString)
		{
			req->contentstr = content->value.string;
			req->contentlen = content->length;
		}
		else
		{
			req->contentstr = (char*)papuga_ValueVariant_tojson(
						content, &m_allocator, nullptr/*structdefs*/,
						papuga_UTF8, m_attributes.beautifiedOutput,
						rootname, elemname, &req->contentlen, &req->errcode);
			if (!req->contentstr)
			{
				throw std::runtime_error( papuga_ErrorCode_tostring( req->errcode));
			}
		}
		m_nof_delegates += 1;
		return req;
	}

	const char* linkBase( char* buf, size_t bufsize) const noexcept
	{
		char const* hi = m_attributes.html_base_href;
		for (; *hi && *hi != ':'; ++hi){}
		for (++hi; *hi && *hi == '/'; ++hi){}
		for (++hi; *hi && *hi != '/'; ++hi){}
		size_t size = hi - m_attributes.html_base_href;
		if (size >= bufsize)
		{
			return nullptr;
		}
		std::memcpy( buf, m_attributes.html_base_href, size);
		buf[ size] = 0;
		return buf;
	}

	~papuga_LuaRequestHandler()
	{
		if (m_ls) {lua_close( m_ls); m_ls = nullptr;}
		papuga_destroy_Allocator( &m_allocator);
	}

	void setDocumentType( papuga_ContentType doctype_, papuga_StringEncoding encoding_) noexcept
	{
		if (m_contentDefined == 0)
		{
			if ((m_attributes.accepted_doctype_set & (1 << (int)doctype_)) != 0
			&&  (m_attributes.accepted_encoding_set & (1 << (int)encoding_)) != 0)
			{
				m_doctype = doctype_;
				m_encoding = encoding_;
				m_contentDefined = 1;
			}
		}
		else if (m_contentDefined == 1)
		{
			if (m_doctype != doctype_ || m_encoding != encoding_)
			{
				if ((m_attributes.accepted_doctype_set & (1 << (int)doctype_)) != 0
				&&  (m_attributes.accepted_encoding_set & (1 << (int)encoding_)) != 0)
				{
					m_contentDefined = 2;
				}
			}
		}
	}

	papuga_ContentType resultContentType() const noexcept
	{
		papuga_ContentType rt = m_doctype;
		if (rt == papuga_ContentType_Unknown)
		{
			rt = firstContentTypeFromSet( m_attributes.accepted_doctype_set);
		}
		return rt;
	}

	papuga_StringEncoding resultEncoding() const noexcept
	{
		papuga_StringEncoding rt = m_encoding;
		if (rt == papuga_Binary)
		{
			if ((m_attributes.accepted_encoding_set & (1<<papuga_UTF8)) != 0) rt = papuga_UTF8;
			else if ((m_attributes.accepted_encoding_set & (1<<papuga_UTF16BE)) != 0) rt = papuga_UTF16BE;
			else if ((m_attributes.accepted_encoding_set & (1<<papuga_UTF16LE)) != 0) rt = papuga_UTF16LE;
			else if ((m_attributes.accepted_encoding_set & (1<<papuga_UTF32BE)) != 0) rt = papuga_UTF32BE;
			else if ((m_attributes.accepted_encoding_set & (1<<papuga_UTF32LE)) != 0) rt = papuga_UTF32LE;
			else if ((m_attributes.accepted_encoding_set & (1<<papuga_UTF32)) != 0) rt = papuga_UTF32;
			else rt = papuga_UTF8;
		}
		return rt;
	}

	bool initResult( const char* rootname, const papuga_ValueVariant& result, papuga_ErrorCode* errcode)
	{
		const char* elemname = nullptr;
		m_result.encoding = papuga_Binary;
		m_result.doctype = papuga_ContentType_Unknown;
		if (!papuga_ValueVariant_defined( &result))
		{
			*errcode = papuga_Ok;
			m_result.contentstr = nullptr;
			m_result.contentlen = 0;
			return true;
		}
		else if (papuga_ValueVariant_isstring( &result))
		{
			*errcode = papuga_Ok;
			m_result.contentstr = result.value.string;
			m_result.contentlen = result.length;
			return true;
		}
		else
		{
			m_result.encoding = resultEncoding();
			m_result.doctype = resultContentType();
			switch (m_result.doctype)
			{
				case papuga_ContentType_Unknown:
				case papuga_ContentType_JSON:
					m_result.contentstr = (char*)papuga_ValueVariant_tojson(
							&result, &m_allocator, nullptr/*structdefs*/,
							m_result.encoding, m_attributes.beautifiedOutput, rootname, elemname,
							&m_result.contentlen, errcode);
					break;
				case papuga_ContentType_XML:
					m_result.contentstr = (char*)papuga_ValueVariant_toxml(
							&result, &m_allocator, nullptr/*structdefs*/,
							m_result.encoding, m_attributes.beautifiedOutput, rootname, elemname,
							&m_result.contentlen, errcode);
					break;
				case papuga_ContentType_HTML:
					m_result.contentstr = (char*)papuga_ValueVariant_tohtml5(
							&result, &m_allocator, nullptr/*structdefs*/,
							m_result.encoding, m_attributes.beautifiedOutput, rootname, elemname,
							m_attributes.html_head, m_attributes.html_base_href,
							&m_result.contentlen, errcode);
					break;
				case papuga_ContentType_TEXT:
					m_result.contentstr = (char*)papuga_ValueVariant_totext(
							&result, &m_allocator, nullptr/*structdefs*/,
							m_result.encoding, m_attributes.beautifiedOutput, rootname, elemname,
							&m_result.contentlen, errcode);
					break;
			}
			return true;
		}
		*errcode = papuga_UnknownContentType;
		m_result.contentstr = nullptr;
		m_result.contentlen = 0;
		return false;
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
		if (nargs != 1)
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

		if (!papuga_Serialization_append_json( &resultser, req->resultstr, req->resultlen, papuga_UTF8, true/*with root*/, &errcode))
		{
			papuga_destroy_Allocator( &allocator);
			luaL_error( ls, papuga_ErrorCode_tostring( errcode));
		}
		papuga_init_ValueVariant_serialization( &resultval, &resultser);
		if (!papuga_lua_push_value( ls, &resultval, &errcode))
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
		if (nargs != 1)
		{
			luaL_error( ls, papuga_ErrorCode_tostring( papuga_NofArgsError));
		}
		if ((*td)->errcode != papuga_Ok)
		{
			lua_pushinteger( ls, (*td)->errcode);
			lua_pushstring( ls, papuga_ErrorCode_tostring( (*td)->errcode));
			return 2;
		}
	}
	catch (...) { lippincottFunction( ls); }
	return 0;
}

struct LuaRequestContext
{
	papuga_RequestContextPool* hnd;
	papuga_RequestContext* ctx;
};

static int papuga_lua_new_context( lua_State* ls, papuga_RequestContextPool* hnd, papuga_RequestContext* ctx)
{
	LuaRequestContext* td = (LuaRequestContext*)lua_newuserdata( ls, sizeof(LuaRequestContext));
	td->hnd = hnd;
	td->ctx = ctx;
	luaL_getmetatable( ls, g_context_metatable_name);
	if (lua_isnil( ls, -1))
	{
		luaL_error( ls, papuga_ErrorCode_tostring( papuga_LogicError));
	}
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
				if (!papuga_lua_push_value( ls, value, &errcode))
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

static int papuga_lua_http_error( lua_State* ls)
{
	papuga_LuaRequestHandler* reqhnd = (papuga_LuaRequestHandler*)lua_touserdata(ls, lua_upvalueindex(1));
	if (!reqhnd)
	{
		luaL_error( ls, papuga_ErrorCode_tostring( papuga_LogicError));
	}
	try
	{
		int nargs = lua_gettop( ls);
		if (nargs != 1)
		{
			luaL_error( ls, papuga_ErrorCode_tostring( papuga_NofArgsError));
		}
		int http_error_id = -1;
		if (lua_type( ls, 1) == LUA_TSTRING)
		{
			const char* http_error_str = lua_tostring( ls, 1);
			if (!!std::strstr( http_error_str, "nternal")) http_error_id = 500;
			else if (!!std::strstr( http_error_str, "mplement")) http_error_id = 501;
			else if (!!std::strstr( http_error_str, "ateway")) http_error_id = 502;
			else if (!!std::strstr( http_error_str, "navailable")) http_error_id = 503;
			else if (!!std::strstr( http_error_str, "imeout")) http_error_id = 504;
			else if (!!std::strstr( http_error_str, "ersion")) http_error_id = 505;
			else if (!!std::strstr( http_error_str, "uthentication")) http_error_id = 511;
			else if (!!std::strstr( http_error_str, "equest")) http_error_id = 400;
			else if (!!std::strstr( http_error_str, "nauthorized")) http_error_id = 401;
			else if (!!std::strstr( http_error_str, "ayment")) http_error_id = 402;
			else if (!!std::strstr( http_error_str, "orbidden")) http_error_id = 403;
			else if (!!std::strstr( http_error_str, "ound")) http_error_id = 404;
			else if (!!std::strstr( http_error_str, "llowed")) http_error_id = 405;
		}
		else
		{
			http_error_id = lua_tointeger( ls, 1);
		}
		papuga_ErrorCode ec = papuga_InvalidAccess;
		switch (http_error_id)
		{
			case 0: ec = papuga_ValueUndefined; break;
			case 500: ec = papuga_ServiceImplementationError; break;
			case 501: ec = papuga_NotImplemented; break;
			case 400: ec = papuga_InvalidRequest; break;
			case 403: ec = papuga_InvalidRequest; break;
			case 404: ec = papuga_InvalidRequest; break;
			case 405: ec = papuga_InvalidRequest; break;
		}
		luaL_error( ls, papuga_ErrorCode_tostring( ec));
	}
	catch (...) {lippincottFunction( ls);}
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
		const char* requesturl = lua_tostring( ls, 2);
		if (!papuga_lua_value( ls, &value, &reqhnd->m_allocator, 3, &errcode))
		{
			luaL_error( ls, papuga_ErrorCode_tostring( errcode));
		}
		papuga_DelegateRequest* req = reqhnd->send( requestmethod, requesturl, &value);
		return papuga_lua_new_request( ls, req);
	}
	catch (...) {lippincottFunction( ls);}
	return 0;
}

static int papuga_lua_document( lua_State* ls)
{
	const char* rootname = nullptr;
	const char* elemname = nullptr;
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
					encoding, reqhnd->m_attributes.beautifiedOutput,
					rootname, elemname, &doclen, &errcode);
			break;
		case papuga_ContentType_XML:
			docstr = (char*)papuga_ValueVariant_toxml(
					&doc, &reqhnd->m_allocator, nullptr/*structdefs*/,
					encoding, reqhnd->m_attributes.beautifiedOutput,
					rootname, elemname, &doclen, &errcode);
			break;
		case papuga_ContentType_HTML:
			docstr = (char*)papuga_ValueVariant_tohtml5(
					&doc, &reqhnd->m_allocator, nullptr/*structdefs*/,
					encoding, reqhnd->m_attributes.beautifiedOutput,
					rootname, elemname, ""/*head*/,""/*href_base*/,
					&doclen, &errcode);
			break;
		case papuga_ContentType_TEXT:
			docstr = (char*)papuga_ValueVariant_totext(
					&doc, &reqhnd->m_allocator, nullptr/*structdefs*/,
					encoding, reqhnd->m_attributes.beautifiedOutput,
					rootname, elemname, &doclen, &errcode);
			break;
	}
	if (!docstr)
	{
		luaL_error( ls, papuga_ErrorCode_tostring( errcode));
	}
	lua_pushlstring( ls, docstr, doclen);
	return 1;
}

static int papuga_lua_log( lua_State* ls)
{
	papuga_LuaRequestHandler* reqhnd = (papuga_LuaRequestHandler*)lua_touserdata(ls, lua_upvalueindex(1));	
	int nn = lua_gettop( ls);
	if (!reqhnd->m_logger.log)
	{
		return 0;
	}
	if (nn != 3)
	{
		luaL_error( ls, papuga_ErrorCode_tostring( papuga_NofArgsError));
	}
	if (lua_type( ls, 1) != LUA_TSTRING || lua_type( ls, 2) != LUA_TSTRING)
	{
		luaL_error( ls, papuga_ErrorCode_tostring( papuga_TypeError));
	}
	const char* rootname = nullptr;
	const char* elemname = nullptr;
	char const* level = lua_tostring( ls, 1);
	char const* tag = lua_tostring( ls, 2);
	char const* contentstr;
	size_t contentlen = 0;
	papuga_ValueVariant contentval;
	papuga_ErrorCode errcode = papuga_Ok;
	if (lua_type( ls, 3) == LUA_TSTRING)
	{
		contentstr = lua_tolstring( ls, 3, &contentlen);
	}
	else if (papuga_lua_value( ls, &contentval, &reqhnd->m_allocator, 3, &errcode))
	{
		contentstr = (char const*)papuga_ValueVariant_tojson(
						&contentval, &reqhnd->m_allocator, nullptr/*structdefs*/,
						papuga_UTF8, reqhnd->m_attributes.beautifiedOutput,
						rootname, elemname, &contentlen, &errcode);
		if (!contentstr)
		{
			luaL_error( ls, papuga_ErrorCode_tostring( papuga_TypeError));
		}
	}
	else
	{
		luaL_error( ls, papuga_ErrorCode_tostring( errcode));
	}
	reqhnd->m_logger.log( reqhnd->m_logger.self, level, tag, contentstr, contentlen);
	return 0;
}

static int papuga_lua_transaction( lua_State* ls)
{
	papuga_LuaRequestHandler* reqhnd = (papuga_LuaRequestHandler*)lua_touserdata(ls, lua_upvalueindex(1));	
	int nn = lua_gettop( ls);
	if (!reqhnd->m_transactionHandler.create)
	{
		luaL_error( ls, papuga_ErrorCode_tostring( papuga_NotImplemented));
	}
	if (nn != 2)
	{
		luaL_error( ls, papuga_ErrorCode_tostring( papuga_NofArgsError));
	}
	if (lua_type( ls, 1) != LUA_TSTRING)
	{
		luaL_error( ls, papuga_ErrorCode_tostring( papuga_TypeError));
	}
	papuga_ErrorCode errcode = papuga_Ok;
	const char* typenam = lua_tostring( ls, 1);
	papuga_RequestContext* transactionContext = papuga_create_RequestContext();
	if (!transactionContext)
	{
		luaL_error( ls, papuga_ErrorCode_tostring( papuga_NoMemError));
	}
	papuga_ValueVariant selfval;
	papuga_Allocator allocator;
	int allocatormem[ 2048];
	papuga_init_Allocator( &allocator, allocatormem, sizeof(allocatormem));
	if (!papuga_lua_value( ls, &selfval, &allocator, 2, &errcode))
	{
		papuga_destroy_RequestContext( transactionContext);
		papuga_destroy_Allocator( &allocator);
		luaL_error( ls, papuga_ErrorCode_tostring( errcode));
	}
	if (!papuga_RequestContext_define_variable( transactionContext, "self", &selfval))
	{
		papuga_destroy_RequestContext( transactionContext);
		papuga_destroy_Allocator( &allocator);
		luaL_error( ls, papuga_ErrorCode_tostring( papuga_NoMemError));
	}
	const char* tid = reqhnd->m_transactionHandler.create( reqhnd->m_transactionHandler.self, typenam, transactionContext, &allocator);
	if (!tid)
	{
		papuga_destroy_Allocator( &allocator);
		luaL_error( ls, papuga_ErrorCode_tostring( papuga_NoMemError));
	}
	char serverbuf[ 1024];
	char linkbuf[ 1024];
	const char* serverid = reqhnd->linkBase( serverbuf, sizeof(serverbuf));
	if (!serverid)
	{
		papuga_destroy_Allocator( &allocator);
		luaL_error( ls, papuga_ErrorCode_tostring( papuga_BufferOverflowError));
	}
	size_t linksize = std::snprintf( linkbuf, sizeof(linkbuf), "%s/transaction/%s", serverid, tid);
	papuga_destroy_Allocator( &allocator);
	if (linksize >= sizeof(linkbuf))
	{
		luaL_error( ls, papuga_ErrorCode_tostring( papuga_BufferOverflowError));
	}
	lua_pushstring( ls, linkbuf);
	return 1;
}

static int papuga_lua_counter( lua_State* ls)
{
	papuga_LuaRequestHandler* reqhnd = (papuga_LuaRequestHandler*)lua_touserdata(ls, lua_upvalueindex(1));	
	int nn = lua_gettop( ls);
	if (!reqhnd->m_transactionHandler.counter)
	{
		luaL_error( ls, papuga_ErrorCode_tostring( papuga_NotImplemented));
	}
	if (nn != 1)
	{
		luaL_error( ls, papuga_ErrorCode_tostring( papuga_NofArgsError));
	}
	if (lua_type( ls, 1) != LUA_TSTRING)
	{
		luaL_error( ls, papuga_ErrorCode_tostring( papuga_TypeError));
	}
	const char* typenam = lua_tostring( ls, 1);
	int tid = reqhnd->m_transactionHandler.counter( reqhnd->m_transactionHandler.self, typenam);
	if (!tid)
	{
		luaL_error( ls, papuga_ErrorCode_tostring( papuga_NoMemError));
	}
	lua_pushinteger( ls, tid);
	return 1;
}

static int papuga_lua_link( lua_State* ls)
{
	papuga_LuaRequestHandler* reqhnd = (papuga_LuaRequestHandler*)lua_touserdata(ls, lua_upvalueindex(1));	
	int nn = lua_gettop( ls);
	if (nn != 1)
	{
		luaL_error( ls, papuga_ErrorCode_tostring( papuga_NofArgsError));
	}
	if (lua_type( ls, 1) != LUA_TSTRING)
	{
		luaL_error( ls, papuga_ErrorCode_tostring( papuga_TypeError));
	}
	const char* path = lua_tostring( ls, 1);

	char serverbuf[ 1024];
	char linkbuf[ 1024];
	const char* serverid = reqhnd->linkBase( serverbuf, sizeof(serverbuf));
	if (!serverid)
	{
		luaL_error( ls, papuga_ErrorCode_tostring( papuga_BufferOverflowError));
	}
	size_t linksize = std::snprintf( linkbuf, sizeof(linkbuf), "%s/%s", serverid, path);
	if (linksize >= sizeof(linkbuf))
	{
		luaL_error( ls, papuga_ErrorCode_tostring( papuga_BufferOverflowError));
	}
	lua_pushstring( ls, linkbuf);
	return 1;
}

static int papuga_lua_http_accept( lua_State* ls)
{
	papuga_LuaRequestHandler* reqhnd = (papuga_LuaRequestHandler*)lua_touserdata(ls, lua_upvalueindex(1));
	int nn = lua_gettop( ls);
	if (nn != 0)
	{
		luaL_error( ls, papuga_ErrorCode_tostring( papuga_NofArgsError));
	}
	{
		lua_newtable( ls);
		int st = reqhnd->m_attributes.accepted_doctype_set;
		int si = 0;
		int ri = 0;
		while (st)
		{
			if ((st & (1 << si)) != 0)
			{
				lua_pushstring( ls, papuga_ContentType_mime( (papuga_ContentType)si));
				lua_rawseti( ls, -2, ++ri);
			}
			st &= ~(1 << si);
			si += 1;
		}
	}{
		lua_newtable( ls);
		int st = reqhnd->m_attributes.accepted_encoding_set;
		int si = 0;
		int ri = 0;
		while (st)
		{
			if ((st & (1 << si)) != 0)
			{
				lua_pushstring( ls, papuga_StringEncoding_name( (papuga_StringEncoding)si));
				lua_rawseti( ls, -2, ++ri);
			}
			st &= ~(1 << si);
			si += 1;
		}
	}
	return 2;
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

static int papuga_lua_content( lua_State* ls)
{
	papuga_LuaRequestHandler* reqhnd = (papuga_LuaRequestHandler*)lua_touserdata(ls, lua_upvalueindex(1));	

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

	lua_pushstring( ls, papuga_ContentType_name( doctype));
	lua_pushstring( ls, papuga_stringEncodingName( encoding));
	return 2;
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
		if (nn < 2 || nn > 3)
		{
			luaL_error( ls, papuga_ErrorCode_tostring( papuga_NofArgsError));
		}
		if (lua_type( ls, 1) != LUA_TSTRING || lua_type( ls, 2) != LUA_TSTRING)
		{
			luaL_error( ls, papuga_ErrorCode_tostring( papuga_TypeError));
		}
		bool with_rootelem = true;
		if (nn == 3)
		{
			with_rootelem = lua_toboolean( ls, 3);
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
		if (!papuga_schema_parse( &contentser, schema, with_rootelem, doctype, encoding, contentstr, contentlen, &schemaerr))
		{
			papuga_destroy_Allocator( &allocator);
			lua_schema_error( ls, schemaerr);
		}
		bool serr = papuga_lua_push_serialization( ls, &contentser, &errcode);
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
	const papuga_LuaRequestHandlerScript* script,
	papuga_LuaInitProc* initproc,
	const papuga_SchemaMap* schemamap,
	papuga_RequestContextPool* contextpool,
	papuga_RequestContext* requestcontext,
	papuga_TransactionHandler* transactionHandler,
	papuga_Logger* logger,
	const papuga_RequestAttributes* attributes,
	const char* requestmethod,
	const char* contextname,
	const char* requestpath,
	const char* contentstr,
	std::size_t contentlen,
	papuga_ErrorCode* errcode)
{
	papuga_LuaRequestHandler* rt = 0;
	rt = new (std::nothrow) papuga_LuaRequestHandler( initproc, contextpool, requestcontext, contextname, transactionHandler, logger, attributes, schemamap);
	if (!rt)
	{
		*errcode = papuga_NoMemError;
		return 0;
	}
	if (!rt->init( script, requestmethod, requestpath, contentstr, contentlen, errcode))
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

extern "C" const papuga_LuaRequestResult* papuga_LuaRequestHandler_get_result( const papuga_LuaRequestHandler* handler)
{
	return handler->m_result.doctype == papuga_ContentType_Unknown ? nullptr : &handler->m_result;
}

