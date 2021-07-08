/*
 * Copyright (c) 2019 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
#include "papuga/schema.h"
#include "papuga/errors.hpp"
#include <iostream>
#include <stdexcept>
#include <iostream>
#include <fstream>
#include <cstring>
#include <new>

static std::string readFile( const char* path)
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

class SchemaException
	:public std::runtime_error
{
public:
	SchemaException( papuga_ErrorCode ec_) :std::runtime_error(papuga_ErrorCode_tostring(ec_)){}
	SchemaException( const papuga_SchemaError& err_) :std::runtime_error(errorString( err_)){}

private:
	static std::string errorString( const papuga_SchemaError& err)
	{
		char buf[ 1024];
		if (err.line)
		{
			if (err.item[0])
			{
				std::snprintf( buf, sizeof(buf), "Error at line %d: %s: %s", err.line, papuga_ErrorCode_tostring(err.code), err.item);
			}
			else
			{
				std::snprintf( buf, sizeof(buf), "Error at line %d: %s", err.line, papuga_ErrorCode_tostring(err.code));
			}
		}
		else
		{
				std::snprintf( buf, sizeof(buf), "%s", papuga_ErrorCode_tostring(err.code));
		}
		return std::string( buf);
	}
};

class SchemaMap
{
public:
	SchemaMap( const char* src)
	{
		papuga_SchemaError err;
		m_map = papuga_create_schemamap( src, &err);
		if (!m_map)
		{
			throw SchemaException( err);
		}
		m_list = papuga_create_schemalist( src, &err);
		if (!m_list)
		{
			papuga_destroy_schemamap( m_map);
			throw SchemaException( err);
		}
	}
	~SchemaMap()
	{
		papuga_destroy_schemamap( m_map);
		papuga_destroy_schemalist( m_list);
	}
private:
	papuga_SchemaMap* m_map;
	papuga_SchemaList* m_list;
};

int main( int argc, const char* argv[])
{
	if (argc >= 3 || argc <= 1 || std::strcmp( argv[1], "-h") == 0 || std::strcmp( argv[1], "--help") == 0)
	{
		std::cerr << "testSchema <schemafile>" << std::endl
				<< "\t<schemafile>      :File path of the schema description to load" << std::endl;
		return 0;
	}
	try
	{
		std::string schemasrc = readFile( argv[ 1]);
		SchemaMap schemaMap( schemasrc.c_str());
		std::cerr << "OK" << std::endl;
		return 0;
	}
	catch (const std::runtime_error& err)
	{
		std::cerr << "error: " << err.what() << std::endl;
		return 1;
	}
	catch (const std::bad_alloc& err)
	{
		std::cerr << "error: " << err.what() << std::endl;
		return 2;
	}
}

