/*
 * Copyright (c) 2019 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
#include "papuga.hpp"
#include <iostream>
#include <stdexcept>
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <cmath>
#include <vector>
#include <string>
#include <iostream>
#include <fstream>
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

bool compareOutputExpected( const char* output, const char* expected, int& errline)
{
	char const* oi = output;
	char const* ei = expected;
	int linecnt = 1;
	while (*oi && *ei)
	{
		while (*oi && (unsigned char)*oi <= 32) ++oi;
		while (*ei && (unsigned char)*ei <= 32) {if (*ei == '\n') {++linecnt;} ++ei;}
		if (*oi != *ei)
		{
			errline = linecnt;
			return false;
		}
		if (*oi)
		{
			++oi;
			++ei;
		}
	}
	while (*oi && (unsigned char)*oi <= 32) ++oi;
	while (*ei && (unsigned char)*ei <= 32) {if (*ei == '\n') {++linecnt;} ++ei;}
	if (*oi=='\0' && *ei=='\0')
	{
		return true;
	}
	else
	{
		errline = linecnt;
		return false;
	}
}

int main( int argc, const char* argv[])
{
	if (argc <= 3 || std::strcmp( argv[1], "-h") == 0 || std::strcmp( argv[1], "--help") == 0)
	{
		std::cerr << "testSerialization <doctype> <inputfile> <expectedfile>" << std::endl
				<< "\t<doctype>        :\"XML\" or \"JSON\"" << std::endl
				<< "\t<inputfile>      :File path of input" << std::endl
				<< "\t<expected>       :File path of expected output" << std::endl;
		return 0;
	}
	try
	{
		std::string doctype = argv[ 1];
		std::string input = readFile( argv[ 2]);
		std::string expected = readFile( argv[ 3]);
		int allocator_buf[ 4096];

		papuga_Allocator allocator;
		papuga_Serialization ser;
		papuga_ErrorCode errcode = papuga_Ok;

		papuga_init_Allocator( &allocator, allocator_buf, sizeof(allocator_buf));
		papuga_init_Serialization( &ser, &allocator);
		if (doctype == "XML")
		{
			if (!papuga_Serialization_append_xml( &ser, input.c_str(), input.size(), papuga_UTF8, true/*withRoot*/, true/*ignoreEmptyContent*/, &errcode))
			{
				papuga_destroy_Allocator( &allocator);
				throw std::runtime_error( std::string("failed serializing XML input: ") + papuga_ErrorCode_tostring(errcode));
			}
		}
		else if (doctype == "JSON")
		{
			if (!papuga_Serialization_append_json( &ser, input.c_str(), input.size(), papuga_UTF8, true/*withRoot*/, &errcode))
			{
				papuga_destroy_Allocator( &allocator);
				throw std::runtime_error( std::string("failed serializing XML input: ") + papuga_ErrorCode_tostring(errcode));
			}
		}
		else
		{
			throw std::runtime_error( std::string("unknown document type (first argument, \"XML\" or \"JSON\" expected): ") + doctype);
		}
		const char* output = papuga_Serialization_tostring( &ser, &allocator, true/*linemode*/, 30/*maxdepth*/, &errcode);
		if (!output)
		{
			papuga_destroy_Allocator( &allocator);
			throw std::runtime_error( std::string("failed output of serialized XML: ") + papuga_ErrorCode_tostring(errcode));
		}
		int errline = -1;
		if (!compareOutputExpected( output, expected.c_str(), errline))
		{
			std::cerr << "OUTPUT:\n" << output << std::endl; 
			std::cerr << "EXPECTED:\n" << expected << std::endl;
			papuga_destroy_Allocator( &allocator);
			std::cerr << "\ndiffers on line " << errline << " of expected output" << std::endl;
			throw std::runtime_error( "result not as expected");
		}
		papuga_destroy_Allocator( &allocator);
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

