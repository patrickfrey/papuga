/*
 * Copyright (c) 2017 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
#include "papuga.hpp"
#include "document.hpp"
#include <string>
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <cmath>
#include <vector>
#include <sstream>
#include <iostream>
#include <stdexcept>

#undef PAPUGA_LOWLEVEL_DEBUG

using namespace papuga;

struct Test
{
	const char* description;
	const test::Document* doc;
};

#if __cplusplus >= 201103L
static const test::Document g_testDocument1 = {"doc", { {"person", {{"name","Hugo"},{"id","1"}}, {{"Bla bla"}} } } };
static const Test g_tests[] = {{"simple document", &g_testDocument1},{0,0}};
#endif


static void executeTest( int tidx, const Test& test)
{
	std::cerr << "Executing test (" << tidx << ") '" << test.description << "'..." << std::endl;
	std::cout << test.description << ":" << std::endl;
	std::cout << "TXT:\n" << test.doc->totext() << std::endl;
	enum {NofEncodings=5};
	static papuga_StringEncoding encodings[ NofEncodings] = {papuga_UTF8, papuga_UTF16BE, papuga_UTF16LE, papuga_UTF32BE, papuga_UTF32LE};
	int ei = 0, ee = NofEncodings;
	for (; ei != ee; ++ei)
	{
		papuga_StringEncoding enc = encodings[ ei];
		std::string doc_xml = test.doc->toxml( enc, false);
		std::string doc_json = test.doc->tojson( enc);
		std::cout << "XML " << test::encodingName( enc) << ":\n" << test.doc->toxml( enc, true) << std::endl;
		std::cout << "JSON " << test::encodingName( enc) << ":\n" << doc_json << std::endl;
		std::cout << "DUMP XML REQUEST:\n" << test::dumpRequest( papuga_ContentType_XML, enc, doc_xml);
		std::cout << "DUMP JSON REQUEST:\n" << test::dumpRequest( papuga_ContentType_JSON, enc, doc_json);
	}
}

int main( int argc, const char* argv[])
{
	int testno = -1;
	int testcnt = 0;
	while (g_tests[testcnt].description) ++testcnt;

	if (argc > 1)
	{
		if (std::strcmp( argv[1], "-h") == 0 || std::strcmp( argv[1], "--help") == 0)
		{
			std::cerr << "testRequest <testno>\n"
					<< "\t<testno>     :Index of test to execute (default all)" << std::endl;
			return 0;
		}
		else
		{
			testno = atoi( argv[1]);
			if (testno <= 0 || testno > testcnt)
			{
				std::cerr << "test program argument must be a positive number between 1 and " << testcnt << std::endl;
				return -1;
			}
		}
	}
	try
	{
#if __cplusplus >= 201103L
		if (testno >= 1)
		{
			executeTest( testno, g_tests[ testno-1]);
		}
		for (int testidx = 1; testidx <= testcnt; ++testidx)
		{
			executeTest( testidx, g_tests[ testidx-1]);
		}
#else
		std::cerr << "This test needs C++11 as it uses std initializer_list" << std::endl;
#endif
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

