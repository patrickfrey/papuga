/*
 * Copyright (c) 2021 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
#include "papuga.hpp"
#include "papuga/luaRequestHandler.h"
#include "papuga/requestParser.h"
#include "papuga/requestHandler.h"
#include "papuga/valueVariant.h"
#include "papuga/schema.h"
#include "papuga/errors.h"
#include "papuga/classdef.h"
#include <string>
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <vector>
#include <sstream>
#include <iostream>
#include <fstream>
#include <stdexcept>
#include <algorithm>
#include <map>
#include <sys/types.h>
#include <dirent.h>

#undef PAPUGA_LOWLEVEL_DEBUG

static bool g_verbose = false;

static std::string readFile( const std::string& path)
{
	std::string rt;
	std::ifstream inFile;
	inFile.open( path);
	if (!inFile) throw std::runtime_error( std::string("failed to open file '") + path + "'");
	std::string line;
	while (std::getline( inFile, line))
	{
		rt.append( line);
		rt.append( "\n");
	}
	inFile.close();
	return rt;
}

static size_t countLines( const std::string& source)
{
	size_t rt = 0;
	auto si = source.begin(), se = source.end();
	for (; si != se; ++si)
	{
		if (*si == '\n')
		{
			++rt;
		}
	}
	return rt;
}

static std::string joinPath( const std::string& path, const std::string& name)
{
#ifdef _WIN32
	static const char pathSeparator = '\\';
#else
	static const char pathSeparator = '/';
#endif

	return path + pathSeparator + name;
}

static void printUsage()
{
	std::cerr << "testLuaRequest <scriptdir> <schemadir> <method> <object> <input> <expect>\n"
			<< "\t<scriptdir>    :Lua script directory (service definitions)\n"
			<< "\t<schemadir>    :Schema description directory (schema definitions)\n"
			<< "\t<method>       :Request method\n"
			<< "\t<object>       :Service object file (basename of Lua script)\n"
			<< "\t<input>        :Input to process\n"
			<< "\t<expect>       :Expected output\n"
	<< std::endl;
}

static std::string baseName( const std::string& filenam)
{
	char const* ei = filenam.c_str() + filenam.size();
	for (; ei != filenam.c_str() && *ei != '/' && *ei != '\\' && *ei != '.'; --ei){}
	char const* fi;
	if (*ei != '.')
	{
		fi = ei;
		ei = filenam.c_str() + filenam.size();
	}
	else
	{
		fi = ei;
		for (; fi != filenam.c_str() && *fi != '/' && *fi != '\\'; --fi){}
		if (*fi == '/' || *fi == '\\') {++fi;}
	}
	return std::string( fi, ei-fi);
}

std::vector<std::string> readDirFiles( const std::string& path, const std::string& ext)
{
	std::vector<std::string> rt;
	DIR *dir = ::opendir( path.c_str());
	struct dirent *ent;
	char errbuf[ 2014];

	if (!dir)
	{
		std::snprintf( errbuf, sizeof(errbuf),
				"Failed to open directory '%s': %s", path.c_str(), ::strerror( errno));
		throw std::runtime_error( errbuf);
	}
	while (!!(ent = ::readdir(dir)))
	{
		if (ent->d_name[0] == '.') continue;
		std::string entry( ent->d_name);
		if (ext.size() > entry.size())
		{
			continue;
		}
		if (ext.empty() && entry[0] != '.')
		{
			rt.push_back( entry);
		}
		else if (ext.size() < entry.size())
		{
			const char* ee = entry.c_str() + entry.size() - ext.size();
			if (0==std::memcmp( ee, ext.c_str(), ext.size()))
			{
				rt.push_back( entry );
			}
		}
	}
	std::sort( rt.begin(), rt.end(), std::less<std::string>());
	::closedir(dir);
	return rt;
}

static const papuga_lua_ClassEntryMap* g_cemap = nullptr;
static const papuga_ClassDef g_classdefs[1] = {papuga_ClassDef_NULL};

class GlobalContext
{
public:
	GlobalContext( const std::string& schemaDir, const std::string& scriptDir)
		:m_requestHandler(nullptr),m_schemaList(nullptr),m_schemaMap(nullptr),m_objectMap()
	{
		m_requestHandler = papuga_create_RequestHandler( g_classdefs);
		if (!m_requestHandler) throw std::bad_alloc();
		try
		{
			loadSchemas( schemaDir);
			loadFunctions( scriptDir);
		}
		catch (...)
		{
			deinit();
			throw;
		}
	}

	~GlobalContext()
	{
		deinit();
	}

	const papuga_LuaRequestHandlerObject* object( const char* name) const
	{
		auto fi = m_objectMap.find( name);
		if (fi == m_objectMap.end())
		{
			throw std::runtime_error( std::string( "undefined object '") + name + "'");
		}
		return fi->second;
	}

	const papuga_SchemaMap* schemaMap() const
	{
		return m_schemaMap;
	}

	papuga_RequestHandler* handler() const
	{
		return m_requestHandler;
	}

private:
	void deinit()
	{
		auto fi = m_objectMap.begin(), fe = m_objectMap.end();
		for (; fi != fe; ++fi)
		{
			papuga_destroy_LuaRequestHandlerObject( fi->second);
		}
		if (m_schemaList) papuga_destroy_SchemaList( m_schemaList);
		if (m_schemaMap) papuga_destroy_SchemaMap( m_schemaMap);
		if (m_requestHandler) papuga_destroy_RequestHandler( m_requestHandler);
	}

	void loadSchemas( const std::string& schemaDir)
	{
		std::vector<std::string> files = readDirFiles( schemaDir, ".psm");
		std::vector<std::string>::const_iterator fi = files.begin(), fe = files.end();
		std::vector<size_t> startPositions;
		size_t startPosition = 0;
		std::string source;
		char errstrbuf[ 1024];
		papuga_SchemaError errbuf;

		for (; fi != fe; ++fi)
		{
			std::string fcontent = readFile( joinPath( schemaDir, *fi));
			source.append( fcontent);
			source.push_back( '\n');
			size_t flines = countLines( fcontent);
			startPosition += flines;
			startPositions.push_back( startPosition + 1);
		}
		papuga_init_SchemaError( &errbuf);

		m_schemaList = papuga_create_SchemaList( source.c_str(), &errbuf);
		m_schemaMap = papuga_create_SchemaMap( source.c_str(), &errbuf);
		if (!m_schemaList || !m_schemaMap)
		{
			if (errbuf.line)
			{
				size_t fidx = 0;
				for (; fidx < files.size() && startPositions[ fidx] < (size_t)errbuf.line; ++fidx){}
				if (fidx < files.size())
				{
					size_t fileline = fidx == 0
							? errbuf.line
							: (errbuf.line - startPositions[ fidx-1]);
					std::string schemaName( files[fidx].c_str(), files[fidx].size()-4);
					std::snprintf( errstrbuf, sizeof(errstrbuf),
							"Error in schema '%s' at line %d: %s", schemaName.c_str(),
							(int)fileline, papuga_ErrorCode_tostring( errbuf.code));
				}
				else
				{
					std::snprintf( errstrbuf, sizeof(errstrbuf),
							"Failed to load schemas from '%s': %s", schemaDir.c_str(),
							papuga_ErrorCode_tostring( errbuf.code));
				}
			}
			else
			{
				std::snprintf( errstrbuf, sizeof(errstrbuf),
						"Failed to load schemas from '%s': %s", schemaDir.c_str(),
						papuga_ErrorCode_tostring( errbuf.code));
			}
			throw std::runtime_error( errstrbuf);
		}
	}

	void loadFunctions( const std::string& objectDir)
	{
		std::vector<std::string> files = readDirFiles( objectDir, ".lua");
		std::vector<std::string>::const_iterator fi = files.begin(), fe = files.end();
		char errstrbuf[ 2048];
		papuga_ErrorBuffer errbuf;
		papuga_init_ErrorBuffer( &errbuf, errstrbuf, sizeof(errstrbuf));

		for (; fi != fe; ++fi)
		{
			std::string scriptName = baseName( *fi);
			std::string scriptSrc = readFile( joinPath( objectDir, *fi));

			papuga_LuaRequestHandlerObject* object
				= papuga_create_LuaRequestHandlerObject(
					scriptName.c_str(), scriptSrc.c_str(), &errbuf);
			if (!object)
			{
				throw std::runtime_error( papuga_ErrorBuffer_lastError( &errbuf));
			}
			m_objectMap[ scriptName] = object;
		}
	}

private:
	papuga_RequestHandler* m_requestHandler;
	papuga_SchemaList* m_schemaList;
	papuga_SchemaMap* m_schemaMap;
	std::map<std::string,papuga_LuaRequestHandlerObject*> m_objectMap;
};

static bool dumpContext( std::string& output, papuga_RequestContext* context, papuga_ErrorCode* errcode)
{
	papuga_Allocator allocator;
	int allocatormem[ 2048];
	enum {MaxContextVariables=128};
	char const* buf[ MaxContextVariables];
	papuga_init_Allocator( &allocator, allocatormem, sizeof(allocatormem));

	try
	{
		const char** varlist = papuga_RequestContext_list_variables( context, -1/*max_inheritcnt*/, buf, MaxContextVariables);
		size_t vi = 0;
		for (; varlist[vi]; ++vi)
		{
			const char* varname = varlist[vi];
			const char* varstr = nullptr;
			size_t varstrlen = 0;
			const papuga_ValueVariant* varval = papuga_RequestContext_get_variable( context, varname);

			if (papuga_ValueVariant_defined( varval))
			{
				if (papuga_ValueVariant_isatomic( varval))
				{
					varstr = papuga_ValueVariant_tostring( varval, &allocator, &varstrlen, errcode);
				}
				else
				{
					varstr = (char*)papuga_ValueVariant_tojson( 
							varval, &allocator, nullptr, 
							papuga_UTF8, false/*beautified*/, nullptr/*root*/, "item",
							&varstrlen, errcode);
				}
				if (!varstr) 
				{
					return false;
				}
				output.append( varname);
				output.append( " = ");
				output.append( varstr, varstrlen);
				output.append( "\n");
			}
		}
		return true;
	}
	catch (...)
	{
		*errcode = papuga_NoMemError;
		return false;
	}
}

static std::string runRequest(
		GlobalContext& ctx, const char* requestMethod, const char* objectName,
		const char* contentstr, size_t contentlen)
{
	papuga_ErrorCode errcode = papuga_Ok;
	papuga_ErrorBuffer errbuf;
	char errbufbuf[ 4096];
	papuga_init_ErrorBuffer( &errbuf, errbufbuf, sizeof(errbufbuf));

	papuga_RequestContext* context = papuga_create_RequestContext();
	if (!context)
	{
		throw std::runtime_error( papuga_ErrorCode_tostring( papuga_NoMemError));
	}
	papuga_LuaRequestHandler* rhnd
		= papuga_create_LuaRequestHandler(
			ctx.object( objectName), g_cemap, ctx.schemaMap(), ctx.handler(), context,
			requestMethod, contentstr, contentlen, true/*beautified*/, true/*deterministic*/, &errcode);
	if (!rhnd)
	{
		papuga_destroy_RequestContext( context);
		throw std::runtime_error( papuga_ErrorCode_tostring( errcode));
	}
	while (!papuga_run_LuaRequestHandler( rhnd, &errbuf))
	{
		if (papuga_ErrorBuffer_hasError( &errbuf))
		{
			papuga_destroy_LuaRequestHandler( rhnd);
			papuga_destroy_RequestContext( context);
			throw std::runtime_error( papuga_ErrorBuffer_lastError( &errbuf));
		}
		int nofDelegates = papuga_LuaRequestHandler_nof_DelegateRequests( rhnd);
		if (nofDelegates)
		{
			int di = 0, de = nofDelegates;
			for (; di != de; ++di)
			{
				const papuga_DelegateRequest* delegate
					= papuga_LuaRequestHandler_get_delegateRequest( rhnd, di);
				try
				{
					std::string delegateObjectName( baseName( delegate->url));
					std::string delegateRes
						= runRequest( ctx, delegate->requestmethod, delegateObjectName.c_str(),
								delegate->contentstr, delegate->contentlen);
					papuga_LuaRequestHandler_init_result( rhnd, di, delegateRes.c_str(), delegateRes.size());
				}
				catch (const std::runtime_error& err)
				{
					papuga_LuaRequestHandler_init_error( rhnd, di, papuga_DelegateRequestFailed, err.what());
				}
			}
		}
	}
	std::string rt;
	if (!dumpContext( rt, context, &errcode))
	{
		throw std::runtime_error( papuga_ErrorCode_tostring( errcode));
	}
	rt.append( "\n----\n");
	size_t resultlen = 0;
	const char* resultstr = papuga_LuaRequestHandler_get_result( rhnd, &resultlen);
	rt.append( resultstr, resultlen);
	rt.append( "\n");
	return rt;
}

static std::string normalizeOutput( const std::string& output)
{
	std::string rt;
	auto oi = output.begin(), oe = output.end();
	for (; oi != oe; ++oi)
	{
		int spaces = 0;
		int eolns = 0;
		while (oi != oe && (unsigned char)*oi <= 32)
		{
			if (*oi == '\n') {++eolns;}
			++oi;
			++spaces;
		}
		if (eolns)
		{
			rt.push_back( '\n');
		}
		else if (spaces)
		{
			rt.push_back( ' ');
		}
		if (oi == oe) break;
		rt.push_back( *oi);
	}
	while (!rt.empty() && (unsigned char)rt[ rt.size()-1] <= 32)
	{
		rt.resize( rt.size()-1);
	}
	return rt;
}

static void runTest( const std::string& scriptDir, const std::string& schemaDir, const std::string& method, const std::string& object, const std::string& inputFile, const std::string& expectFile)
{
	std::string inputSrc = readFile( inputFile);
	std::string expectSrc = readFile( expectFile);
	GlobalContext ctx( schemaDir, scriptDir);
	std::string output = runRequest( ctx, method.c_str(), object.c_str(), inputSrc.c_str(), inputSrc.size());

	if (normalizeOutput( output) != normalizeOutput( expectSrc))
	{
		if (g_verbose)
		{
			std::cerr
				<< "OUTPUT:\n" << output << "\n--\n"
				<< "EXPECT:\n" << expectSrc << "\n--\n"
				<< std::endl;
		}
		throw std::runtime_error( "Different output than expected");
	}
	else if (g_verbose)
	{
		std::cerr << "OUTPUT:\n" << output << "\n--\n" << std::endl;
	}
}

int main( int argc, const char* argv[])
{
	try
	{
		int argi = 1;
		for (; argi < argc and argv[argi][0] == '-'; ++argi)
		{
			if (std::strcmp( argv[argi], "-h") == 0 || std::strcmp( argv[argi], "--help") == 0)
			{
				printUsage();
				return 0;
			}
			else if (std::strcmp( argv[argi], "-V") == 0 || std::strcmp( argv[argi], "--verbose") == 0)
			{
				g_verbose = true;
			}
			else if (std::strcmp( argv[argi], "--") == 0)
			{
				++argi;
				break;
			}
			else
			{
				printUsage();
				throw std::runtime_error( std::string("Unknown option ") + argv[argi]);
			}
		}
		int argn = argc - argi;
		if (argn < 6)
		{
			printUsage();
			throw std::runtime_error( "Too few arguments");
		}
		else if (argn > 6)
		{
			printUsage();
			throw std::runtime_error( "Too many arguments");
		}
		std::string scriptDir = argv[ argi+0];
		std::string schemaDir  = argv[ argi+1];
		std::string method  = argv[ argi+2];
		std::string object  = argv[ argi+3];
		std::string input  = argv[ argi+4];
		std::string expect  = argv[ argi+5];

		runTest( scriptDir, schemaDir, method, object, input, expect);

		std::cerr << "OK" << std::endl;
		return 0;
	}
	catch (const std::runtime_error& err)
	{
		std::cerr << "ERROR " << err.what() << std::endl;
		return -1;
	}
	catch (const std::bad_alloc& )
	{
		std::cerr << "ERROR out of memory" << std::endl;
		return -2;
	}
	catch (...)
	{
		std::cerr << "EXCEPTION uncaught" << std::endl;
		return -3;
	}
}

