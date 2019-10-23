/*
 * Copyright (c) 2017 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
#include "papuga.hpp"
#include "document.hpp"
#include "execRequest.hpp"
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

std::string g_call_dump;

static void LOG_METHOD_CALL( const char* classname, const char* methodname, size_t argc, const papuga_ValueVariant* argv)
{
	papuga_ErrorCode errcode = papuga_Ok;
	std::ostringstream out;
	out << "executing method " << classname << "::" << methodname << "(";
	size_t ai=0, ae=argc;
	for (; ai != ae; ++ai)
	{
		out << (ai?", ":" ");
		if (papuga_ValueVariant_isatomic( argv+ai))
		{
			out << "'" << papuga::ValueVariant_tostring( argv[ai], errcode) << "'";
		}
		else
		{
			out << "<" << papuga_Type_name( argv[ai].valuetype) << ">";
		}
	}
	out << ");\n";
	g_call_dump.append( out.str());
}

#ifdef PAPUGA_LOWLEVEL_DEBUG
static void LOG_TEST_CONTENT( const std::string& title, const std::string& content)
{
	std::cerr << title << ":\n" << content << std::endl;
}
#else
#define LOG_TEST_CONTENT( TITLE, CONTENT)
#endif

#if __cplusplus >= 201103L
static std::string mapCallList( const char** calllist)
{
	std::string rt;
	char const** cl = calllist;
	for (; *cl != NULL; ++cl)
	{
		rt.append( *cl);
		rt.push_back('\n');
	}
	return rt;
}

static std::string mapDocument( const papuga::test::Document& doc, papuga_StringEncoding encoding, papuga_ContentType doctype, bool with_indent)
{
	std::string rt;
	switch (doctype)
	{
		case papuga_ContentType_XML:
			rt = doc.toxml( encoding, with_indent);
			LOG_TEST_CONTENT( "DOC", doc.toxml( encoding, true));
			break;
		case papuga_ContentType_JSON:
			rt = doc.tojson( encoding);
			LOG_TEST_CONTENT( "DOC", rt);
			break;
		case papuga_ContentType_Unknown:
			break;
	}
	return rt;
}
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
	if (!papuga_ValueVariant_defined( src))
	{
		papuga_init_ValueVariant( dest);
		return true;
	}
	else if (papuga_ValueVariant_isatomic( src))
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
					std::transform( item.begin(), item.end(), item.begin(), ::toupper);
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
		if (!convertValueVariant( &res, argv+ai, retval->allocator, convId, &errcode)) goto ERROR;
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

enum
{
	VoidItem,
	PersonName,
	PersonContent,
	CityName,
	CityList,
	TreeNode,
	TreeNodeValue,
	TreeNodeLeft,
	TreeNodeRight
};
static const char* itemName( int itemid)
{
	static const char* ar[] = {"VoidItem","PersonName","PersonContent","CityName","CityList","TreeNode","TreeNodeValue","TreeNodeLeft","TreeNodeRight",0};
	return ar[ itemid];
}

#if __cplusplus >= 201103L
struct TestData
{
	const char* description;
	papuga::test::Document* doc;
	papuga::RequestAutomaton* atm;
	const RequestVariable* var;
	const char** calls;
	papuga::test::Document* expected;

	TestData() :description(0),doc(0),atm(0),var(0),calls(0),expected(0){}
	~TestData()
	{
		if (doc) delete doc;
		if (atm) delete atm;
		if (expected) delete expected;
	}
};

typedef TestData* (*createTestDataFunction)();

static TestData* createTestData_1()
{
	TestData* data = new TestData();
	data->description = "single item with content and attribute, select attribute";
	data->doc = new papuga::test::Document(
		"doc", {
			{"person", {{"name","Hugo"},{"id","1"}}, {{"Bla bla"}} }
			}
		);
	data->atm = new papuga::RequestAutomaton(
		g_classdefs, g_structdefs, itemName, true/*strict*/, false/*exclusive*/,
		{/*env*/},
		{/*result*/
			{"person", {{"/doc", "var", "var", '!'}}}
		},
		{/*inherit*/},
		{/*input*/
			{"/doc/person", "@name", (int)PersonName, papuga_TypeString, "John Doe"},
			{"/doc/person", "()", (int)PersonContent, papuga_TypeVoid, NULL},
			{"/doc", "obj", 0, C1::constructor(), {} },
			{"/doc", "var", "obj", C1::m1(), {{(int)PersonName}} }
		} );
	static const char* expected_calls[] = {
		"executing method C1::new();",
		"executing method C1::m1( 'Hugo');",
		"executing method C1::delete();",
		"EV open tag -1 'doc'",
		"EV open tag -1 'person'",
		"EV attribute name -1 'name'",
		"EV attribute value -1 'Hugo'",
		"EV instantiate 1 'Hugo'",
		"EV attribute name -1 'id'",
		"EV attribute value -1 '1'",
		"EV content value -1 'Bla bla'",
		"EV close tag -1 ''",
		"EV collect 1 'Hugo'",
		"EV close tag -1 ''",
		"EV close tag -1 ''",
		"C1 0  obj <HostObject>",
		"EV resolved required 1 'Hugo'",
		"C1 M1 1 Hugo var HUGO",
		0};
	data->calls = expected_calls;
	data->expected = new papuga::test::Document(
		"person", {
			{"var", {{"HUGO"}} }
			}
		);
	return data;
}

static TestData* createTestData_2()
{
	TestData* data = new TestData();
	data->description = "array, empty request";
	data->doc = new papuga::test::Document(
		"doc", {
			{"city", {{"Bern"}}},
			{"city", {{"Luzern"}}},
			{"city", {{"Biel"}}}
			}
		);
	data->atm = new papuga::RequestAutomaton(
		g_classdefs, g_structdefs, itemName, true/*strict*/, false/*exclusive*/,
		{/*env*/},
		{/*result*/},
		{/*inherit*/},
		{
			{"/doc", "obj", 0, C1::constructor(), {} }
		}
		);
	static const char* expected_calls[] = {
		"executing method C1::new();",
		"executing method C1::delete();",
		"EV open tag -1 'doc'",
		"EV open tag -1 'city'",
		"EV content value -1 'Bern'",
		"EV close tag -1 ''",
		"EV open tag -1 'city'",
		"EV content value -1 'Luzern'",
		"EV close tag -1 ''",
		"EV open tag -1 'city'",
		"EV content value -1 'Biel'",
		"EV close tag -1 ''",
		"EV close tag -1 ''",
		"EV close tag -1 ''",
		"C1 0  obj <HostObject>",
		0};
	data->calls = expected_calls;
	data->expected = new papuga::test::Document();
	return data;
}

static TestData* createTestData_3()
{
	TestData* data = new TestData();
	data->description = "array, foreach item";
	data->doc = new papuga::test::Document(
		"doc", {
			{"city", {{"Bern"}}},
			{"city", {{"Luzern"}}},
			{"city", {{"Biel"}}}
			}
		);
	data->atm = new papuga::RequestAutomaton(
		g_classdefs, g_structdefs, itemName, true/*strict*/, false/*exclusive*/,
		{/*env*/},
		{/*result*/
			{"list", { {"/doc", "lo", "lo", '+'},{"/doc", "hi", "hi", '*'} }}
		},
		{/*inherit*/},
		{
			{"/doc/city", "()", (int)CityName, papuga_TypeString, "Berlin"},
			{"/doc", "obj", 0, C1::constructor(), {} },
			{"/doc/city", "lo", "obj", C1::m2(), {{(int)CityName}} },
			{"/doc/city", "hi", "obj", C1::m1(), {{(int)CityName}} }
		});
	static const char* expected_calls[] = {
		"executing method C1::new();",
		"executing method C1::m2( 'Bern');",
		"executing method C1::m2( 'Luzern');",
		"executing method C1::m2( 'Biel');",
		"executing method C1::m1( 'Bern');",
		"executing method C1::m1( 'Luzern');",
		"executing method C1::m1( 'Biel');",
		"executing method C1::delete();",
		"EV open tag -1 'doc'",
		"EV open tag -1 'city'",
		"EV content value -1 'Bern'",
		"EV instantiate 3 'Bern'",
		"EV close tag -1 ''",
		"EV collect 3 'Bern'",
		"EV open tag -1 'city'",
		"EV content value -1 'Luzern'",
		"EV instantiate 3 'Luzern'",
		"EV close tag -1 ''",
		"EV collect 3 'Luzern'",
		"EV open tag -1 'city'",
		"EV content value -1 'Biel'",
		"EV instantiate 3 'Biel'",
		"EV close tag -1 ''",
		"EV collect 3 'Biel'",
		"EV close tag -1 ''",
		"EV close tag -1 ''",
		"C1 0  obj <HostObject>",
		"EV resolved required 3 'Bern'",
		"C1 M2 1 Bern lo bern",
		"EV resolved required 3 'Luzern'",
		"C1 M2 1 Luzern lo luzern",
		"EV resolved required 3 'Biel'",
		"C1 M2 1 Biel lo biel",
		"EV resolved required 3 'Bern'",
		"C1 M1 1 Bern hi BERN",
		"EV resolved required 3 'Luzern'",
		"C1 M1 1 Luzern hi LUZERN",
		"EV resolved required 3 'Biel'",
		"C1 M1 1 Biel hi BIEL",
		0};
	data->calls = expected_calls;
	data->expected = new papuga::test::Document(
		"list", {
			{"lo", {{"bern"}} },
			{"lo", {{"luzern"}} },
			{"lo", {{"biel"}} },
			{"hi", {{"BERN"}} },
			{"hi", {{"LUZERN"}} },
			{"hi", {{"BIEL"}} }
			}
		);
	return data;
}

static TestData* createTestData_4()
{
	TestData* data = new TestData();
	data->description = "array, foreach item group";
	data->doc = new papuga::test::Document(
		"doc", {
			{"city", {{"Bern"}}},
			{"city", {{"Luzern"}}},
			{"city", {{"Biel"}}}
			}
		);
	data->atm = new papuga::RequestAutomaton(
		g_classdefs, g_structdefs, itemName, true/*strict*/, false/*exclusive*/,
		{/*env*/},
		{/*result*/
			{"list", { {"/doc", "lo", "lo", '*'},{"/doc", "hi", "hi", '+'} }}
		},
		{/*inherit*/},
		{
			{"/doc/{city,town}", "()", (int)CityName, papuga_TypeString, "Berlin"},
			{"/doc", "obj", 0, C1::constructor(), {} },
			{{
				{"/doc/{city,town}", "lo", "obj", C1::m2(), {{(int)CityName}} },
				{"/doc/{city,town}", "hi", "obj", C1::m1(), {{(int)CityName}} }
			}}
		});
	static const char* expected_calls[] = {
		"executing method C1::new();",
		"executing method C1::m2( 'Bern');",
		"executing method C1::m1( 'Bern');",
		"executing method C1::m2( 'Luzern');",
		"executing method C1::m1( 'Luzern');",
		"executing method C1::m2( 'Biel');",
		"executing method C1::m1( 'Biel');",
		"executing method C1::delete();",
		"EV open tag -1 'doc'",
		"EV open tag -1 'city'",
		"EV content value -1 'Bern'",
		"EV instantiate 3 'Bern'",
		"EV close tag -1 ''",
		"EV collect 3 'Bern'",
		"EV open tag -1 'city'",
		"EV content value -1 'Luzern'",
		"EV instantiate 3 'Luzern'",
		"EV close tag -1 ''",
		"EV collect 3 'Luzern'",
		"EV open tag -1 'city'",
		"EV content value -1 'Biel'",
		"EV instantiate 3 'Biel'",
		"EV close tag -1 ''",
		"EV collect 3 'Biel'",
		"EV close tag -1 ''",
		"EV close tag -1 ''",
		"C1 0  obj <HostObject>",
		"EV resolved required 3 'Bern'",
		"C1 M2 1 Bern lo bern",
		"EV resolved required 3 'Bern'",
		"C1 M1 1 Bern hi BERN",
		"EV resolved required 3 'Luzern'",
		"C1 M2 1 Luzern lo luzern",
		"EV resolved required 3 'Luzern'",
		"C1 M1 1 Luzern hi LUZERN",
		"EV resolved required 3 'Biel'",
		"C1 M2 1 Biel lo biel",
		"EV resolved required 3 'Biel'",
		"C1 M1 1 Biel hi BIEL",
		0
	};
	data->calls = expected_calls;
	data->expected = new papuga::test::Document(
		"list", {
			{"lo", {{"bern"}} },
			{"lo", {{"luzern"}} },
			{"lo", {{"biel"}} },
			{"hi", {{"BERN"}} },
			{"hi", {{"LUZERN"}} },
			{"hi", {{"BIEL"}} }
			}
		);
	return data;
}

static TestData* createTestData_5()
{
	TestData* data = new TestData();
	data->description = "array, foreach struct group";
	data->doc = new papuga::test::Document(
		"doc", {
			{"city", {{"Bern"}}},
			{"city", {{"Luzern"}}},
			{"city", {{"Biel"}}}
			}
		);
	data->atm = new papuga::RequestAutomaton(
		g_classdefs, g_structdefs, itemName, true/*strict*/, false/*exclusive*/,
		{/*env*/},
		{/*result*/
			{"list", { {"/doc", "lo", "lo", '!'},{"/doc", "hi", "hi", '!'} }}
		},
		{/*inherit*/},
		{
			{"/doc/city", "()", CityName, papuga_TypeString, "Berlin"},
			{"/doc", "obj", 0, C1::constructor(), {} },
			{{
				{"/doc", "lo", "obj", C1::m2(), {{CityName, '*'}} },
				{"/doc", "hi", "obj", C1::m1(), {{CityName, '*'}} }
			}}
		});
	static const char* expected_calls[] = {
		"executing method C1::new();",
		"executing method C1::m2( <Serialization>);",
		"executing method C1::m1( <Serialization>);",
		"executing method C1::delete();",
		"EV open tag -1 'doc'",
		"EV open tag -1 'city'",
		"EV content value -1 'Bern'",
		"EV instantiate 3 'Bern'",
		"EV close tag -1 ''",
		"EV collect 3 'Bern'",
		"EV open tag -1 'city'",
		"EV content value -1 'Luzern'",
		"EV instantiate 3 'Luzern'",
		"EV close tag -1 ''",
		"EV collect 3 'Luzern'",
		"EV open tag -1 'city'",
		"EV content value -1 'Biel'",
		"EV instantiate 3 'Biel'",
		"EV close tag -1 ''",
		"EV collect 3 'Biel'",
		"EV close tag -1 ''",
		"EV close tag -1 ''",
		"C1 0  obj <HostObject>",
		"EV resolved first of array 3 'Bern'",
		"C1 M2 1 <Serialization> lo <Serialization>",
		"EV resolved first of array 3 'Bern'",
		"C1 M1 1 <Serialization> hi <Serialization>",
		0
	};
	data->calls = expected_calls;
	data->expected = new papuga::test::Document(
		"list", {
			{"lo", {{"bern"}} },
			{"lo", {{"luzern"}} },
			{"lo", {{"biel"}} },
			{"hi", {{"BERN"}} },
			{"hi", {{"LUZERN"}} },
			{"hi", {{"BIEL"}} }
		});
	return data;
}

static TestData* createTestData_6()
{
	TestData* data = new TestData();
	data->description = "binary tree";
	data->doc = new papuga::test::Document(
		"tree", {
				{"left", {
						{"left", {
								{"value", {{"L.L"}}}
							}
						},
						{"right", {
								{"value", {{"L.R"}}}
							}
						}
					}
				},
				{"right", {
						{"value", {{"R"}}}
					}
				},
				{"value", {{"C"}}}
			}
		);
	data->atm = new papuga::RequestAutomaton(
		g_classdefs, g_structdefs, itemName, true/*strict*/, false/*exclusive*/,
		{/*env*/},
		{/*result*/
			{"result", { {"/tree", "lo", "lo", '!'},{"/tree", "hi", "hi", '!'} }}
		},
		{/*inherit*/},
		{
			{"/tree/value", "()", TreeNodeValue, papuga_TypeString, "T"},
			{"//left/value", "()", TreeNodeValue, papuga_TypeString, "L"},
			{"//right/value", "()", TreeNodeValue, papuga_TypeString, "R"},

			{"/tree", TreeNode, {{"value", TreeNodeValue, '?'}, {"left", TreeNodeLeft, '?'}, {"right", TreeNodeRight, '?'}} },
			{"//left", TreeNodeLeft, {{"value", TreeNodeValue, '?'}, {"left", TreeNodeLeft, '?'}, {"right", TreeNodeRight, '?'}} },
			{"//right", TreeNodeRight, {{"value", TreeNodeValue, '?'}, {"left", TreeNodeLeft, '?'}, {"right", TreeNodeRight, '?'}} },

			{"/tree", "obj", 0, C1::constructor(), {} },
			{{
				{"/tree", "lo", "obj", C1::m2(), {{TreeNode, '?'}} },
				{"/tree", "hi", "obj", C1::m1(), {{TreeNode, '?'}} }
			}}
		});
	static const char* expected_calls[] = {
		"executing method C1::new();",
		"executing method C1::m2( <Serialization>);",
		"executing method C1::m1( <Serialization>);",
		"executing method C1::delete();",
		"EV open tag -1 'tree'",
		"EV open tag -1 'left'",
		"EV open tag -1 'left'",
		"EV open tag -1 'value'",
		"EV content value -1 'L.L'",
		"EV instantiate 6 'L.L'",
		"EV close tag -1 ''",
		"EV collect 6 'L.L'",
		"EV close tag -1 ''",
		"EV struct 7 ''",
		"EV open tag -1 'right'",
		"EV open tag -1 'value'",
		"EV content value -1 'L.R'",
		"EV instantiate 6 'L.R'",
		"EV close tag -1 ''",
		"EV collect 6 'L.R'",
		"EV close tag -1 ''",
		"EV struct 8 ''",
		"EV close tag -1 ''",
		"EV struct 7 ''",
		"EV open tag -1 'right'",
		"EV open tag -1 'value'",
		"EV content value -1 'R'",
		"EV instantiate 6 'R'",
		"EV close tag -1 ''",
		"EV collect 6 'R'",
		"EV close tag -1 ''",
		"EV struct 8 ''",
		"EV open tag -1 'value'",
		"EV content value -1 'C'",
		"EV instantiate 6 'C'",
		"EV close tag -1 ''",
		"EV collect 6 'C'",
		"EV close tag -1 ''",
		"EV struct 5 ''",
		"EV close tag -1 ''",
		"C1 0  obj <HostObject>",
		"EV resolved required 5 '#4'",
		"EV resolved required 6 'C'",
		"EV resolved required 7 '#2'",
		"EV resolved required 7 '#0'",
		"EV resolved required 6 'L.L'",
		"EV resolved required 8 '#1'",
		"EV resolved required 6 'L.R'",
		"EV resolved required 8 '#3'",
		"EV resolved required 6 'R'",
		"C1 M2 1 <Serialization> lo <Serialization>",
		"EV resolved required 5 '#4'",
		"EV resolved required 6 'C'",
		"EV resolved required 7 '#2'",
		"EV resolved required 7 '#0'",
		"EV resolved required 6 'L.L'",
		"EV resolved required 8 '#1'",
		"EV resolved required 6 'L.R'",
		"EV resolved required 8 '#3'",
		"EV resolved required 6 'R'",
		"C1 M1 1 <Serialization> hi <Serialization>",
		0
	};
	data->calls = expected_calls;
	data->expected = new papuga::test::Document(
		"result", {
			{"lo", {
				{"value", {{"c"}}},
				{"left", {
						{"left", {
								{"value", {{"l.l"}}}
							}
						},
						{"right", {
								{"value", {{"l.r"}}}
							}
						}
					}
				},
				{"right", {
						{"value", {{"r"}}}
					}
				}
			}
			},
			{"hi", {
				{"value", {{"C"}}},
				{"left", {
						{"left", {
								{"value", {{"L.L"}}}
							}
						},
						{"right", {
								{"value", {{"L.R"}}}
							}
						}
					}
				},
				{"right", {
						{"value", {{"R"}}}
					}
				}
			}
			}
		});
	return data;
}

static createTestDataFunction g_tests[] = {
	&createTestData_1,
	&createTestData_2,
	&createTestData_3,
	&createTestData_4,
	&createTestData_5,
	&createTestData_6,
	NULL};

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

static void executeTest( int tidx, const TestData& test)
{
	std::cerr << "Executing test (" << tidx << ") '" << test.description << "'..." << std::endl;
	LOG_TEST_CONTENT( "TXT", test.doc->totext());
	int ei = 0, ee = -1;
	for (; ei != ee && testsets[ei].doctype != papuga_ContentType_Unknown; ++ei)
	{
		g_call_dump.clear();
		std::string resout;
		std::string logout;
		papuga_StringEncoding enc = testsets[ ei].encoding;
		papuga_ContentType doctype = testsets[ ei].doctype;

		std::cerr << ei << ". doctype=" << papuga_ContentType_name( doctype) << ", encoding=" << papuga_StringEncoding_name( enc) << std::endl;

		std::string content = mapDocument( *test.doc, enc, doctype, false/*no indent*/);
		LOG_TEST_CONTENT( "DUMP", papuga::test::dumpRequest( doctype, enc, content));

		if (!papuga_execute_request( test.atm->impl(), doctype, enc, content, test.var, resout, logout))
		{
			LOG_TEST_CONTENT( "ERROR", resout);
			std::string errmsg( std::string("executing test request: ") + resout);
			throw std::runtime_error( errmsg);
		}
		else
		{
			std::string expected = mapCallList( test.calls) + "---\n" + mapDocument( *test.expected, enc, doctype, true/*with indent*/);
			std::string result = g_call_dump + logout + "---\n" + resout;
			if (expected != result)
			{
				std::cout << "Result [" << result.size() << "]:\n" << result << std::endl;
				std::cout << "Expected [" << expected.size() << "]:\n" << expected << std::endl;
				throw std::runtime_error( "test output differs");
			}
			else
			{
				LOG_TEST_CONTENT( "RESULT", result);
			}
		}
	}
}
#endif


int main( int argc, const char* argv[])
{
	int testno = -1;
	int testcnt = 0;
#if __cplusplus >= 201103L
	while (g_tests[testcnt]) ++testcnt;
	std::cerr << "found " << testcnt << " tests." << std::endl;
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
			createTestDataFunction createTestData = g_tests[ testno-1];
			TestData* test = createTestData();
			executeTest( testno, *test);
			delete test;
		}
		else for (int testidx = 1; testidx <= testcnt; ++testidx)
		{
			createTestDataFunction createTestData = g_tests[ testidx-1];
			TestData* test = createTestData();
			executeTest( testidx, *test);
			delete test;
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

