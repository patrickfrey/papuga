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

static std::string joinExpression( const std::string& expr1, const std::string& expr2)
{
	if (expr2 == "/" && !expr1.empty()) return expr1;
	return expr2.empty() || expr2[0] == '/' || expr2[0] == '(' || expr1.empty() || expr1[expr1.size()-1] == '/'
			? expr1 + expr2
			: expr1 + "/" + expr2;
}

void RequestAutomaton_FunctionDef::addToAutomaton( const std::string& rootexpr, papuga_RequestAutomaton* atm, papuga_SchemaDescription* descr) const
{
	std::string fullexpr = joinExpression( rootexpr, expression);
#ifdef PAPUGA_LOWLEVEL_DEBUG
	fprintf( stderr, "ATM define function expression='%s', method=[%d,%d], self=%s, result=%s, n=%d\n",
		 fullexpr.c_str(), methodid.classid, methodid.functionid, selfvar, resultvar, (int)args.size());
#endif
	if (!papuga_RequestAutomaton_add_call( atm, fullexpr.c_str(), &methodid, selfvar, resultvar, appendresult, args.size()))
	{
		papuga_ErrorCode errcode = papuga_RequestAutomaton_last_error( atm);
		if (errcode != papuga_Ok) throw papuga::runtime_error( _TXT("request automaton add function, expression %s: %s"), fullexpr.c_str(), papuga_ErrorCode_tostring(errcode));
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
		if (!papuga_SchemaDescription_add_dependency( descr, fullexpr.c_str(), ai->itemid, ai->resolvetype))
		{
			papuga_ErrorCode errcode = papuga_SchemaDescription_last_error( descr);
			if (errcode != papuga_Ok) throw papuga::runtime_error( _TXT("schema description add dependency, expression %s: %s"), fullexpr.c_str(), papuga_ErrorCode_tostring(errcode));
		}
	}
}

void RequestAutomaton_StructDef::addToAutomaton( const std::string& rootexpr, papuga_RequestAutomaton* atm, papuga_SchemaDescription* descr) const
{
	std::string fullexpr = joinExpression( rootexpr, expression);
#ifdef PAPUGA_LOWLEVEL_DEBUG
	fprintf( stderr, "ATM define structure expression='%s', itemid=%d, n=%d\n", fullexpr.c_str(), itemid, (int)elems.size());
#endif	
	if (!papuga_RequestAutomaton_add_structure( atm, fullexpr.c_str(), itemid, elems.size()))
	{
		papuga_ErrorCode errcode = papuga_RequestAutomaton_last_error( atm);
		if (errcode != papuga_Ok) throw papuga::runtime_error( _TXT("request automaton add structure, expression %s: %s"), fullexpr.c_str(), papuga_ErrorCode_tostring(errcode));
	}
	if (!papuga_SchemaDescription_add_element( descr, itemid, fullexpr.c_str(), papuga_TypeVoid, papuga_ResolveTypeRequired, NULL/*examples*/))
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
		if (!papuga_SchemaDescription_add_relation( descr, itemid, fullexpr.c_str(), ei->itemid, ei->resolvetype))
		{
			papuga_ErrorCode errcode = papuga_SchemaDescription_last_error( descr);
			if (errcode != papuga_Ok) throw papuga::runtime_error( _TXT("schema description add structure element, expression %s: %s"), fullexpr.c_str(), papuga_ErrorCode_tostring(errcode));
		}
	}
}

void RequestAutomaton_ValueDef::addToAutomaton( const std::string& rootexpr, papuga_RequestAutomaton* atm, papuga_SchemaDescription* descr) const
{
	std::string scope_fullexpr = joinExpression( rootexpr, scope_expression);
	std::string descr_fullexpr = joinExpression( scope_fullexpr, select_expression);
#ifdef PAPUGA_LOWLEVEL_DEBUG
	fprintf( stderr, "ATM define value scope='%s', select=%s, id=%d\n", scope_fullexpr.c_str(), select_expression, itemid);
#endif	
	if (!papuga_RequestAutomaton_add_value( atm, scope_fullexpr.c_str(), select_expression, itemid))
	{
		papuga_ErrorCode errcode = papuga_RequestAutomaton_last_error( atm);
		if (errcode != papuga_Ok) throw papuga::runtime_error( _TXT("request automaton add value, scope expression %s select expression %s: %s"), scope_fullexpr.c_str(), select_expression, papuga_ErrorCode_tostring(errcode));
	}
	if (!papuga_SchemaDescription_add_element( descr, itemid/*id*/, descr_fullexpr.c_str(), valuetype, papuga_ResolveTypeRequired, NULL/*examples*/))
	{
		papuga_ErrorCode errcode = papuga_SchemaDescription_last_error( descr);
		if (errcode != papuga_Ok) throw papuga::runtime_error( _TXT("schema description add element, expression %s: %s"), descr_fullexpr.c_str(), papuga_ErrorCode_tostring(errcode));
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

void RequestAutomaton_ResolveDef::addToAutomaton( const std::string& rootexpr, papuga_RequestAutomaton* atm, papuga_SchemaDescription* descr) const
{
	std::string fullexpr = joinExpression( rootexpr, expression);
#ifdef PAPUGA_LOWLEVEL_DEBUG
	fprintf( stderr, "ATM define resolve type\n");
#endif	
	if (!papuga_SchemaDescription_set_resolve( descr, fullexpr.c_str(), resolvetype))
	{
		papuga_ErrorCode errcode = papuga_SchemaDescription_last_error( descr);
		if (errcode != papuga_Ok) throw papuga::runtime_error( _TXT("schema description add resolve type, expression %s: %s"), fullexpr.c_str(), papuga_ErrorCode_tostring(errcode));
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
		case ResolveDef:
			delete value.resolvedef;
			break;
	}
}

static int getClassId()
{
	static int idgen = 0;
	return ++idgen;
}

RequestAutomaton_Node::RequestAutomaton_Node()
	:type(Empty),rootexpr(),thisid(getClassId())
{
	value.functiondef = 0;
}
RequestAutomaton_Node::RequestAutomaton_Node( const std::initializer_list<RequestAutomaton_FunctionDef>& nodes)
	:type(Group),rootexpr(),thisid(getClassId())
{
	value.groupdef = new RequestAutomaton_GroupDef( std::vector<RequestAutomaton_FunctionDef>( nodes.begin(), nodes.end()));
}
RequestAutomaton_Node::RequestAutomaton_Node( const char* expression, const char* resultvar, const char* selfvar, const papuga_RequestMethodId& methodid, const std::initializer_list<RequestAutomaton_FunctionDef::Arg>& args)
	:type(Function),rootexpr(),thisid(getClassId())
{
	value.functiondef = new RequestAutomaton_FunctionDef( expression, resultvar?resultvar:"", selfvar, methodid, std::vector<RequestAutomaton_FunctionDef::Arg>( args.begin(), args.end()));
}
RequestAutomaton_Node::RequestAutomaton_Node( const char* expression, int itemid, const std::initializer_list<RequestAutomaton_StructDef::Element>& elems)
	:type(Struct),rootexpr(),thisid(getClassId())
{
	value.structdef = new RequestAutomaton_StructDef( expression, itemid, std::vector<RequestAutomaton_StructDef::Element>( elems.begin(), elems.end()));
}
RequestAutomaton_Node::RequestAutomaton_Node( const char* scope_expression, const char* select_expression, int itemid, papuga_Type valuetype, const char* examples)
	:type(Value),rootexpr(),thisid(getClassId())
{
	value.valuedef = new RequestAutomaton_ValueDef( scope_expression, select_expression, itemid, valuetype, examples);
}
RequestAutomaton_Node::RequestAutomaton_Node( const char* expression, char resolvechr)
	:type(ResolveDef),rootexpr(),thisid(getClassId())
{
	value.resolvedef = new RequestAutomaton_ResolveDef( expression, resolvechr);
}
RequestAutomaton_Node::RequestAutomaton_Node( const RequestAutomaton_NodeList& nodelist_)
	:type(NodeList),rootexpr(),thisid(getClassId())
{
	value.nodelist = new RequestAutomaton_NodeList( nodelist_);
}

static void copyNodeValueUnion( RequestAutomaton_Node::Type type, RequestAutomaton_Node::ValueUnion& dest, const RequestAutomaton_Node::ValueUnion& src)
{
	switch (type)
	{
		case RequestAutomaton_Node::Empty:
			dest.functiondef = 0;
			break;
		case RequestAutomaton_Node::Function:
			dest.functiondef = new RequestAutomaton_FunctionDef( *src.functiondef);
			break;
		case RequestAutomaton_Node::Struct:
			dest.structdef = new RequestAutomaton_StructDef( *src.structdef);
			break;
		case RequestAutomaton_Node::Value:
			dest.valuedef = new RequestAutomaton_ValueDef( *src.valuedef);
			break;
		case RequestAutomaton_Node::Group:
			dest.groupdef = new RequestAutomaton_GroupDef( *src.groupdef);
			break;
		case RequestAutomaton_Node::NodeList:
			dest.nodelist = new RequestAutomaton_NodeList( *src.nodelist);
			break;
		case RequestAutomaton_Node::ResolveDef:
			dest.resolvedef = new RequestAutomaton_ResolveDef( *src.resolvedef);
			break;
	}
}

RequestAutomaton_Node::RequestAutomaton_Node( const RequestAutomaton_Node& o)
	:type(o.type),rootexpr(o.rootexpr),thisid(getClassId())
{
	copyNodeValueUnion( type, value, o.value);
}

RequestAutomaton_Node::RequestAutomaton_Node( const std::string& rootprefix, const RequestAutomaton_Node& o)
	:type(o.type),rootexpr(joinExpression( rootprefix, o.rootexpr)),thisid(getClassId())
{
	copyNodeValueUnion( type, value, o.value);
}

RequestAutomaton_Node::RequestAutomaton_Node( RequestAutomaton_Node&& o)
	:type(o.type),rootexpr(std::move(o.rootexpr)),thisid(o.thisid)
{
	std::memcpy( &value, &o.value, sizeof(value));
	std::memset( &o.value, 0, sizeof(value));
}

RequestAutomaton_Node& RequestAutomaton_Node::operator=( RequestAutomaton_Node&& o)
{
	type = o.type;
	rootexpr = std::move(o.rootexpr);
	thisid = o.thisid;
	std::memcpy( &value, &o.value, sizeof(value));
	std::memset( &o.value, 0, sizeof(value));
	return *this;
}

RequestAutomaton_Node& RequestAutomaton_Node::operator=( const RequestAutomaton_Node& o)
{
	type = o.type;
	rootexpr = o.rootexpr;
	thisid = o.thisid;
	copyNodeValueUnion( type, value, o.value);
	return *this;
}

void RequestAutomaton_Node::addToAutomaton( const std::string& rootpath_, papuga_RequestAutomaton* atm, papuga_SchemaDescription* descr) const
{
	std::string rootpath = joinExpression( rootpath_, rootexpr);
	switch (type)
	{
		case Empty:
			break;
		case Function:
			value.functiondef->addToAutomaton( rootpath, atm, descr);
			break;
		case Struct:
			value.structdef->addToAutomaton( rootpath, atm, descr);
			break;
		case Value:
			value.valuedef->addToAutomaton( rootpath, atm, descr);
			break;
		case Group:
			value.groupdef->addToAutomaton( rootpath, atm, descr);
			break;
		case NodeList:
			for (auto node: *value.nodelist)
			{
				node.addToAutomaton( rootpath, atm, descr);
			}
			break;
		case ResolveDef:
			value.resolvedef->addToAutomaton( rootpath, atm, descr);
			break;
	}
}
#endif

RequestAutomaton::RequestAutomaton(
		const papuga_ClassDef* classdefs,
		const papuga_StructInterfaceDescription* structdefs,
		const char* answername,
		bool mergeInputAnswer)
	:m_atm(papuga_create_RequestAutomaton(classdefs,structdefs,answername,mergeInputAnswer))
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
RequestAutomaton::RequestAutomaton( const papuga_ClassDef* classdefs,
					const papuga_StructInterfaceDescription* structdefs,
					const char* answername,
					bool mergeInputAnswer,
					const std::initializer_list<InheritedDef>& inherited,
					const std::initializer_list<RequestAutomaton_Node>& nodes)
	:m_atm(papuga_create_RequestAutomaton(classdefs,structdefs,answername,mergeInputAnswer))
	,m_descr(papuga_create_SchemaDescription())
{
	if (!m_atm || !m_descr)
	{
		if (m_atm) papuga_destroy_RequestAutomaton( m_atm);
		if (m_descr) papuga_destroy_SchemaDescription( m_descr);
		throw std::bad_alloc();
	}
	try
	{
		for (auto hi : inherited)
		{
			if (!papuga_RequestAutomaton_inherit_from( m_atm, hi.type.c_str(), hi.name_expression.c_str(), hi.required))
			{
				papuga_ErrorCode errcode = papuga_RequestAutomaton_last_error( m_atm);
				if (errcode != papuga_Ok) throw papuga::runtime_error( _TXT("request automaton add inherit from error: %s"), papuga_ErrorCode_tostring(errcode));
			}
			if (!papuga_SchemaDescription_add_element( m_descr, -1/*NullId*/, hi.name_expression.c_str(), papuga_TypeString, hi.required ? papuga_ResolveTypeRequired : papuga_ResolveTypeOptional, "analyzer;storage"))
			{
				papuga_ErrorCode errcode = papuga_SchemaDescription_last_error( m_descr);
				if (errcode != papuga_Ok)
				{
					const char* errexpr = papuga_SchemaDescription_error_expression( m_descr);
					if (!errexpr) errexpr = "<unknown>";
					throw papuga::runtime_error( _TXT("schema description check and compile error (expression %s): %s"), errexpr, papuga_ErrorCode_tostring(errcode));
				}
			}
		}
		for (auto ni : nodes)
		{
			ni.addToAutomaton( "", m_atm, m_descr);
		}
		if (!papuga_RequestAutomaton_done( m_atm))
		{
			papuga_ErrorCode errcode = papuga_RequestAutomaton_last_error( m_atm);
			if (errcode != papuga_Ok) throw papuga::runtime_error( _TXT("request automaton check and compile error: %s"), papuga_ErrorCode_tostring(errcode));
		}
		if (!papuga_SchemaDescription_done( m_descr))
		{
			papuga_ErrorCode errcode = papuga_SchemaDescription_last_error( m_descr);
			if (errcode != papuga_Ok)
			{
				const char* errexpr = papuga_SchemaDescription_error_expression( m_descr);
				if (!errexpr) errexpr = "<unknown>";
				throw papuga::runtime_error( _TXT("schema description check and compile error (expression %s): %s"), errexpr, papuga_ErrorCode_tostring(errcode));
			}
		}
	}
	catch (const std::bad_alloc&)
	{
		if (m_atm) papuga_destroy_RequestAutomaton( m_atm);
		if (m_descr) papuga_destroy_SchemaDescription( m_descr);
		throw std::bad_alloc();
	}
	catch (const std::runtime_error& err)
	{
		if (m_atm) papuga_destroy_RequestAutomaton( m_atm);
		if (m_descr) papuga_destroy_SchemaDescription( m_descr);
		throw std::runtime_error( err.what());
	}
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
	if (!papuga_SchemaDescription_add_element( m_descr, -1/*NullId*/, expression, papuga_TypeString, required?papuga_ResolveTypeRequired:papuga_ResolveTypeOptional, "analyzer;storage"))
	{
		papuga_ErrorCode errcode = papuga_SchemaDescription_last_error( m_descr);
		if (errcode != papuga_Ok)
		{
			const char* errexpr = papuga_SchemaDescription_error_expression( m_descr);
			if (!errexpr) errexpr = "<unknown>";
			throw papuga::runtime_error( _TXT("schema description check and compile error (expression %s): %s"), errexpr, papuga_ErrorCode_tostring(errcode));
		}
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

void RequestAutomaton::addValue( const char* scope_expression, const char* select_expression, int itemid, papuga_Type valuetype, const char* examples)
{
	RequestAutomaton_ValueDef val( scope_expression, select_expression, itemid, valuetype, examples);
	val.addToAutomaton( m_rootexpr, m_atm, m_descr);
}

void RequestAutomaton::setResolve( const char* expression, char resolvechr)
{
	if (!papuga_SchemaDescription_set_resolve( m_descr, expression, getResolveType( resolvechr)))
	{
		papuga_ErrorCode errcode = papuga_RequestAutomaton_last_error( m_atm);
		if (errcode != papuga_Ok) throw papuga::runtime_error( _TXT("request automaton set resolve: %s"), papuga_ErrorCode_tostring(errcode));
	}
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
	if (!papuga_RequestAutomaton_done( m_atm))
	{
		papuga_ErrorCode errcode = papuga_RequestAutomaton_last_error( m_atm);
		if (errcode != papuga_Ok)
		{
			throw papuga::runtime_error( _TXT("request automaton check and compile error: %s"), papuga_ErrorCode_tostring(errcode));
		}
	}
	if (!papuga_SchemaDescription_done( m_descr))
	{
		papuga_ErrorCode errcode = papuga_SchemaDescription_last_error( m_descr);
		if (errcode != papuga_Ok)
		{
			const char* errexpr = papuga_SchemaDescription_error_expression( m_descr);
			if (!errexpr) errexpr = "<unknown>";
			throw papuga::runtime_error( _TXT("schema description check and compile error (expression %s): %s"), errexpr, papuga_ErrorCode_tostring(errcode));
		}
	}
}

