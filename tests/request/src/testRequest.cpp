/*
 * Copyright (c) 2017 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
#include "papuga.hpp"
#include "document.hpp"
#include "execRequest.h"
#include <string>
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <cmath>
#include <vector>
#include <sstream>
#include <iostream>
#include <stdexcept>
#include <algorithm>

#define PAPUGA_LOWLEVEL_DEBUG

struct Test
{
	const char* description;
	const papuga::test::Document* doc;
	const papuga::RequestAutomaton* atm;
	const RequestVariable* var;
	const char* result;
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

enum ConversionId {Ident,ToLower,ToUpper};
static bool convertValueVariant( papuga_ValueVariant* dest, const papuga_ValueVariant* src, papuga_Allocator* allocator, ConversionId convId, papuga_ErrorCode* errcode)
{
	if (papuga_ValueVariant_isatomic( src))
	{
		try
		{
			switch (convId)
			{
				case Ident:
					break;
				case ToLower:
				{
					std::string item = papuga::ValueVariant_tostring( *src, *errcode);
					if (*errcode != papuga_Ok) return false;
					std::transform( item.begin(), item.end(), item.begin(), ::tolower);
					char* copystr = papuga_Allocator_copy_string( allocator, item.c_str(), item.size());
					papuga_init_ValueVariant_string( dest, copystr, item.size());
					break;
				}
				case ToUpper:
				{
					std::string item = papuga::ValueVariant_tostring( *src, *errcode);
					if (*errcode != papuga_Ok) return false;
					std::transform( item.begin(), item.end(), item.begin(), ::tolower);
					char* copystr = papuga_Allocator_copy_string( allocator, item.c_str(), item.size());
					papuga_init_ValueVariant_string( dest, copystr, item.size());
					break;
				}
			}
		}
		catch (...)
		{
			*errcode = papuga_NoMemError;
			return false;
		}
		return true;
	}
	else if (src->valuetype == papuga_TypeSerialization)
	{
		papuga_Serialization* srcser = src->value.serialization;
		papuga_Serialization* destser = papuga_Allocator_alloc_Serialization( allocator);
		papuga_SerializationIter srcitr;
		papuga_init_SerializationIter( &srcitr, srcser);
		for (; !papuga_SerializationIter_eof( &srcitr); papuga_SerializationIter_skip( &srcitr))
		{
			const papuga_ValueVariant* srcval = papuga_SerializationIter_value( &srcitr);
			papuga_Tag tag = papuga_SerializationIter_tag( &srcitr);
			if (tag == papuga_TagValue)
			{
				papuga_ValueVariant destval;
				if (!convertValueVariant( &destval, srcval, allocator, convId, errcode)) return false;
				if (!papuga_Serialization_push( destser, tag, &destval))
				{
					*errcode = papuga_NoMemError;
					return false;
				}
			}
			else
			{
				if (!papuga_Serialization_push( destser, tag, srcval))
				{
					*errcode = papuga_NoMemError;
					return false;
				}
			}
		}
		papuga_init_ValueVariant_serialization( dest, destser);
		return true;
	}
	else
	{
		*errcode = papuga_TypeError;
		return false;
	}
}

static bool impl_method( const char* methodname, papuga_CallResult* retval, size_t argc, const papuga_ValueVariant* argv, ConversionId convId)
{
	papuga_ErrorCode errcode = papuga_Ok;
	size_t ai = 0, ae = argc;
	for (; ai != ae; ++ai)
	{
		papuga_ValueVariant res;
		if (!convertValueVariant( &res, argv+ai, &retval->allocator, convId, &errcode)) goto ERROR;
		if (!papuga_add_CallResult_value( retval, &res)) goto ERROR;
	}
	return true;
ERROR:
	if (errcode == papuga_Ok) errcode = papuga_NoMemError;
	papuga_CallResult_reportError( retval, "error in method %s: %s", methodname, papuga_ErrorCode_tostring(errcode));
	return false;
}

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
	return impl_method( "C1::m1", retval, argc, argv, ToUpper);
}
static bool method_C1M2( void* self, papuga_CallResult* retval, size_t argc, const papuga_ValueVariant* argv)
{
	LOG_METHOD_CALL( "C1", "m2", argc, argv);
	return impl_method( "C1::m2", retval, argc, argv, ToLower);
}
static bool method_C1M3( void* self, papuga_CallResult* retval, size_t argc, const papuga_ValueVariant* argv)
{
	LOG_METHOD_CALL( "C1", "m3", argc, argv);
	return impl_method( "C1::m2", retval, argc, argv, Ident);
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
	return impl_method( "C2::m2", retval, argc, argv, ToUpper);
}
static bool method_C2M2( void* self, papuga_CallResult* retval, size_t argc, const papuga_ValueVariant* argv)
{
	LOG_METHOD_CALL( "C2", "m2", argc, argv);
	return impl_method( "C2::m2", retval, argc, argv, ToLower);
}
static bool method_C2M3( void* self, papuga_CallResult* retval, size_t argc, const papuga_ValueVariant* argv)
{
	LOG_METHOD_CALL( "C2", "m3", argc, argv);
	return impl_method( "C2::m3", retval, argc, argv, Ident);
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

enum {nof_structdefs=0};
papuga_StructInterfaceDescription g_structdefs[ nof_structdefs+1] = {
	{NULL/*name*/,NULL/*doc*/,NULL/*members*/}
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


#if __cplusplus >= 201103L
enum
{
	VoidItem,
	PersonName,
	PersonContent
};
static const papuga::test::Document g_testDocument1 = {
	"doc", {
		{"person", {{"name","Hugo"},{"id","1"}}, {{"Bla bla"}} }
		}
	};
static const papuga::RequestAutomaton g_testRequest1 = {
	g_classdefs, g_structdefs, "person",
	{
		{"/doc/person", "@name", (int)PersonName},
		{"/doc/person", "()", (int)PersonContent},
		{"/doc", "obj", 0, C1::constructor(), {} },
		{"/doc", "var", "obj", C1::m1(), {{(int)PersonName}} }
	}};
static const papuga::test::Document g_testDocument2 = {
	"doc", {
		{"cities", {}, {{"Bern"}}},
		{"cities", {}, {{"Luzern"}}},
		{"cities", {}, {{"Biel"}}}
		}
	};

static const papuga::RequestAutomaton g_testRequest2 = {
	g_classdefs, g_structdefs,
	{}};

static const Test g_tests[] = {
	{"simple document", &g_testDocument1, &g_testRequest1, NULL/*variables*/, NULL/*result*/},
	{"arrays", &g_testDocument2, &g_testRequest2, NULL/*variables*/, NULL/*result*/},
	{0,0,0}};

struct TestSet
{
	papuga_StringEncoding encoding;
	papuga_ContentType doctype;
};
static const TestSet testsets[] = {
	{papuga_UTF8,papuga_ContentType_XML},
	{papuga_UTF8,papuga_ContentType_JSON},
	{papuga_UTF16BE,papuga_ContentType_XML},
	{papuga_UTF16BE,papuga_ContentType_JSON},
	{papuga_UTF16LE,papuga_ContentType_XML},
	{papuga_UTF16LE,papuga_ContentType_JSON},
	{papuga_UTF32BE,papuga_ContentType_XML},
	{papuga_UTF32BE,papuga_ContentType_JSON},
	{papuga_UTF32LE,papuga_ContentType_XML},
	{papuga_UTF32LE,papuga_ContentType_JSON},
	{papuga_UTF8,papuga_ContentType_Unknown}
};

#ifdef PAPUGA_LOWLEVEL_DEBUG
#define TESTLOG( TITLE, CONTENT)\
	std::cerr << (TITLE) << ":\n" << (CONTENT) << std::endl;
#else
#define TESTLOG( TITLE, CONTENT)
#endif


static void executeTest( int tidx, const Test& test)
{
	std::cerr << "Executing test (" << tidx << ") '" << test.description << "'..." << std::endl;
	TESTLOG( "TXT", test.doc->totext())
	int ei = 0, ee = 1;
	for (; ei != ee && testsets[ei].doctype != papuga_ContentType_Unknown; ++ei)
	{
		papuga_ErrorCode errcode = papuga_Ok;
		char* resstr = 0;
		std::size_t reslen = 0;
		papuga_StringEncoding enc = testsets[ ei].encoding;
		papuga_ContentType doctype = testsets[ ei].doctype;

		std::string content;
		switch (doctype)
		{
			case papuga_ContentType_XML:
				content = test.doc->toxml( enc, false);
				TESTLOG( std::string("DOC XML") + papuga_StringEncoding_name( enc), test.doc->toxml( enc, true))
				break;
			case papuga_ContentType_JSON:
				content = test.doc->tojson( enc);
				TESTLOG( std::string("DOC JSON") + papuga_StringEncoding_name( enc), content)
				break;
			case papuga_ContentType_Unknown:
				break;
		}
		TESTLOG( "DUMP", papuga::test::dumpRequest( doctype, enc, content))

		if (!papuga_execute_request( test.atm->impl(), doctype, enc, content.c_str(), content.size(), test.var, &errcode, &resstr, &reslen))
		{
			TESTLOG( "ERROR", std::string( resstr, reslen))
			throw papuga::error_exception( errcode, "executing test request");
		}
		else
		{
			TESTLOG( "RESULT", std::string( resstr, reslen))
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
		else for (int testidx = 1; testidx <= testcnt; ++testidx)
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

