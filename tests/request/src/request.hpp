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
	struct Element
	{
		const char* name;
		int itemid;
		bool inherited;

		Element( const char* name_, int itemid_, bool inherited_)
			:name(name_),itemid(itemid_),inherited(inherited_){}
		Element( const Element& o)
			:name(o.name),itemid(o.itemid),inherited(o.inherited){}
	};

	std::string expression;
	int itemid;
	std::vector<Element> elems;

	RequestAutomaton_StructDef( const RequestAutomaton_StructDef& o)
		:expression(o.expression),itemid(o.itemid),elems(o.elems){}
	RequestAutomaton_StructDef( const char* expression_, int itemid_, const std::initializer_list<Element>& elems_);
};

struct RequestAutomaton_ValueDef
{
	std::string scope_expression;
	std::string select_expression;
	int itemid;

	RequestAutomaton_ValueDef( const RequestAutomaton_ValueDef& o)
		:scope_expression(o.scope_expression),select_expression(o.select_expression),itemid(o.itemid){}
	RequestAutomaton_ValueDef( const std::string& scope_expression_, const std::string& select_expression_, int itemid_)
		:scope_expression(scope_expression_),select_expression(select_expression_),itemid(itemid_){}
};

struct RequestAutomaton_Node
{
	enum Type {Empty,Function,Struct,Value};
	Type type;
	union
	{
		RequestAutomaton_FunctionDef* functiondef;
		RequestAutomaton_StructDef* structdef;
		RequestAutomaton_ValueDef* valuedef;
	} value;

	RequestAutomaton_Node()
		:type(Empty)
	{
		value.functiondef = 0;
	}
	RequestAutomaton_Node( const RequestAutomaton_FunctionDef& functiondef)
		:type(Function)
	{
		value.functiondef = new RequestAutomaton_FunctionDef( functiondef);
	}
	RequestAutomaton_Node( const RequestAutomaton_StructDef& structdef)
		:type(Struct)
	{
		value.structdef = new RequestAutomaton_StructDef( structdef);
	}
	RequestAutomaton_Node( const RequestAutomaton_ValueDef& valuedef)
		:type(Value)
	{
		value.valuedef = new RequestAutomaton_ValueDef( valuedef);
	}
	RequestAutomaton_Node( const RequestAutomaton_Node& o)
		:type(o.type)
	{
		switch (type)
		{
			case Empty:
				value.functiondef = 0;
				break;
			case Function:
				value.functiondef = new RequestAutomaton_FunctionDef( *o.value.functiondef);
				break;
			case Struct:
				value.structdef = new RequestAutomaton_StructDef( *o.value.structdef);
				break;
			case Value:
				value.valuedef = new RequestAutomaton_ValueDef( *o.value.valuedef);
				break;
		}
	}

	void addToAutomaton( papuga_RequestAutomaton* atm) const
	{
		switch (type)
		{
			case Empty:
				break;
			case Function:
			{
				if (!papuga_RequestAutomaton_add_call( atm, value.functiondef->expression.c_str(), value.functiondef->classname.c_str(),
									value.functiondef->methodname.c_str(), value.functiondef->selfvarname.c_str(),
									value.functiondef->resultvarname.c_str(), value.functiondef->args.size()))
				{
					papuga_ErrorCode errcode = papuga_RequestAutomaton_last_error( atm);
					if (errcode != papuga_Ok) error_exception( errcode, "request automaton add function");
				}
				std::vector<RequestAutomaton_FunctionDef::Arg>::const_iterator ai = value.functiondef->args.begin(), ae = value.functiondef->args.end();
				for (int aidx=0; ai != ae; ++ai,++aidx)
				{
					if (ai->varname)
					{
						if (!papuga_RequestAutomaton_set_call_arg_var( atm, aidx, ai->varname))
						{
							papuga_ErrorCode errcode = papuga_RequestAutomaton_last_error( atm);
							if (errcode != papuga_Ok) error_exception( errcode, "request automaton add variable call arg");
						}
					}
					else
					{
						if (!papuga_RequestAutomaton_set_call_arg_item( atm, aidx, ai->itemid, ai->inherited))
						{
							papuga_ErrorCode errcode = papuga_RequestAutomaton_last_error( atm);
							if (errcode != papuga_Ok) error_exception( errcode, "request automaton add item call arg");
						}
					}
				}
				break;
			}
			case Struct:
			{
				if (!papuga_RequestAutomaton_add_structure( atm, value.structdef->expression.c_str(), value.structdef->itemid, 
										value.structdef->elems.size()))
				{
					papuga_ErrorCode errcode = papuga_RequestAutomaton_last_error( atm);
					if (errcode != papuga_Ok) error_exception( errcode, "request automaton add structure");
				}
				std::vector<RequestAutomaton_StructDef::Element>::const_iterator
					ei = value.structdef->elems.begin(), ee = value.structdef->elems.end();
				for (int eidx=0; ei != ee; ++ei,++eidx)
				{
					if (!papuga_RequestAutomaton_set_structure_element( atm, eidx, ei->name, ei->itemid, ei->inherited))
					{
						papuga_ErrorCode errcode = papuga_RequestAutomaton_last_error( atm);
						if (errcode != papuga_Ok) error_exception( errcode, "request automaton add structure element");
					}
				}
				break;
			}
			case Value:
			{
				if (!papuga_RequestAutomaton_add_value( atm, value.valuedef->scope_expression.c_str(),
									value.valuedef->select_expression.c_str(), value.valuedef->itemid))
				{
					papuga_ErrorCode errcode = papuga_RequestAutomaton_last_error( atm);
					if (errcode != papuga_Ok) error_exception( errcode, "request automaton add value");
				}
				break;
			}
		}
	}
};
#endif

class RequestAutomaton
{
private:
	RequestAutomaton( const RequestAutomaton&)
		:m_atm(papuga_create_RequestAutomaton()){}
	void operator=( const RequestAutomaton&) {}
	//... non copyable

public:
	RequestAutomaton()
		:m_atm(papuga_create_RequestAutomaton())
	{
		if (!m_atm) throw std::bad_alloc();
	}

#if __cplusplus >= 201103L
	RequestAutomaton( const std::initializer_list<RequestAutomaton_Node>& nodes)
	{
		std::initializer_list<RequestAutomaton_Node>::const_iterator ni = nodes.begin(), ne = nodes.end();
		for (; ni != ne; ++ni)
		{
			ni->addToAutomaton( m_atm);
		}
		papuga_RequestAutomaton_done( m_atm);
	}

	RequestAutomaton( const std::initializer_list< std::initializer_list< RequestAutomaton_Node> >& nodes)
	{
		std::initializer_list< std::initializer_list<RequestAutomaton_Node> >::const_iterator lni = nodes.begin(), lne = nodes.end();
		for (; lni != lne; ++lni)
		{
			if (lni->size() > 2)
			{
				if (!papuga_RequestAutomaton_open_group( m_atm))
				{
					papuga_ErrorCode errcode = papuga_RequestAutomaton_last_error( m_atm);
					if (errcode != papuga_Ok) error_exception( errcode, "request automaton open group");
				}
			}
			std::initializer_list<RequestAutomaton_Node>::const_iterator ni = lni->begin(), ne = lni->end();
			for (; ni != ne; ++ni)
			{
				ni->addToAutomaton( m_atm);
			}
			if (lni->size() > 2)
			{
				if (!papuga_RequestAutomaton_close_group( m_atm))
				{
					papuga_ErrorCode errcode = papuga_RequestAutomaton_last_error( m_atm);
					if (errcode != papuga_Ok) error_exception( errcode, "request automaton close group");
				}
			}
		}
		papuga_RequestAutomaton_done( m_atm);
	}
#endif

	~RequestAutomaton()
	{
		papuga_destroy_RequestAutomaton( m_atm);
	}

	void check_error()
	{
		papuga_ErrorCode errcode = papuga_RequestAutomaton_last_error( m_atm);
		if (errcode != papuga_Ok) error_exception( errcode, "request automaton");
	}

private:
	papuga_RequestAutomaton* m_atm;
};



}}//namespace
#endif

