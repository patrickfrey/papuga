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

#define PAPUGA_LOWLEVEL_DEBUG

using namespace papuga;

struct Test
{
	const char* description;
	const test::Document* doc;
	const test::RequestAutomaton* atm;
};

class Request
{
public:
	Request( papuga_Request* ths)			:m_this(ths){}
	~Request()					{if (m_this) papuga_destroy_Request(m_this);}
	operator bool() const				{return !!m_this;}
	papuga_Request* operator->() const		{return m_this;}
	papuga_Request* impl() const			{return m_this;}
private:
	papuga_Request* m_this;
};

class RequestParser
{
public:
	RequestParser( papuga_RequestParser* ths)	:m_this(ths){}
	~RequestParser()				{if (m_this) papuga_destroy_RequestParser(m_this);}
	operator bool() const				{return !!m_this;}
	papuga_RequestParser* operator->() const	{return m_this;}
	papuga_RequestParser* impl() const		{return m_this;}
private:
	papuga_RequestParser* m_this;
};

static std::string getRequestMethodsString( papuga_Request* request)
{
	papuga_ErrorCode errcode = papuga_Ok;
	std::ostringstream out;
	papuga_RequestMethodCall* call = 0;
	while (0 != (call = papuga_Request_next_call( request)))
	{
		out << "[" << call->methodid.classid << "," << call->methodid.functionid << "] (";
		int ai = 0, ae = call->args.argc;
		for (; ai != ae; ++ai)
		{
			out << (ai ? ", ":" ");
			out << "'" << papuga::ValueVariant_tostring( call->args.argv[ai], errcode) << "'";
		}
		out << ")" << std::endl;
	}
	if (errcode == papuga_Ok)
	{
		errcode = papuga_Request_last_error( request);
	}
	if (errcode != papuga_Ok)
	{
		throw papuga::error_exception( errcode, "print request");
	}
	return out.str();
}

static std::string executeRequestXml( const test::RequestAutomaton* atm, papuga_StringEncoding enc, const std::string& doc)
{
	Request request( papuga_create_Request( atm->impl()));
	if (!request) throw std::bad_alloc();
	papuga_ErrorCode errcode = papuga_Ok;
	RequestParser parser( papuga_create_RequestParser_xml( enc, doc.c_str(), doc.size(), &errcode));
	if (!parser) throw papuga::error_exception( errcode, "creating XML request parser");
	if (!papuga_RequestParser_feed_request( parser.impl(), request.impl(), &errcode))
	{
		char buf[ 2048];
		int pos = papuga_RequestParser_get_position( parser.impl(), buf, sizeof(buf));
		throw papuga::runtime_error( "error at position %d: %s, feeding request, location: %s", pos, papuga_ErrorCode_tostring( errcode), buf);
	}
	return getRequestMethodsString( request.impl());
}

static std::string executeRequestJson( const test::RequestAutomaton* atm, papuga_StringEncoding enc, const std::string& doc)
{
	Request request( papuga_create_Request( atm->impl()));
	if (!request) throw std::bad_alloc();
	papuga_ErrorCode errcode = papuga_Ok;
	RequestParser parser( papuga_create_RequestParser_json( enc, doc.c_str(), doc.size(), &errcode));
	if (!parser) throw papuga::error_exception( errcode, "creating JSON request parser");
	if (!papuga_RequestParser_feed_request( parser.impl(), request.impl(), &errcode))
	{
		char buf[ 2048];
		int pos = papuga_RequestParser_get_position( parser.impl(), buf, sizeof(buf));
		throw papuga::runtime_error( "error at position %d: %s, feeding request, location: %s", pos, papuga_ErrorCode_tostring( errcode), buf);
	}
	return getRequestMethodsString( request.impl());
}

#if __cplusplus >= 201103L
enum
{
	VoidItem,
	PersonName,
	PersonContent
};
static const test::Document g_testDocument1 = {"doc", { {"person", {{"name","Hugo"},{"id","1"}}, {{"Bla bla"}} } } };
static const test::RequestAutomaton g_testRequest1 = {
							{"/doc/person", "@name", (int)PersonName},
							{"/doc/person", "()", (int)PersonContent},
							{"/doc", "var", "obj", {1,1}, {{(int)PersonName}} } 
						};
static const test::Document g_testDocument2 = {"doc", { {"cities", {}, {{"Bern"}}}, {"cities", {}, {{"Luzern"}}}, {"cities", {}, {{"Biel"}}} } };
static const test::RequestAutomaton g_testRequest2 = {};

static const Test g_tests[] = {
				{"simple document", &g_testDocument1, &g_testRequest1},
				{"arrays", &g_testDocument2, &g_testRequest2},
				{0,0,0}};
#endif

#if __cplusplus >= 201103L
static void executeTest( int tidx, const Test& test)
{
	std::cerr << "Executing test (" << tidx << ") '" << test.description << "'..." << std::endl;
	std::cout << test.description << ":" << std::endl;
	std::cout << "TXT:\n" << test.doc->totext() << std::endl;
	enum {NofEncodings=5};
	static papuga_StringEncoding encodings[ NofEncodings] = {papuga_UTF8, papuga_UTF16BE, papuga_UTF16LE, papuga_UTF32BE, papuga_UTF32LE};
	//[+]int ei = 0, ee = NofEncodings;
	int ei = 0, ee = 1;
	for (; ei != ee; ++ei)
	{
		papuga_StringEncoding enc = encodings[ ei];
		{
			std::string content = test.doc->toxml( enc, false);
			std::cout << "XML " << papuga_StringEncoding_name( enc) << ":\n" << test.doc->toxml( enc, true) << std::endl;
			//[+] std::cout << "DUMP XML REQUEST:\n" << test::dumpRequest( papuga_ContentType_XML, enc, content);
			std::cout << "ITEMS XML REQUEST:\n" << executeRequestXml( test.atm, enc, content);
		}{
			std::string content = test.doc->tojson( enc);
			std::cout << "JSON " << papuga_StringEncoding_name( enc) << ":\n" << content << std::endl;
			std::cout << "DUMP JSON REQUEST:\n" << test::dumpRequest( papuga_ContentType_JSON, enc, content);
			std::cout << "ITEMS JSON REQUEST:\n" << executeRequestJson( test.atm, enc, content);
		}
	}
}
#endif

int main( int argc, const char* argv[])
{
	int testno = -1;
	int testcnt = 0;
#if __cplusplus >= 201103L
	while (g_tests[testcnt].description) ++testcnt;
	std::cerr << "found " << testcnt << "tests." << std::endl;
#endif
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

