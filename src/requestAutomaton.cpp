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
#include <stdexcept>
#include <cstdio>

#undef PAPUGA_LOWLEVEL_DEBUG

using namespace papuga;

void RequestAutomaton_FunctionDef::addToAutomaton( papuga_RequestAutomaton* atm) const
{
#ifdef PAPUGA_LOWLEVEL_DEBUG
	fprintf( stderr, "ATM define function expression='%s', method=[%d,%d], self=%s, result=%s, n=%d\n",
		 expression, methodid.classid, methodid.functionid, selfvar, resultvar, (int)args.size());
#endif	
	if (!papuga_RequestAutomaton_add_call( atm, expression, &methodid, selfvar, resultvar, args.size()))
	{
		papuga_ErrorCode errcode = papuga_RequestAutomaton_last_error( atm);
		if (errcode != papuga_Ok) throw error_exception( errcode, "request automaton add function");
	}
	std::vector<RequestAutomaton_FunctionDef::Arg>::const_iterator ai = args.begin(), ae = args.end();
	for (int aidx=0; ai != ae; ++ai,++aidx)
	{
		if (ai->varname)
		{
			if (!papuga_RequestAutomaton_set_call_arg_var( atm, aidx, ai->varname))
			{
				papuga_ErrorCode errcode = papuga_RequestAutomaton_last_error( atm);
				if (errcode != papuga_Ok) throw error_exception( errcode, "request automaton add variable call arg");
			}
		}
		else
		{
			if (!papuga_RequestAutomaton_set_call_arg_item( atm, aidx, ai->itemid, ai->inherited, ai->defaultvalue))
			{
				papuga_ErrorCode errcode = papuga_RequestAutomaton_last_error( atm);
				if (errcode != papuga_Ok) throw error_exception( errcode, "request automaton add item call arg");
			}
		}
	}
}

void RequestAutomaton_StructDef::addToAutomaton( papuga_RequestAutomaton* atm) const
{
#ifdef PAPUGA_LOWLEVEL_DEBUG
	fprintf( stderr, "ATM define structure expression='%s', itemid=%d, n=%d\n", expression, itemid, (int)elems.size());
#endif	
	if (!papuga_RequestAutomaton_add_structure( atm, expression, itemid, elems.size()))
	{
		papuga_ErrorCode errcode = papuga_RequestAutomaton_last_error( atm);
		if (errcode != papuga_Ok) throw error_exception( errcode, "request automaton add structure");
	}
	std::vector<RequestAutomaton_StructDef::Element>::const_iterator ei = elems.begin(), ee = elems.end();
	for (int eidx=0; ei != ee; ++ei,++eidx)
	{
		if (!papuga_RequestAutomaton_set_structure_element( atm, eidx, ei->name, ei->itemid, ei->inherited))
		{
			papuga_ErrorCode errcode = papuga_RequestAutomaton_last_error( atm);
			if (errcode != papuga_Ok) throw error_exception( errcode, "request automaton add structure element");
		}
	}
}

void RequestAutomaton_ValueDef::addToAutomaton( papuga_RequestAutomaton* atm) const
{
#ifdef PAPUGA_LOWLEVEL_DEBUG
	fprintf( stderr, "ATM define value scope='%s', select=%s, id=%d\n", scope_expression, select_expression, itemid);
#endif	
	if (!papuga_RequestAutomaton_add_value( atm, scope_expression, select_expression, itemid))
	{
		papuga_ErrorCode errcode = papuga_RequestAutomaton_last_error( atm);
		if (errcode != papuga_Ok) throw error_exception( errcode, "request automaton add value");
	}
}

void RequestAutomaton_GroupDef::addToAutomaton( papuga_RequestAutomaton* atm) const
{
#ifdef PAPUGA_LOWLEVEL_DEBUG
	fprintf( stderr, "ATM start group\n");
#endif	
	if (!papuga_RequestAutomaton_open_group( atm))
	{
		papuga_ErrorCode errcode = papuga_RequestAutomaton_last_error( atm);
		if (errcode != papuga_Ok) throw error_exception( errcode, "request automaton open group");
	}
	std::vector<RequestAutomaton_FunctionDef>::const_iterator ni = nodes.begin(), ne = nodes.end();
	for (; ni != ne; ++ni)
	{
		ni->addToAutomaton( atm);
	}
#ifdef PAPUGA_LOWLEVEL_DEBUG
	fprintf( stderr, "ATM end group\n");
#endif	
	if (!papuga_RequestAutomaton_close_group( atm))
	{
		papuga_ErrorCode errcode = papuga_RequestAutomaton_last_error( atm);
		if (errcode != papuga_Ok) throw error_exception( errcode, "request automaton close group");
	}
}

#if __cplusplus >= 201103L
RequestAutomaton_Node::RequestAutomaton_Node()
	:type(Empty)
{
	value.functiondef = 0;
}

RequestAutomaton_Node::RequestAutomaton_Node( const RequestAutomaton_Node::Group_&, const std::initializer_list<RequestAutomaton_FunctionDef>& nodes)
	:type(Group)
{
	value.groupdef = new RequestAutomaton_GroupDef( std::vector<RequestAutomaton_FunctionDef>( nodes.begin(), nodes.end()));
}

RequestAutomaton_Node::RequestAutomaton_Node( const char* expression, const char* resultvar, const char* selfvar, const papuga_RequestMethodId& methodid, const std::initializer_list<RequestAutomaton_FunctionDef::Arg>& args)
	:type(Function)
{
	value.functiondef = new RequestAutomaton_FunctionDef( expression, resultvar, selfvar, methodid, std::vector<RequestAutomaton_FunctionDef::Arg>( args.begin(), args.end()));
}

RequestAutomaton_Node::RequestAutomaton_Node( const char* expression, int itemid, const std::initializer_list<RequestAutomaton_StructDef::Element>& elems)
	:type(Struct)
{
	value.structdef = new RequestAutomaton_StructDef( expression, itemid, std::vector<RequestAutomaton_StructDef::Element>( elems.begin(), elems.end()));
}

RequestAutomaton_Node::RequestAutomaton_Node( const char* scope_expression, const char* select_expression, int itemid)
	:type(Value)
{
	value.valuedef = new RequestAutomaton_ValueDef( scope_expression, select_expression, itemid);
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
	}
}

void RequestAutomaton_Node::addToAutomaton( papuga_RequestAutomaton* atm) const
{
	switch (type)
	{
		case Empty:
			break;
		case Function:
			value.functiondef->addToAutomaton( atm);
			break;
		case Struct:
			value.structdef->addToAutomaton( atm);
			break;
		case Value:
			value.valuedef->addToAutomaton( atm);
			break;
		case Group:
			value.groupdef->addToAutomaton( atm);
			break;
	}
}
#endif

RequestAutomaton::RequestAutomaton( const papuga_ClassDef* classdefs, const papuga_StructInterfaceDescription* structdefs, const char* answername)
	:m_atm(papuga_create_RequestAutomaton(classdefs,structdefs,answername))
{
	if (!m_atm) throw std::bad_alloc();
}

#if __cplusplus >= 201103L
RequestAutomaton::RequestAutomaton( const papuga_ClassDef* classdefs, const papuga_StructInterfaceDescription* structdefs, const char* answername, const std::initializer_list<RequestAutomaton_Node>& nodes)
	:m_atm(papuga_create_RequestAutomaton(classdefs,structdefs,answername))
{
	if (!m_atm) throw std::bad_alloc();
	std::initializer_list<RequestAutomaton_Node>::const_iterator ni = nodes.begin(), ne = nodes.end();
	for (; ni != ne; ++ni)
	{
		ni->addToAutomaton( m_atm);
	}
	papuga_RequestAutomaton_done( m_atm);
}
#endif

RequestAutomaton::~RequestAutomaton()
{
	papuga_destroy_RequestAutomaton( m_atm);
}

void RequestAutomaton::addFunction( const char* expression, const char* resultvar, const char* selfvar, const papuga_RequestMethodId& methodid, const RequestAutomaton_FunctionDef::Arg* args)
{
	std::vector<RequestAutomaton_FunctionDef::Arg> argvec;
	RequestAutomaton_FunctionDef::Arg const* ai = args;
	for (; ai->itemid || ai->varname; ++ai) argvec.push_back( *ai);
	RequestAutomaton_FunctionDef func( expression, resultvar, selfvar, methodid, argvec);
	func.addToAutomaton( m_atm);
}

void RequestAutomaton::addStruct( const char* expression, int itemid, const RequestAutomaton_StructDef::Element* elems)
{
	std::vector<RequestAutomaton_StructDef::Element> elemvec;
	RequestAutomaton_StructDef::Element const* ei = elems;
	for (; ei->name; ++ei) elemvec.push_back( *ei);
	RequestAutomaton_StructDef st( expression, itemid, elemvec);
	st.addToAutomaton( m_atm);
}

void RequestAutomaton::addValue( const char* scope_expression, const char* select_expression, int itemid)
{
	RequestAutomaton_ValueDef val( scope_expression, select_expression, itemid);
	val.addToAutomaton( m_atm);
}

void RequestAutomaton::openGroup()
{
	if (!papuga_RequestAutomaton_open_group( m_atm))
	{
		papuga_ErrorCode errcode = papuga_RequestAutomaton_last_error( m_atm);
		if (errcode != papuga_Ok) throw error_exception( errcode, "request automaton open group");
	}
}

void RequestAutomaton::closeGroup()
{
	if (!papuga_RequestAutomaton_close_group( m_atm))
	{
		papuga_ErrorCode errcode = papuga_RequestAutomaton_last_error( m_atm);
		if (errcode != papuga_Ok) throw error_exception( errcode, "request automaton close group");
	}
}

void RequestAutomaton::done()
{
	papuga_RequestAutomaton_done( m_atm);
}

