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
#include "papuga/schema.h"
#include "papuga/errors.h"
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

static void printUsage()
{
	std::cerr << "testLuaRequest <schemadir> <scriptfile> <inputfile>\n"
			<< "\t<schemadir>    :Input schema description directory\n"
			<< "\t<scriptfile>   :Lua script file of the command\n"
			<< "\t<inputfile>    :Input file to process\n"
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

std::pair<papuga_SchemaMap*,papuga_SchemaList*> loadSchemas( const std::string& schemaDir)
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
		source.append( readFile( *fi));
		source.push_back( '\n');
		startPosition += source.size();
		startPositions.push_back( startPosition);
	}
	papuga_init_SchemaError( &errbuf);

	papuga_SchemaList* list = papuga_create_schemalist( source.c_str(), &errbuf);
	papuga_SchemaMap* map = papuga_create_schemamap( source.c_str(), &errbuf);
	if (!list || !map)
	{
		if (list) papuga_destroy_schemalist( list);
		if (map) papuga_destroy_schemamap( map);

		if (errbuf.line)
		{
			size_t fidx = 0;
			for (; fidx < files.size(); ++fidx)
			{
				if (startPositions[ fidx] <= (size_t)errbuf.line) break;
			}
			if (fidx < files.size())
			{
				std::string schemaName( files[fidx].c_str(), files[fidx].size()-4);
				std::snprintf( errstrbuf, sizeof(errstrbuf),
						"Error in schema '%s': %s", schemaName.c_str(),
						papuga_ErrorCode_tostring( errbuf.code));
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
	return std::pair<papuga_SchemaMap*,papuga_SchemaList*>( map, list);
}

static void runTest( const std::string& scriptFile, const std::string& schemaDir, const std::string& inputFile)
{
	std::string functionName = baseName( scriptFile);
	std::string scriptSrc = readFile( scriptFile);
	std::string inputSrc = readFile( inputFile);
	papuga_ErrorCode errcode = papuga_Ok;
	papuga_ErrorBuffer errbuf;
	char errbufbuf[ 4096];

	std::pair<papuga_SchemaMap*,papuga_SchemaList*> schema = loadSchemas( schemaDir);
	papuga_SchemaMap* schemaMap = schema.first;
	papuga_SchemaList* schemaList = schema.second;

	papuga_init_ErrorBuffer( &errbuf, errbufbuf, sizeof(errbufbuf));

	papuga_LuaRequestHandlerFunction* fhnd
		= papuga_create_LuaRequestHandlerFunction( functionName.c_str(), scriptSrc.c_str(), &errbuf);
	if (!fhnd)
	{
		throw std::runtime_error( papuga_ErrorBuffer_lastError( &errbuf));
	}
	papuga_RequestContext* context = papuga_create_RequestContext();
	if (!context)
	{
		papuga_destroy_LuaRequestHandlerFunction( fhnd);
		throw std::runtime_error( papuga_ErrorCode_tostring( papuga_NoMemError));
	}
	papuga_LuaRequestHandler* rhnd
		= papuga_create_LuaRequestHandler(
			fhnd, g_cemap, schemaMap, context, inputSrc.c_str(), inputSrc.size(), &errcode);
	if (!rhnd)
	{
		papuga_destroy_RequestContext( context);
		papuga_destroy_LuaRequestHandlerFunction( fhnd);
		throw std::runtime_error( papuga_ErrorCode_tostring( errcode));
	}

	papuga_destroy_LuaRequestHandler( rhnd);
	papuga_destroy_RequestContext( context);
	papuga_destroy_LuaRequestHandlerFunction( fhnd);
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
		std::string schemaDir  = argv[ argi+0];
		std::string scriptFile = argv[ argi+1];
		std::string inputFile  = argv[ argi+2];

		runTest( scriptFile, schemaDir, inputFile);

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

