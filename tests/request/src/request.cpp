/*
 * Copyright (c) 2017 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/// \brief Some classes and functions for building test requests in a convenient way
/// \file request.cpp
#include "request.hpp"
#include <stdexcept>
#include <cstdio>

#define PAPUGA_LOWLEVEL_DEBUG

using namespace papuga;
using namespace papuga::test;

static bool skipSpaces( char const*& si)
{
	while (*si && (unsigned char)*si <= 32) ++si;
	return *si != 0;
}

static bool isDigit( char ch)
{
	return (ch >= '0' && ch <= '9');
}

static bool isAlpha( char ch)
{
	ch |= 32;
	return (ch >= 'a' && ch <= 'z') || ch == '_';
}

static bool isAlnum( char ch)
{
	return isAlpha(ch)||isDigit(ch);
}

static std::string parse_identifier( char const*& si)
{
	std::string rt;
	skipSpaces( si);
	if (!isAlpha(*si)) throw std::runtime_error("identifier expected");
	for (; isAlnum(*si); ++si)
	{
		rt.push_back( *si);
	}
	skipSpaces(si);
	return rt;
}

void RequestAutomaton_FunctionDef::parseCall( const char* call)
{
	char const* si = 0;
	try
	{
		si = call;
		if (!skipSpaces(si)) throw std::runtime_error("call is empty");
		std::string name = parse_identifier( si);
		
		if (*si == '=')
		{
			resultvarname = name;
			++si;
			name = parse_identifier( si);
		}
		if (si[0] == '-' && si[1] == '>')
		{
			selfvarname = name;
			si += 2;
			name = parse_identifier( si);
		}
		if (*si == ':')
		{
			classname = name;
			++si;
			if (*si != ':') throw std::runtime_error("expected '::' to separate method name from class in call");
			++si;
			name = parse_identifier( si);
		}
		methodname = name;
	}
	catch (const std::runtime_error& err)
	{
		char errbuf[ 2048];
		std::snprintf( errbuf, sizeof( errbuf), "error in call call '%s' at position %u: %s", call, (int)(si-call), err.what());
		throw std::runtime_error( errbuf);
	}
}

void RequestAutomaton_FunctionDef::addToAutomaton( papuga_RequestAutomaton* atm) const
{
#ifdef PAPUGA_LOWLEVEL_DEBUG
	fprintf( stderr, "ATM define function class=%s, method=%s, self=%s, result=%s, n=%d\n", classname.c_str(), methodname.c_str(), selfvarname.c_str(), resultvarname.c_str(), (int)args.size());
#endif	
	if (!papuga_RequestAutomaton_add_call( atm, expression.c_str(), classname.c_str(),
						methodname.c_str(), selfvarname.c_str(),
						resultvarname.c_str(), args.size()))
	{
		papuga_ErrorCode errcode = papuga_RequestAutomaton_last_error( atm);
		if (errcode != papuga_Ok) error_exception( errcode, "request automaton add function");
	}
	std::vector<RequestAutomaton_FunctionDef::Arg>::const_iterator ai = args.begin(), ae = args.end();
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
}

void RequestAutomaton_StructDef::addToAutomaton( papuga_RequestAutomaton* atm) const
{
#ifdef PAPUGA_LOWLEVEL_DEBUG
	fprintf( stderr, "ATM define structure expression='%s', itemid=%d, n=%d\n", expression.c_str(), itemid, (int)elems.size());
#endif	
	if (!papuga_RequestAutomaton_add_structure( atm, expression.c_str(), itemid, elems.size()))
	{
		papuga_ErrorCode errcode = papuga_RequestAutomaton_last_error( atm);
		if (errcode != papuga_Ok) error_exception( errcode, "request automaton add structure");
	}
	std::vector<RequestAutomaton_StructDef::Element>::const_iterator ei = elems.begin(), ee = elems.end();
	for (int eidx=0; ei != ee; ++ei,++eidx)
	{
		if (!papuga_RequestAutomaton_set_structure_element( atm, eidx, ei->name, ei->itemid, ei->inherited))
		{
			papuga_ErrorCode errcode = papuga_RequestAutomaton_last_error( atm);
			if (errcode != papuga_Ok) error_exception( errcode, "request automaton add structure element");
		}
	}
}

void RequestAutomaton_ValueDef::addToAutomaton( papuga_RequestAutomaton* atm) const
{
#ifdef PAPUGA_LOWLEVEL_DEBUG
	fprintf( stderr, "ATM define value scope='%s', select=%s, id=%d\n", scope_expression.c_str(), select_expression.c_str(), itemid);
#endif	
	if (!papuga_RequestAutomaton_add_value( atm, scope_expression.c_str(), select_expression.c_str(), itemid))
	{
		papuga_ErrorCode errcode = papuga_RequestAutomaton_last_error( atm);
		if (errcode != papuga_Ok) error_exception( errcode, "request automaton add value");
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
		if (errcode != papuga_Ok) error_exception( errcode, "request automaton open group");
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
		if (errcode != papuga_Ok) error_exception( errcode, "request automaton close group");
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

RequestAutomaton_Node::RequestAutomaton_Node( const char* expression, const char* call, const std::initializer_list<RequestAutomaton_FunctionDef::Arg>& args)
	:type(Function)
{
	value.functiondef = new RequestAutomaton_FunctionDef( expression, call, std::vector<RequestAutomaton_FunctionDef::Arg>( args.begin(), args.end()));
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

RequestAutomaton::RequestAutomaton()
	:m_atm(papuga_create_RequestAutomaton())
{
	if (!m_atm) throw std::bad_alloc();
}

#if __cplusplus >= 201103L
RequestAutomaton::RequestAutomaton( const std::initializer_list<RequestAutomaton_Node>& nodes)
	:m_atm(papuga_create_RequestAutomaton())
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

void RequestAutomaton::addFunction( const char* expression, const char* call, const RequestAutomaton_FunctionDef::Arg* args)
{
	std::vector<RequestAutomaton_FunctionDef::Arg> argvec;
	RequestAutomaton_FunctionDef::Arg const* ai = args;
	for (; ai->itemid || ai->varname; ++ai) argvec.push_back( *ai);
	RequestAutomaton_FunctionDef func( expression, call, argvec);
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
		if (errcode != papuga_Ok) error_exception( errcode, "request automaton open group");
	}
}

void RequestAutomaton::closeGroup()
{
	if (!papuga_RequestAutomaton_close_group( m_atm))
	{
		papuga_ErrorCode errcode = papuga_RequestAutomaton_last_error( m_atm);
		if (errcode != papuga_Ok) error_exception( errcode, "request automaton close group");
	}
}

void RequestAutomaton::done()
{
	papuga_RequestAutomaton_done( m_atm);
}

