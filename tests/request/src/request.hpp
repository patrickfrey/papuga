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

struct RequestAutomaton_FunctionDef
{
	struct Arg
	{
		const char* varname;
		int itemid;
		bool inherited;

		Arg( const char* varname_)
			:varname(varname_),itemid(-1),inherited(false){}
		Arg( int itemid_, bool inherited_=false)
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
	RequestAutomaton_FunctionDef( const char* expression_, const char* call, const std::vector<Arg>& args_)
		:expression(expression_),classname(),methodname(),selfvarname(),resultvarname(),args(args_)
	{
		parseCall( call);
	}
	void addToAutomaton( papuga_RequestAutomaton* atm) const;

private:
	void parseCall( const char* call);
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
	RequestAutomaton_StructDef( const char* expression_, int itemid_, const std::vector<Element>& elems_)
		:expression(expression_),itemid(itemid_),elems(elems_){}

	void addToAutomaton( papuga_RequestAutomaton* atm) const;
};

struct RequestAutomaton_ValueDef
{
	std::string scope_expression;
	std::string select_expression;
	int itemid;

	RequestAutomaton_ValueDef( const RequestAutomaton_ValueDef& o)
		:scope_expression(o.scope_expression),select_expression(o.select_expression),itemid(o.itemid){}
	RequestAutomaton_ValueDef( const char* scope_expression_, const char* select_expression_, int itemid_)
		:scope_expression(scope_expression_),select_expression(select_expression_),itemid(itemid_){}

	void addToAutomaton( papuga_RequestAutomaton* atm) const;
};

struct RequestAutomaton_GroupDef
{
	std::vector<RequestAutomaton_FunctionDef> nodes;

	RequestAutomaton_GroupDef( const RequestAutomaton_GroupDef& o)
		:nodes(o.nodes){}
	RequestAutomaton_GroupDef( const std::vector<RequestAutomaton_FunctionDef>& nodes_)
		:nodes(nodes_){}

	void addToAutomaton( papuga_RequestAutomaton* atm) const;
};

#if __cplusplus >= 201103L
/// \class RequestAutomaton_Node
/// \brief Union of all RequestAutomaton_XXDef types for using in C++ initializer lists
struct RequestAutomaton_Node
{
	struct Group_ {};
	enum Type {
		Empty,
		Group,
		Function,
		Struct,
		Value
	};
	Type type;
	union
	{
		RequestAutomaton_GroupDef* groupdef;
		RequestAutomaton_FunctionDef* functiondef;
		RequestAutomaton_StructDef* structdef;
		RequestAutomaton_ValueDef* valuedef;
	} value;

	RequestAutomaton_Node();
	RequestAutomaton_Node( const Group_&, const std::initializer_list<RequestAutomaton_FunctionDef>& nodes_);
	RequestAutomaton_Node( const char* expression_, const char* call, const std::initializer_list<RequestAutomaton_FunctionDef::Arg>& args_);
	RequestAutomaton_Node( const char* expression_, int itemid_, const std::initializer_list<RequestAutomaton_StructDef::Element>& elems_);
	RequestAutomaton_Node( const char* scope_expression_, const char* select_expression_, int itemid_);

	RequestAutomaton_Node( const RequestAutomaton_Node& o);
	void addToAutomaton( papuga_RequestAutomaton* atm) const;
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
	RequestAutomaton();

#if __cplusplus >= 201103L
	RequestAutomaton( const std::initializer_list<RequestAutomaton_Node>& nodes);
#endif

	~RequestAutomaton();
	void addFunction( const char* expression, const char* call, const RequestAutomaton_FunctionDef::Arg* args);
	void addStruct( const char* expression, int itemid, const RequestAutomaton_StructDef::Element* elems);
	void addValue( const char* scope_expression, const char* select_expression, int itemid);
	void openGroup();
	void closeGroup();
	void done();

	const papuga_RequestAutomaton* impl() const	{return m_atm;}

private:
	papuga_RequestAutomaton* m_atm;
};


}}//namespace
#endif

