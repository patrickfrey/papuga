/*
 * Copyright (c) 2021 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
#include "papuga.hpp"
#include "luaRequestHandler.h"
#include "papuga/requestParser.h"
#include "papuga/requestHandler.h"
#include <string>
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <vector>
#include <sstream>
#include <iostream>
#include <stdexcept>
#include <algorithm>

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

static void printUsage()
{
	std::cerr << "testLuaRequest <schema> <script> <input>\n"
			<< "\t<script>     :Lua script file of the command\n"
			<< "\t<schema>     :Input schema description file\n"
			<< "\t<input>      :Input file to process\n"
	<< std::endl;
}

static std::string baseName( const std::string& filenam)
{
	char const* ei = filenam.c_str() + filenam.size();
	for (; ei != filenam.c_str() && *ei != '.'; --ei){}
	char const* fi = ei;
	for (; fi != filenam.c_str() && *fi != '/' && *fi != '\\'; --fi){}
	if (ei != fi && (*fi == '/' || *fi == '\\')) {++fi;}
	return std::string( fi, ei-fi);
}

static const papuga_lua_ClassEntryMap* g_cemap = nullptr;

static bool runTest( const std::string& scriptFile, const std::string& schemaFile, const std::string& inputFile)
{
	std::string functionName = baseName( scriptFile);
	std::string scriptSrc = readFile( scriptFile);
	std::string schemaSrc = readFile( schemaFile);
	std::string inputSrc = readFile( inputFile);
	papuga_ErrorBuffer errbuf;
	char errbufbuf[ 4096];

	papuga_init_ErrorBuffer( errbuf, errbufbuf, sizeof(errbufbuf));

	papuga_LuaRequestHandlerFunction* fhnd
		= papuga_create_LuaRequestHandlerFunction(
			functionName.c_str(), scriptSrc.c_str(), g_cemap, errbuf);
	if (!fhnd) throw std::runtime_error( papuga_ErrorBuffer_lastError( &errbuf));

	papuga_RequestContext* context = papuga_create_RequestContext();
	if (!context) throw std::runtime_error( papuga_ErrorCode_tostring( papuga_NoMemError));

	papuga_ContentType doctype = papuga_guess_ContentType( schemaSrc.c_str(), schemaSrc.size());
	papuga_StringEncoding encoding = papuga_guess_StringEncoding( schemaSrc.c_str(), schemaSrc.size());
	if (doctype == papuga_ContentType_Unknown || encoding == papuga_Binary)
	{
	}

	papuga_LuaRequestHandler* rhnd
		= papuga_create_LuaRequestHandler(
			fhnd, context,
	const papuga_Serialization* input,
	papuga_ErrorCode* errcode);

	papuga_delete_LuaRequestHandler( rhnd);
	papuga_delete_LuaRequestHandlerFunction( fhnd);
}


int main( int argc, const char* argv[])
{
	try
	{
		int argi = 1;
		for (; argi < argc; ++argi)
		{
			if (std::strcmp( argv[argi], "-h") == 0 || std::strcmp( argv[argi], "--help") == 0)
			{
				printUsage();
				return 0;
			}
			else if (std::strcmp( argv[argi], "-V") == 0 || std::strcmp( argv[argi], "--verbose") == 0)
			{
				g_verbose = true;
				return 0;
			}
			else if (std::strcmp( argv[argi], "-V") == 0 || std::strcmp( argv[argi], "--verbose") == 0)
			{
				g_verbose = true;
				return 0;
			}
			else
			{
				printUsage();
				throw std::runtime_error( std::string("Unknown option ") + argv[argi]);
			}
		}
		int argn = argc - argi;
		if (argn < 3)
		{
			printUsage();
			throw std::runtime_error( "Too few arguments");
		}
		else if (argn > 3)
		{
			printUsage();
			throw std::runtime_error( "Too many arguments");
		}
		std::string scriptFile = argv[ argi+0];
		std::string schemaFile = argv[ argi+1];
		std::string inputFile  = argv[ argi+2];

		if (!runTest( scriptFile, schemaFile, inputFile))
		{
			std::cerr << "FAILED" << std::endl;
			return 1;
		}
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

