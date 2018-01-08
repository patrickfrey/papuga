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

struct Test
{
	const char* description;
	const papuga::test::Document* doc;
	const papuga::test::RequestAutomaton* atm;
};

#ifdef PAPUGA_LOWLEVEL_DEBUG
static void LOG_METHOD_CALL( const char* classname, const char* methodname, size_t argc, const papuga_ValueVariant* argv)
{
	papuga_ErrorCode errcode = papuga_Ok;
	std::cerr << "executing method " << classname << "::" << methodname << "(";
	size_t ai=0, ae=argc;
	for (; ai != ae; ++ai)
	{
		std::cerr << (ai?", ":" ");
		if (papuga_ValueVariant_isatomic( argv+ai))
		{
			std::cerr << "'" << papuga::ValueVariant_tostring( argv[ai], errcode) << "'";
		}
		else
		{
			std::cerr << "<" << papuga_Type_name( argv[ai].valuetype) << ">";
		}
	}
	std::cerr << ");" << std::endl;
}
#else
#define LOG_METHOD_CALL( classname, methodname, argc, argv)
#endif

class ObjectC1
{
public:
	ObjectC1(){}
};
class ObjectC2
{
public:
	ObjectC2(){}
};

static void* constructor_C1( papuga_ErrorBuffer* errbuf, size_t argc, const papuga_ValueVariant* argv)
{
	LOG_METHOD_CALL( "C1", "new", argc, argv);
	return new ObjectC1();
}
static void destructor_C1( void* self)
{
	LOG_METHOD_CALL( "C1", "delete", 0, 0);
	delete (ObjectC1*)self;
}
static bool method_C1M1( void* self, papuga_CallResult* retval, size_t argc, const papuga_ValueVariant* argv)
{
	LOG_METHOD_CALL( "C1", "m1", argc, argv);
	return true;
}
static bool method_C1M2( void* self, papuga_CallResult* retval, size_t argc, const papuga_ValueVariant* argv)
{
	LOG_METHOD_CALL( "C1", "m2", argc, argv);
	return true;
}
static bool method_C1M3( void* self, papuga_CallResult* retval, size_t argc, const papuga_ValueVariant* argv)
{
	LOG_METHOD_CALL( "C1", "m3", argc, argv);
	return true;
}
enum {methodtable_size_C1=3};
static papuga_ClassMethod methodtable_C1[ methodtable_size_C1] = {
	&method_C1M1,
	&method_C1M2,
	&method_C1M3
};
static const char* methodnames_C1[ methodtable_size_C1] = {
	"M1","M2","M3"
};
static void destructor_C2( void* self)
{
	LOG_METHOD_CALL( "C2", "delete", 0, 0);
	delete (ObjectC1*)self;
}
static bool method_C2M1( void* self, papuga_CallResult* retval, size_t argc, const papuga_ValueVariant* argv)
{
	LOG_METHOD_CALL( "C2", "m1", argc, argv);
	return true;
}
static bool method_C2M2( void* self, papuga_CallResult* retval, size_t argc, const papuga_ValueVariant* argv)
{
	LOG_METHOD_CALL( "C2", "m2", argc, argv);
	return true;
}
static bool method_C2M3( void* self, papuga_CallResult* retval, size_t argc, const papuga_ValueVariant* argv)
{
	LOG_METHOD_CALL( "C2", "m3", argc, argv);
	return true;
}
enum {methodtable_size_C2=3};
static papuga_ClassMethod methodtable_C2[ methodtable_size_C2] = {
	&method_C2M1,
	&method_C2M2,
	&method_C2M3
};
static const char* methodnames_C2[ methodtable_size_C2] = {
	"M1","M2","M3"
};

enum {nof_classdefs=2};
static const papuga_ClassDef g_classdefs[ nof_classdefs+1] = {
	{"C1",constructor_C1,destructor_C1,methodtable_C1,methodnames_C1,methodtable_size_C1},
	{"C2",		NULL,destructor_C2,methodtable_C2,methodnames_C2,methodtable_size_C2},
	{NULL,NULL,NULL,NULL,NULL,0}
};

struct C1
{
	static papuga_RequestMethodId constructor() {papuga_RequestMethodId rt = {1,0}; return rt;}
	static papuga_RequestMethodId m1() {papuga_RequestMethodId rt = {1,1}; return rt;}
	static papuga_RequestMethodId m2() {papuga_RequestMethodId rt = {1,2}; return rt;}
	static papuga_RequestMethodId m3() {papuga_RequestMethodId rt = {1,3}; return rt;}
};
struct C2
{
	static papuga_RequestMethodId m1() {papuga_RequestMethodId rt = {2,1}; return rt;}
	static papuga_RequestMethodId m2() {papuga_RequestMethodId rt = {2,2}; return rt;}
	static papuga_RequestMethodId m3() {papuga_RequestMethodId rt = {2,3}; return rt;}
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

static std::string executeRequestXml( const papuga::test::RequestAutomaton* atm, papuga_StringEncoding enc, const std::string& doc)
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
#ifdef PAPUGA_LOWLEVEL_DEBUG
	{
		papuga_Allocator allocator;
		papuga_init_Allocator( &allocator, 0, 0);
		const char* requestdump = papuga_Request_tostring( request.impl(), &allocator, g_classdefs, &errcode);
		if (!requestdump) throw papuga::error_exception( errcode, "dumping request");
		std::cerr << "ITEMS XML REQUEST:\n" << requestdump << std::endl;
	}
#endif
	return std::string();
}

static std::string executeRequestJson( const papuga::test::RequestAutomaton* atm, papuga_StringEncoding enc, const std::string& doc)
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
#ifdef PAPUGA_LOWLEVEL_DEBUG
	{
		papuga_Allocator allocator;
		papuga_init_Allocator( &allocator, 0, 0);
		const char* requestdump = papuga_Request_tostring( request.impl(), &allocator, g_classdefs, &errcode);
		if (!requestdump) throw papuga::error_exception( errcode, "dumping request");
		std::cerr << "ITEMS JSON REQUEST:\n" << requestdump << std::endl;
	}
#endif
	return std::string();
}

#if __cplusplus >= 201103L
enum
{
	VoidItem,
	PersonName,
	PersonContent
};
static const papuga::test::Document g_testDocument1 = {"doc", { {"person", {{"name","Hugo"},{"id","1"}}, {{"Bla bla"}} } } };
static const papuga::test::RequestAutomaton g_testRequest1 = {
							{"/doc/person", "@name", (int)PersonName},
							{"/doc/person", "()", (int)PersonContent},
							{"/doc", "var", "obj", C1::m1(), {{(int)PersonName}} } 
						};
static const papuga::test::Document g_testDocument2 = {"doc", { {"cities", {}, {{"Bern"}}}, {"cities", {}, {{"Luzern"}}}, {"cities", {}, {{"Biel"}}} } };
static const papuga::test::RequestAutomaton g_testRequest2 = {};

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
			std::cout << "DUMP XML REQUEST:\n" << papuga::test::dumpRequest( papuga_ContentType_XML, enc, content);
			std::cout << "ITEMS XML REQUEST:\n" << executeRequestXml( test.atm, enc, content);
		}{
			std::string content = test.doc->tojson( enc);
			std::cout << "JSON " << papuga_StringEncoding_name( enc) << ":\n" << content << std::endl;
			std::cout << "DUMP JSON REQUEST:\n" << papuga::test::dumpRequest( papuga_ContentType_JSON, enc, content);
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

