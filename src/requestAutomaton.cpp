/*
 * Copyright (c) 2017 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/// \brief Structure to define an automaton mapping a request to function calls in C++ in a convenient way
/// \file requestAutomaton.cpp
#include "papuga/requestAutomaton.hpp"
#include "papuga/classdef.h"
#include "papuga/request.h"
#include "papuga/errors.hpp"
#include "papuga/errors.h"
#include "private/internationalization.h"
#include <stdexcept>
#include <cstdio>
#include <cstring>

#undef PAPUGA_LOWLEVEL_DEBUG

using namespace papuga;

static void validateRootExpression( const char* rootexpr)
{
	if (!rootexpr[0] && rootexpr[ std::strlen(rootexpr)-1] == '/')
	{
		throw papuga::runtime_error( _TXT("invalid path '%s' as node root path (slash at end of path)"), rootexpr);
	}
}

void RequestAutomaton_FunctionDef::addToAutomaton( const std::string& rootexpr, papuga_RequestAutomaton* atm, papuga_SchemaDescription* descr) const
{
	std::string fullexpr = rootexpr + expression;
#ifdef PAPUGA_LOWLEVEL_DEBUG
	fprintf( stderr, "ATM define function expression='%s', method=[%d,%d], self=%s, result=%s, n=%d\n",
		 fullexpr.c_str(), methodid.classid, methodid.functionid, selfvar, resultvar, (int)args.size());
#endif
	if (!papuga_RequestAutomaton_add_call( atm, fullexpr.c_str(), &methodid, selfvar, resultvar, appendresult, args.size()))
	{
		papuga_ErrorCode errcode = papuga_RequestAutomaton_last_error( atm);
		if (errcode != papuga_Ok) throw papuga::runtime_error( _TXT("request automaton add function, expression %s: %s"), fullexpr.c_str(), papuga_ErrorCode_tostring(errcode));
	}
	if (!papuga_SchemaDescription_add_element( descr, -1/*id*/, fullexpr.c_str(), papuga_TypeVoid, NULL/*examples*/))
	{
		papuga_ErrorCode errcode = papuga_SchemaDescription_last_error( descr);
		if (errcode != papuga_Ok) throw papuga::runtime_error( _TXT("schema description add element, expression %s: %s"), fullexpr.c_str(), papuga_ErrorCode_tostring(errcode));
	}
	std::vector<RequestAutomaton_FunctionDef::Arg>::const_iterator ai = args.begin(), ae = args.end();
	for (int aidx=0; ai != ae; ++ai,++aidx)
	{
		if (ai->varname)
		{
			if (!papuga_RequestAutomaton_set_call_arg_var( atm, aidx, ai->varname))
			{
				papuga_ErrorCode errcode = papuga_RequestAutomaton_last_error( atm);
				if (errcode != papuga_Ok) throw papuga::runtime_error( _TXT("request automaton add variable call arg, expression %s: %s"), fullexpr.c_str(), papuga_ErrorCode_tostring(errcode));
			}
		}
		else
		{
			if (!papuga_RequestAutomaton_set_call_arg_item( atm, aidx, ai->itemid, ai->resolvetype, ai->max_tag_diff))
			{
				papuga_ErrorCode errcode = papuga_RequestAutomaton_last_error( atm);
				if (errcode != papuga_Ok) throw papuga::runtime_error( _TXT("request automaton add item call arg, expression %s: %s"), fullexpr.c_str(), papuga_ErrorCode_tostring(errcode));
			}
		}
	}
}

void RequestAutomaton_StructDef::addToAutomaton( const std::string& rootexpr, papuga_RequestAutomaton* atm, papuga_SchemaDescription* descr) const
{
	std::string fullexpr = rootexpr + expression;
#ifdef PAPUGA_LOWLEVEL_DEBUG
	fprintf( stderr, "ATM define structure expression='%s', itemid=%d, n=%d\n", fullexpr.c_str(), itemid, (int)elems.size());
#endif	
	if (!papuga_RequestAutomaton_add_structure( atm, fullexpr.c_str(), itemid, elems.size()))
	{
		papuga_ErrorCode errcode = papuga_RequestAutomaton_last_error( atm);
		if (errcode != papuga_Ok) throw papuga::runtime_error( _TXT("request automaton add structure, expression %s: %s"), fullexpr.c_str(), papuga_ErrorCode_tostring(errcode));
	}
	if (!papuga_SchemaDescription_add_element( descr, itemid, fullexpr.c_str(), papuga_TypeVoid, NULL/*examples*/))
	{
		papuga_ErrorCode errcode = papuga_SchemaDescription_last_error( descr);
		if (errcode != papuga_Ok) throw papuga::runtime_error( _TXT("schema description add element, expression %s: %s"), fullexpr.c_str(), papuga_ErrorCode_tostring(errcode));
	}
	std::vector<RequestAutomaton_StructDef::Element>::const_iterator ei = elems.begin(), ee = elems.end();
	for (int eidx=0; ei != ee; ++ei,++eidx)
	{
		if (!papuga_RequestAutomaton_set_structure_element( atm, eidx, ei->name, ei->itemid, ei->resolvetype, ei->max_tag_diff))
		{
			papuga_ErrorCode errcode = papuga_RequestAutomaton_last_error( atm);
			if (errcode != papuga_Ok) throw papuga::runtime_error( _TXT("request automaton add structure element, expression %s: %s"), fullexpr.c_str(), papuga_ErrorCode_tostring(errcode));
		}
		if (!papuga_SchemaDescription_add_relation( descr, itemid, ei->itemid, ei->resolvetype))
		{
			papuga_ErrorCode errcode = papuga_SchemaDescription_last_error( descr);
			if (errcode != papuga_Ok) throw papuga::runtime_error( _TXT("schema description add structure element, expression %s: %s"), fullexpr.c_str(), papuga_ErrorCode_tostring(errcode));
		}
	}
}

void RequestAutomaton_ValueDef::addToAutomaton( const std::string& rootexpr, papuga_RequestAutomaton* atm, papuga_SchemaDescription* descr) const
{
	std::string scope_fullexpr = rootexpr + scope_expression;
	std::string select_fullexpr = rootexpr + select_expression;
#ifdef PAPUGA_LOWLEVEL_DEBUG
	fprintf( stderr, "ATM define value scope='%s', select=%s, id=%d\n", scope_fullexpr.c_str(), select_fullexpr.c_str(), itemid);
#endif	
	if (!papuga_RequestAutomaton_add_value( atm, scope_fullexpr.c_str(), select_fullexpr.c_str(), itemid))
	{
		papuga_ErrorCode errcode = papuga_RequestAutomaton_last_error( atm);
		if (errcode != papuga_Ok) throw papuga::runtime_error( _TXT("request automaton add value, scope expression %s select expression %s: %s"), scope_fullexpr.c_str(), select_fullexpr.c_str(), papuga_ErrorCode_tostring(errcode));
	}
	if (!papuga_SchemaDescription_add_element( descr, -1/*id*/, scope_fullexpr.c_str(), papuga_TypeVoid, NULL/*examples*/))
	{
		papuga_ErrorCode errcode = papuga_SchemaDescription_last_error( descr);
		if (errcode != papuga_Ok) throw papuga::runtime_error( _TXT("schema description add element, expression %s: %s"), scope_fullexpr.c_str(), papuga_ErrorCode_tostring(errcode));
	}
	if (!papuga_SchemaDescription_add_element( descr, itemid, select_fullexpr.c_str(), papuga_TypeVoid, examples))
	{
		papuga_ErrorCode errcode = papuga_SchemaDescription_last_error( descr);
		if (errcode != papuga_Ok) throw papuga::runtime_error( _TXT("schema description add element, expression %s: %s"), select_fullexpr.c_str(), papuga_ErrorCode_tostring(errcode));
	}
}

void RequestAutomaton_GroupDef::addToAutomaton( const std::string& rootexpr, papuga_RequestAutomaton* atm, papuga_SchemaDescription* descr) const
{
#ifdef PAPUGA_LOWLEVEL_DEBUG
	fprintf( stderr, "ATM start group\n");
#endif	
	if (!papuga_RequestAutomaton_open_group( atm))
	{
		papuga_ErrorCode errcode = papuga_RequestAutomaton_last_error( atm);
		if (errcode != papuga_Ok) throw papuga::runtime_error( _TXT("request automaton open group: %s"), papuga_ErrorCode_tostring(errcode));
	}
	std::vector<RequestAutomaton_FunctionDef>::const_iterator ni = nodes.begin(), ne = nodes.end();
	for (; ni != ne; ++ni)
	{
		ni->addToAutomaton( rootexpr, atm, descr);
	}
#ifdef PAPUGA_LOWLEVEL_DEBUG
	fprintf( stderr, "ATM end group\n");
#endif	
	if (!papuga_RequestAutomaton_close_group( atm))
	{
		papuga_ErrorCode errcode = papuga_RequestAutomaton_last_error( atm);
		if (errcode != papuga_Ok) throw papuga::runtime_error( _TXT("request automaton close group: %s"), papuga_ErrorCode_tostring(errcode));
	}
}

#if __cplusplus >= 201103L
RequestAutomaton_Node::~RequestAutomaton_Node()
{
	switch (type)
	{
		case Empty:
			break;
		case Function:
			delete value.functiondef;
			break;
		case Struct:
			delete value.structdef;
			break;
		case Value:
			delete value.valuedef;
			break;
		case Group:
			delete value.groupdef;
			break;
		case NodeList:
			delete value.nodelist;
			break;
	}
}
RequestAutomaton_Node::RequestAutomaton_Node()
	:type(Empty)
{
	value.functiondef = 0;
}
RequestAutomaton_Node::RequestAutomaton_Node( const std::initializer_list<RequestAutomaton_FunctionDef>& nodes)
	:type(Group)
{
	value.groupdef = new RequestAutomaton_GroupDef( std::vector<RequestAutomaton_FunctionDef>( nodes.begin(), nodes.end()));
}
RequestAutomaton_Node::RequestAutomaton_Node( const char* expression, const char* resultvar, const char* selfvar, const papuga_RequestMethodId& methodid, const std::initializer_list<RequestAutomaton_FunctionDef::Arg>& args)
	:type(Function)
{
	value.functiondef = new RequestAutomaton_FunctionDef( expression, resultvar?resultvar:"", selfvar, methodid, std::vector<RequestAutomaton_FunctionDef::Arg>( args.begin(), args.end()));
}
RequestAutomaton_Node::RequestAutomaton_Node( const char* expression, int itemid, const std::initializer_list<RequestAutomaton_StructDef::Element>& elems)
	:type(Struct)
{
	value.structdef = new RequestAutomaton_StructDef( expression, itemid, std::vector<RequestAutomaton_StructDef::Element>( elems.begin(), elems.end()));
}
RequestAutomaton_Node::RequestAutomaton_Node( const char* scope_expression, const char* select_expression, int itemid, const char* examples)
	:type(Value)
{
	value.valuedef = new RequestAutomaton_ValueDef( scope_expression, select_expression, itemid, examples);
}
RequestAutomaton_Node::RequestAutomaton_Node( const RequestAutomaton_NodeList& nodelist_)
	:type(NodeList)
{
	value.nodelist = new RequestAutomaton_NodeList( nodelist_);
}
RequestAutomaton_Node::RequestAutomaton_Node( const RequestAutomaton_Node& o)
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
		case Group:
			value.groupdef = new RequestAutomaton_GroupDef( *o.value.groupdef);
			break;
		case NodeList:
			value.nodelist = new RequestAutomaton_NodeList( *o.value.nodelist);
			break;
	}
}

void RequestAutomaton_Node::addToAutomaton( papuga_RequestAutomaton* atm, papuga_SchemaDescription* descr) const
{
	switch (type)
	{
		case Empty:
			break;
		case Function:
			value.functiondef->addToAutomaton( rootexpr, atm, descr);
			break;
		case Struct:
			value.structdef->addToAutomaton( rootexpr, atm, descr);
			break;
		case Value:
			value.valuedef->addToAutomaton( rootexpr, atm, descr);
			break;
		case Group:
			value.groupdef->addToAutomaton( rootexpr, atm, descr);
			break;
		case NodeList:
			for (auto node: *value.nodelist)
			{
				node.addToAutomaton( atm, descr);
			}
			break;
	}
}

RequestAutomaton_Node& RequestAutomaton_Node::root( const char* rootexpr_)
{
	if (type == NodeList)
	{
		for (auto node: *value.nodelist) node.root( rootexpr_);
	}
	else
	{
		validateRootExpression( rootexpr_);
		rootexpr = rootexpr_ + rootexpr;
	}
	return *this;
}
#endif

RequestAutomaton::RequestAutomaton( const papuga_ClassDef* classdefs, const papuga_StructInterfaceDescription* structdefs, const char* answername)
	:m_atm(papuga_create_RequestAutomaton(classdefs,structdefs,answername))
	,m_descr(papuga_create_SchemaDescription())
{
	if (!m_atm || !m_descr)
	{
		if (m_atm) papuga_destroy_RequestAutomaton( m_atm);
		if (m_descr) papuga_destroy_SchemaDescription( m_descr);
		throw std::bad_alloc();
	}
}

#if __cplusplus >= 201103L
RequestAutomaton::RequestAutomaton( const papuga_ClassDef* classdefs, const papuga_StructInterfaceDescription* structdefs, const char* answername,
					const std::initializer_list<InheritedDef>& inherited,
					const std::initializer_list<RequestAutomaton_Node>& nodes)
	:m_atm(papuga_create_RequestAutomaton(classdefs,structdefs,answername))
	,m_descr(papuga_create_SchemaDescription())
{
	if (!m_atm || !m_descr)
	{
		if (m_atm) papuga_destroy_RequestAutomaton( m_atm);
		if (m_descr) papuga_destroy_SchemaDescription( m_descr);
		throw std::bad_alloc();
	}
	for (auto hi : inherited)
	{
		if (!papuga_RequestAutomaton_inherit_from( m_atm, hi.type.c_str(), hi.name_expression.c_str(), hi.required))
		{
			papuga_ErrorCode errcode = papuga_RequestAutomaton_last_error( m_atm);
			if (errcode != papuga_Ok) throw papuga::runtime_error( _TXT("request automaton add inherit from: %s"), papuga_ErrorCode_tostring(errcode));
		}
	}
	for (auto ni : nodes)
	{
		ni.addToAutomaton( m_atm, m_descr);
	}
	papuga_RequestAutomaton_done( m_atm);
	papuga_SchemaDescription_done( m_descr);
}
#endif

RequestAutomaton::~RequestAutomaton()
{
	papuga_destroy_RequestAutomaton( m_atm);
	papuga_destroy_SchemaDescription( m_descr);
}

void RequestAutomaton::addFunction( const char* expression, const char* resultvar, const char* selfvar, const papuga_RequestMethodId& methodid, const RequestAutomaton_FunctionDef::Arg* args)
{
	std::vector<RequestAutomaton_FunctionDef::Arg> argvec;
	RequestAutomaton_FunctionDef::Arg const* ai = args;
	for (; ai->itemid || ai->varname; ++ai) argvec.push_back( *ai);
	RequestAutomaton_FunctionDef func( expression, resultvar?resultvar:"", selfvar, methodid, argvec);
	func.addToAutomaton( m_rootexpr, m_atm, m_descr);
}

void RequestAutomaton::addInheritContext( const char* typenam, const char* expression, bool required)
{
#ifdef PAPUGA_LOWLEVEL_DEBUG
	fprintf( stderr, "ATM define inherit context type=%s, expression='%s', required=%s\n",
		 typenam, expression, required ? "yes":"no");
#endif	
	if (!papuga_RequestAutomaton_inherit_from( m_atm, typenam, expression, required))
	{
		papuga_ErrorCode errcode = papuga_RequestAutomaton_last_error( m_atm);
		if (errcode != papuga_Ok) throw papuga::runtime_error( _TXT("request automaton add inherit context, expression %s: %s"), expression, papuga_ErrorCode_tostring(errcode));
	}
}

void RequestAutomaton::addStruct( const char* expression, int itemid, const RequestAutomaton_StructDef::Element* elems)
{
	std::vector<RequestAutomaton_StructDef::Element> elemvec;
	RequestAutomaton_StructDef::Element const* ei = elems;
	for (; ei->name; ++ei) elemvec.push_back( *ei);
	RequestAutomaton_StructDef st( expression, itemid, elemvec);
	st.addToAutomaton( m_rootexpr, m_atm, m_descr);
}

void RequestAutomaton::addValue( const char* scope_expression, const char* select_expression, int itemid, const char* examples)
{
	RequestAutomaton_ValueDef val( scope_expression, select_expression, itemid, examples);
	val.addToAutomaton( m_rootexpr, m_atm, m_descr);
}

void RequestAutomaton::openGroup()
{
	if (!papuga_RequestAutomaton_open_group( m_atm))
	{
		papuga_ErrorCode errcode = papuga_RequestAutomaton_last_error( m_atm);
		if (errcode != papuga_Ok) throw papuga::runtime_error( _TXT("request automaton open group: %s"), papuga_ErrorCode_tostring(errcode));
	}
}

void RequestAutomaton::closeGroup()
{
	if (!papuga_RequestAutomaton_close_group( m_atm))
	{
		papuga_ErrorCode errcode = papuga_RequestAutomaton_last_error( m_atm);
		if (errcode != papuga_Ok) throw papuga::runtime_error( _TXT("request automaton close group: %s"), papuga_ErrorCode_tostring(errcode));
	}
}

void RequestAutomaton::openRoot( const char* expr)
{
	m_rootstk.push_back( m_rootexpr.size());
	m_rootexpr.append( expr);
}

void RequestAutomaton::closeRoot()
{
	if (!m_rootstk.empty())
	{
		m_rootexpr.resize( m_rootstk.back());
		m_rootstk.pop_back();
	}
	else
	{
		throw std::runtime_error( _TXT("request automaton close root invalid"));
	}
}

void RequestAutomaton::done()
{
	papuga_RequestAutomaton_done( m_atm);
}

