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

class RequestAutomaton
{
private:
	RequestAutomaton( const RequestAutomaton&)
		:m_atm(papuga_create_RequestAutomaton()),m_elems(),m_state(Init),m_expression(0),m_classname(0),m_methodname(0),m_selfvarname(0),m_resultvarname(0),m_itemid(-1) {}
	void operator=( const RequestAutomaton&) {}
	//... non copyable

public:
	RequestAutomaton()
		:m_atm(papuga_create_RequestAutomaton()),m_elems(),m_args(),m_state(Init),m_expression(0),m_classname(0),m_methodname(0),m_selfvarname(0),m_resultvarname(0),m_itemid(-1)
	{
		if (!m_atm) throw std::bad_alloc();
	}

	~RequestAutomaton()
	{
		papuga_destroy_RequestAutomaton( m_atm);
	}

	void check_error()
	{
		papuga_ErrorCode errcode = papuga_RequestAutomaton_last_error( m_atm);
		if (errcode != papuga_Ok) error_exception( errcode, "request automaton");
	}

	void operator()( const char* scope_expression, const char* select_expression, int itemid)
	{
		switch (m_state)
		{
			case Init: if (!papuga_RequestAutomaton_add_value( m_atm, scope_expression, select_expression, itemid)) check_error(); break;
			case Structure: throw std::runtime_error( "structure not closed in automaton");
			case Function: throw std::runtime_error( "function not closed in automaton");
		}
	}

	void operator()( const char* expression, int itemid)
	{
		switch (m_state)
		{
			case Init: m_expression = expression; m_itemid = itemid; m_state = Structure; break;
			case Structure: throw std::runtime_error( "structure not closed in automaton");
			case Function: throw std::runtime_error( "function not closed in automaton");
		}
	}

	void operator()( const char* expression, int itemid, bool inherited)
	{
		switch (m_state)
		{
			case Init: throw std::runtime_error( "structure element without structure");
			case Structure: m_elems.push_back( StructElement( expression, itemid, inherited)); break;
			case Function: throw std::runtime_error( "loose structure element in function");
		}
	}

	void operator()( const char* varname)
	{
		switch (m_state)
		{
			case Init: throw std::runtime_error( "function argument without function");
			case Structure: throw std::runtime_error( "loose function argument in structure");
			case Function: m_args.push_back( FunctionArg( varname)); break;
		}
	}

	void operator()( int itemid, bool inherited)
	{
		switch (m_state)
		{
			case Init: throw std::runtime_error( "function argument without function");
			case Structure: throw std::runtime_error( "loose function argument in structure");
			case Function: m_args.push_back( FunctionArg( itemid, inherited)); break;
		}
	}

	void operator()()
	{
		switch (m_state)
		{
			case Init:
				if (!papuga_RequestAutomaton_done( m_atm)) check_error();
				break;
			case Structure:
			{
				if (!papuga_RequestAutomaton_add_structure( m_atm, m_expression, m_itemid, m_elems.size())) check_error();
				std::vector<StructElement>::const_iterator ei = m_elems.begin(), ee = m_elems.end();
				for (int eidx=0; ei != ee; ++ei,++eidx)
				{
					if (!papuga_RequestAutomaton_set_structure_element( m_atm, eidx, ei->name, ei->itemid, ei->inherited)) check_error();
				}
				m_elems.clear();
				m_expression = 0;
				m_itemid = -1;
				break;
			}
			case Function:
			{
				if (!papuga_RequestAutomaton_add_call( m_atm, m_expression, m_classname, m_methodname, m_selfvarname, m_resultvarname, m_args.size())) check_error();
				std::vector<FunctionArg>::const_iterator ai = m_args.begin(), ae = m_args.end();
				for (int aidx=0; ai != ae; ++ai,++aidx)
				{
					if (ai->varname)
					{
						if (!papuga_RequestAutomaton_set_call_arg_var( m_atm, aidx, ai->varname)) check_error();
					}
					else
					{
						if (!papuga_RequestAutomaton_set_call_arg_item( m_atm, aidx, ai->itemid, ai->inherited)) check_error();
					}
				}
				m_args.clear();
				m_expression = 0;
				m_classname = 0;
				m_methodname = 0;
				m_selfvarname = 0;
				m_resultvarname = 0;
				break;
			}
		}
	}

private:
	enum State
	{
		Init,
		Structure,
		Function
	};
	struct StructElement
	{
		const char* name;
		int itemid;
		bool inherited;

		StructElement( const char* name_, int itemid_, bool inherited_)
			:name(name_),itemid(itemid_),inherited(inherited_){}
		StructElement( const StructElement& o)
			:name(o.name),itemid(o.itemid),inherited(o.inherited){}
	};
	struct FunctionArg
	{
		const char* varname;
		int itemid;
		bool inherited;

		FunctionArg( const char* varname_)
			:varname(varname_),itemid(-1),inherited(false){}
		FunctionArg( int itemid_, bool inherited_)
			:varname(0),itemid(itemid_),inherited(inherited_){}
		FunctionArg( const FunctionArg& o)
			:varname(o.varname),itemid(o.itemid),inherited(o.inherited){}
	};

private:
	papuga_RequestAutomaton* m_atm;
	std::vector<StructElement> m_elems;
	std::vector<FunctionArg> m_args;
	State m_state;
	const char* m_expression;
	const char* m_classname;
	const char* m_methodname;
	const char* m_selfvarname;
	const char* m_resultvarname;
	int m_itemid;
};


#if __cplusplus >= 201103L
static const test::Document g_testDocument1 = {"doc", { {"person", {{"name","Hugo"},{"id","1"}}, {{"Bla bla"}} } } };

static const Test g_tests[] = {{"simple document", &g_testDocument1},{0,0}};
#endif

#if __cplusplus >= 201103L
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
		std::cout << "XML " << papuga_StringEncoding_name( enc) << ":\n" << test.doc->toxml( enc, true) << std::endl;
		std::cout << "JSON " << papuga_StringEncoding_name( enc) << ":\n" << doc_json << std::endl;
		std::cout << "DUMP XML REQUEST:\n" << test::dumpRequest( papuga_ContentType_XML, enc, doc_xml);
		std::cout << "DUMP JSON REQUEST:\n" << test::dumpRequest( papuga_ContentType_JSON, enc, doc_json);
	}
}
#endif

int main( int argc, const char* argv[])
{
	int testno = -1;
	int testcnt = 0;
#if __cplusplus >= 201103L
	while (g_tests[testcnt].description) ++testcnt;
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

