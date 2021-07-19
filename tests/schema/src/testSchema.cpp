/*
 * Copyright (c) 2019 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
#include "papuga/schema.h"
#include "papuga/errors.hpp"
#include "papuga/allocator.h"
#include "papuga/serialization.h"
#include "papuga/serialization.hpp"
#include "papuga/requestParser.h"
#include <iostream>
#include <stdexcept>
#include <iostream>
#include <fstream>
#include <cstring>
#include <new>

bool g_verbose = false;

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
				std::snprintf( buf, sizeof(buf), "Error at line %d: %s \"%s\"", err.line, papuga_ErrorCode_tostring(err.code), err.item);
			}
			else
			{
				std::snprintf( buf, sizeof(buf), "Error at line %d: %s", err.line, papuga_ErrorCode_tostring(err.code));
			}
		}
		else
		{
			if (err.item[0])
			{
				std::snprintf( buf, sizeof(buf), "%s \"%s\"", papuga_ErrorCode_tostring(err.code), err.item);
			}
			else
			{
				std::snprintf( buf, sizeof(buf), "%s", papuga_ErrorCode_tostring(err.code));
			}
		}
		return std::string( buf);
	}
};

class SchemaMap
{
public:
	explicit SchemaMap( const std::string& src)
	{
		papuga_SchemaError err;
		m_map = papuga_create_schemamap( src.c_str(), &err);
		if (!m_map)
		{
			throw SchemaException( err);
		}
		m_list = papuga_create_schemalist( src.c_str(), &err);
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

	std::string source( const std::string& schemaName)
	{
		std::string rt;
		papuga_SchemaSource const* source = papuga_schemalist_get( m_list, schemaName.c_str());
		if (!source) throw SchemaException( papuga_AddressedItemNotFound);
		char tbuf[ 256];
		std::snprintf( tbuf, sizeof(tbuf), "SCHEMA lines=%d, name='%s':\n", source->lines, source->name);
		rt.append( tbuf);
		rt.append( source->source);
		rt.append( "\n");
		return rt;
	}

	std::string dump( const std::string& schemaSource, const std::string& schemaName)
	{
		std::string rt;
		papuga_Allocator allocator;
		papuga_init_Allocator( &allocator, 0, 0);
		papuga_SchemaError err;
		papuga_init_SchemaError( &err);

		const char* atm_source = papuga_print_schema_automaton( &allocator, schemaSource.c_str(), schemaName.c_str(), &err);
		if (!atm_source)
		{
			papuga_destroy_Allocator( &allocator);
			throw SchemaException( err);
		}
		rt.append( "AUTOMATON\n");
		rt.append( atm_source);
		rt.append( "\n");

		papuga_destroy_Allocator( &allocator);
		return rt;
	}

	std::string process( const std::string& schemaName, const std::string& src)
	{
		std::string rt;
		papuga_Schema const* schema = papuga_schemamap_get( m_map, schemaName.c_str());
		if (!schema) throw SchemaException( papuga_AddressedItemNotFound);
		papuga_Allocator allocator;
		papuga_init_Allocator( &allocator, 0, 0);
		papuga_ErrorCode errcode = papuga_Ok;
		papuga_SchemaError err;
		papuga_init_SchemaError( &err);
		papuga_Serialization dest;
		papuga_init_Serialization( &dest, &allocator);

		papuga_ContentType doctype = papuga_guess_ContentType( src.c_str(), src.size());
		papuga_StringEncoding encoding = papuga_guess_StringEncoding( src.c_str(), src.size());

		if (!papuga_schema_parse( &dest, schema, doctype, encoding, src.c_str(), src.size(),  &err))
		{
			papuga_destroy_Allocator( &allocator);
			throw SchemaException( err);
		}
		rt.append( papuga::Serialization_tostring_deterministic( dest, true/*linemode*/, -1/*maxdepth*/, errcode));
		rt.append( "\n");
		if (errcode != papuga_Ok)
		{
			papuga_destroy_Allocator( &allocator);
			throw SchemaException( errcode);
		}
		papuga_destroy_Allocator( &allocator);
		return rt;
	}

private:
	papuga_SchemaMap* m_map;
	papuga_SchemaList* m_list;
};

void printUsage()
{
	std::cerr << "testSchema [-h][-V] <schemafile> <schema> <input>\n"
		<< "\t<schemafile>  :File path of the schema description to load\n"
		<< "\t<schema>      :Name of the schema to filter input with\n"
		<< "\t<inputfile>   :File path of the input to process\n"
		<< "\t<expectfile>  :File path of the expected ouput\n"
		<< std::endl;

}

std::string normalizeOutput( const std::string& output)
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
	return rt;
}

int main( int argc, const char* argv[])
{
	int argi = 1;
	try
	{
		for (; argi < argc; ++argi)
		{
			if (0==std::strcmp( argv[argi],"-h") || 0==std::strcmp( argv[1], "--help"))
			{
				printUsage();
				exit(0);
			}
			else if (0==std::strcmp( argv[argi],"-V") || 0==std::strcmp( argv[1], "--verbose"))
			{
				g_verbose = true;
			}
			else if (0==std::strcmp( argv[argi],"--"))
			{
				++argi;
				break;
			}
			else if (argv[argi][0] == '-')
			{
				char buf[ 128];
				std::snprintf( buf, sizeof(buf), "unknown option '%s'", argv[argi]);
				throw std::runtime_error( buf);
			}
			else
			{
				break;
			}
		}
		int argn = argc-argi;
		if (argn < 4)
		{
			printUsage();
			throw std::runtime_error( "too few arguments");
		}
		if (argn > 4)
		{
			printUsage();
			throw std::runtime_error( "too many arguments");
		}
		std::string schemaFile( argv[ argi+0]);
		std::string schemaName( argv[ argi+1]);
		std::string inputFile( argv[ argi+2]);
		std::string expectFile( argv[ argi+3]);
		std::string schemaSrc = readFile( schemaFile);
		std::string inputSrc = readFile( inputFile);
		std::string expectSrc = readFile( expectFile);
		SchemaMap schemaMap( schemaSrc);
		std::string dump = schemaMap.source( schemaName) + schemaMap.dump( schemaSrc, schemaName);
		std::string output = schemaMap.process( schemaName, inputSrc);
		;
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
			std::cerr
				<< "DUMP:\n" << dump << "\n--\n"
				<< "OUTPUT:\n" << output << "\n--\n";
		}
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

