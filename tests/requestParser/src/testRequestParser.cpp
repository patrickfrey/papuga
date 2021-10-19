/*
 * Copyright (c) 2019 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
#include "papuga/requestParser.h"
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

int main( int argc, const char* argv[])
{
	if (argc <= 3 || std::strcmp( argv[1], "-h") == 0 || std::strcmp( argv[1], "--help") == 0)
	{
		std::cerr << "testRequestParser <doctype> <root> <inputfile>" << std::endl
				<< "\t<doctype>        :\"XML\" or \"JSON\"" << std::endl
				<< "\t<root>           :Expected root element name" << std::endl
				<< "\t<inputfile>      :File path of input" << std::endl;
		return 0;
	}
	try
	{
		std::string doctype = argv[ 1];
		std::string expected_root = argv[ 2];
		std::string input = readFile( argv[ 3]);
		char rootbuf[ 32];
		const char* root = 0;
		papuga_ErrorCode errcode;

		if (doctype == "XML")
		{
			root = papuga_parseRootElement_xml( rootbuf, sizeof(rootbuf), input.c_str(), input.size(), &errcode);
		}
		else if (doctype == "JSON")
		{
			root = papuga_parseRootElement_json( rootbuf, sizeof(rootbuf), input.c_str(), input.size(), &errcode);
		}
		else
		{
			throw std::runtime_error( std::string("unknown document type (first argument, \"XML\" or \"JSON\" expected): ") + doctype);
		}
		if (!root)
		{
			throw papuga::runtime_error( "failed to parse %s root element", doctype.c_str());
		}
		if (expected_root != root)
		{
			throw papuga::runtime_error( "%s root element not as expected: parsed '%s' expected '%s'", doctype.c_str(), root, expected_root.c_str());
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

