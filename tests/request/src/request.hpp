/*
 * Copyright (c) 2017 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
#ifndef _PAPUGA_TEST_REQUEST_HPP_INCLUDED
#define _PAPUGA_TEST_REQUEST_HPP_INCLUDED
/// \brief Some classes and functions for building test requests in a convenient way
/// \file request.hpp
#include "papuga.hpp"
#include <string>
#include <stdexcept>
#include <vector>

namespace papuga {
namespace test {

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
struct RequestAutomaton_FunctionDef
{
	struct Arg
	{
		const char* varname;
		int itemid;
		bool inherited;

		Arg( const char* varname_)
			:varname(varname_),itemid(-1),inherited(false){}
		Arg( int itemid_, bool inherited_)
			:varname(0),itemid(itemid_),inherited(inherited_){}
		Arg( const Arg& o)
			:varname(o.varname),itemid(o.itemid),inherited(o.inherited){}
	};

	std::string expression;
	std::string classname;
	std::string methodname;
	std::string selfvarname;
	std::string resultvarname;
	std::vector<Arg> args;

	RequestAutomaton_FunctionDef( const RequestAutomaton_FunctionDef& o)
		:expression(o.expression),classname(o.classname),methodname(o.methodname),selfvarname(o.selfvarname),resultvarname(o.resultvarname),args(o.args){}
	RequestAutomaton_FunctionDef( const char* expression_, const char* call, const std::initializer_list<Arg>& args_);
};

struct RequestAutomaton_StructDef
{
	struct Item
	{
		const char* expression;
		int itemid;
		bool inherited;

		Item( const char* expression_, int itemid_, bool inherited_)
			:expression(expression_),itemid(itemid_),inherited(inherited_){}
		Item( const Item& o)
			:expression(o.expression),itemid(o.itemid),inherited(o.inherited){}
	};
};
#endif


}}//namespace
#endif

