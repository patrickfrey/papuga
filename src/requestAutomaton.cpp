﻿/*
 * Copyright (c) 2017 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/// \brief Structure to define an automaton mapping a request to function calls in C++ in a convenient way
/// \file requestAutomaton.cpp
#include "papuga/requestAutomaton.hpp"
#include "papuga/requestResult.h"
#include "papuga/classdef.h"
#include "papuga/request.h"
#include "papuga/errors.hpp"
#include "papuga/errors.h"
#include "private/internationalization.h"
#include <stdexcept>
#include <cstdio>
#include <cstring>

using namespace papuga;

static std::string joinExpression( const std::string& expr1, const std::string& expr2)
{
	if (expr2 == "/" && !expr1.empty()) return expr1;
	return expr2.empty() || expr2[0] == '/' || expr2[0] == '(' || expr1.empty() || expr1[expr1.size()-1] == '/'
			? expr1 + expr2
			: expr1 + "/" + expr2;
}

void RequestAutomaton_FunctionDef::addToAutomaton( const std::string& rootexpr, papuga_RequestAutomaton* atm, papuga_SchemaDescription* descr, MapItemIdToName itemName) const
{
	std::string scope_fullexpr = joinExpression( rootexpr, scope_expression);
	std::string select_fullexpr = joinExpression( scope_fullexpr, select_expression);

	if (!papuga_RequestAutomaton_add_call( atm, select_fullexpr.c_str(), &methodid, selfvar, resultvar, args.size()))
	{
		papuga_ErrorCode errcode = papuga_RequestAutomaton_last_error( atm);
		if (errcode != papuga_Ok) throw papuga::runtime_error( _TXT("request automaton add function, expression '%s': %s"), select_fullexpr.c_str(), papuga_ErrorCode_tostring(errcode));
	}
	std::vector<RequestAutomaton_FunctionDef::Arg>::const_iterator ai = args.begin(), ae = args.end();
	for (int aidx=0; ai != ae; ++ai,++aidx)
	{
		if (ai->varname)
		{
			if (!papuga_RequestAutomaton_set_call_arg_var( atm, aidx, ai->varname))
			{
				papuga_ErrorCode errcode = papuga_RequestAutomaton_last_error( atm);
				if (errcode != papuga_Ok) throw papuga::runtime_error( _TXT("request automaton add variable as call arg, expression '%s': %s"), select_fullexpr.c_str(), papuga_ErrorCode_tostring(errcode));
			}
		}
		else
		{
			if (!papuga_RequestAutomaton_set_call_arg_item( atm, aidx, ai->itemid, ai->resolvetype, ai->max_tag_diff))
			{
				papuga_ErrorCode errcode = papuga_RequestAutomaton_last_error( atm);
				if (errcode != papuga_Ok) throw papuga::runtime_error( _TXT("request automaton add item '%s' as call arg, expression '%s': %s"), itemName(ai->itemid), select_fullexpr.c_str(), papuga_ErrorCode_tostring(errcode));
			}
			if (!papuga_SchemaDescription_add_dependency( descr, select_fullexpr.c_str(), ai->itemid, ai->resolvetype))
			{
				papuga_ErrorCode errcode = papuga_SchemaDescription_last_error( descr);
				if (errcode != papuga_Ok) throw papuga::runtime_error( _TXT("schema description add dependency to item '%s', expression '%s': %s"), itemName(ai->itemid), select_fullexpr.c_str(), papuga_ErrorCode_tostring(errcode));
			}
		}
	}
	if (prioritize())
	{
		papuga_RequestAutomaton_prioritize_last_call( atm, scope_fullexpr.c_str());
	}
}

void RequestAutomaton_StructDef::addToAutomaton( const std::string& rootexpr, papuga_RequestAutomaton* atm, papuga_SchemaDescription* descr, MapItemIdToName itemName) const
{
	std::string fullexpr = joinExpression( rootexpr, expression);
	if (!papuga_RequestAutomaton_add_structure( atm, fullexpr.c_str(), itemid, elems.size()))
	{
		papuga_ErrorCode errcode = papuga_RequestAutomaton_last_error( atm);
		if (errcode != papuga_Ok) throw papuga::runtime_error( _TXT("request automaton add structure '%s', expression '%s': %s"), itemName(itemid), fullexpr.c_str(), papuga_ErrorCode_tostring(errcode));
	}
	if (!papuga_SchemaDescription_add_element( descr, itemid, fullexpr.c_str(), papuga_TypeVoid, papuga_ResolveTypeRequired, NULL/*examples*/))
	{
		papuga_ErrorCode errcode = papuga_SchemaDescription_last_error( descr);
		if (errcode != papuga_Ok) throw papuga::runtime_error( _TXT("schema description add structure '%s', expression '%s': %s"), itemName(itemid), fullexpr.c_str(), papuga_ErrorCode_tostring(errcode));
	}
	std::vector<RequestAutomaton_StructDef::Element>::const_iterator ei = elems.begin(), ee = elems.end();
	for (int eidx=0; ei != ee; ++ei,++eidx)
	{
		if (ei->varname)
		{
			if (!papuga_RequestAutomaton_set_structure_element_var( atm, eidx, ei->name, ei->varname))
			{
				papuga_ErrorCode errcode = papuga_RequestAutomaton_last_error( atm);
				if (errcode != papuga_Ok) throw papuga::runtime_error( _TXT("request automaton add variable '%s' to structure '%s', expression '%s': %s"), ei->varname, itemName(itemid), fullexpr.c_str(), papuga_ErrorCode_tostring(errcode));
			}
		}
		else if (ei->itemid >= 0)
		{
			if (!papuga_RequestAutomaton_set_structure_element_item( atm, eidx, ei->name, ei->itemid, ei->resolvetype, ei->max_tag_diff))
			{
				papuga_ErrorCode errcode = papuga_RequestAutomaton_last_error( atm);
				if (errcode != papuga_Ok) throw papuga::runtime_error( _TXT("request automaton add element '%s' to structure '%s', expression '%s': %s"), itemName(ei->itemid), itemName(itemid), fullexpr.c_str(), papuga_ErrorCode_tostring(errcode));
			}
			if (!papuga_SchemaDescription_add_relation( descr, itemid, fullexpr.c_str(), ei->itemid, ei->resolvetype))
			{
				papuga_ErrorCode errcode = papuga_SchemaDescription_last_error( descr);
				if (errcode != papuga_Ok) throw papuga::runtime_error( _TXT("schema description add element '%s' to structure '%s', expression '%s': %s"), itemName(ei->itemid), itemName(itemid), fullexpr.c_str(), papuga_ErrorCode_tostring(errcode));
			}
		}
		else
		{
			throw papuga::runtime_error( _TXT("request automaton add element to structure, expression '%s': %s"), fullexpr.c_str(), papuga_ErrorCode_tostring(papuga_ValueUndefined));
		}
	}
}

std::string RequestAutomaton_StructDef::key( const std::string& rootexpr) const

{
	char itemidbuf[ 64];
	std::snprintf( itemidbuf, sizeof(itemidbuf), "%d", itemid);
	std::string rt( joinExpression( rootexpr, expression));
	rt.push_back(' ');
	rt.append( itemidbuf);
	return rt;
}

void RequestAutomaton_ValueDef::addToAutomaton( const std::string& rootexpr, papuga_RequestAutomaton* atm, papuga_SchemaDescription* descr, MapItemIdToName itemName) const
{
	std::string scope_fullexpr = joinExpression( rootexpr, scope_expression);
	std::string descr_fullexpr = joinExpression( scope_fullexpr, select_expression);
	if (valuetype != papuga_TypeVoid)
	{
		//... Null value (Void) type triggers only description
		if (!papuga_RequestAutomaton_add_value( atm, scope_fullexpr.c_str(), select_expression, itemid))
		{
			papuga_ErrorCode errcode = papuga_RequestAutomaton_last_error( atm);
			if (errcode != papuga_Ok) throw papuga::runtime_error( _TXT("request automaton add value '%s', scope expression '%s' select expression '%s': %s"), itemName(itemid), scope_fullexpr.c_str(), select_expression, papuga_ErrorCode_tostring(errcode));
		}
	}
	if (!papuga_SchemaDescription_add_element( descr, itemid/*id*/, descr_fullexpr.c_str(), valuetype, papuga_ResolveTypeRequired, examples))
	{
		papuga_ErrorCode errcode = papuga_SchemaDescription_last_error( descr);
		if (errcode != papuga_Ok) throw papuga::runtime_error( _TXT("schema description add value '%s', expression '%s': %s"), itemName(itemid), descr_fullexpr.c_str(), papuga_ErrorCode_tostring(errcode));
	}
}

std::string RequestAutomaton_ValueDef::key( const std::string& rootexpr) const
{
	char itemidbuf[ 64];
	std::snprintf( itemidbuf, sizeof(itemidbuf), "%d", itemid);
	std::string rt = joinExpression( joinExpression( rootexpr, scope_expression), select_expression);
	rt.push_back(' ');
	rt.append( itemidbuf);
	return rt;
}

void RequestAutomaton_GroupDef::addToAutomaton( const std::string& rootexpr, papuga_RequestAutomaton* atm, papuga_SchemaDescription* descr, MapItemIdToName itemName) const
{
	if (!papuga_RequestAutomaton_open_group( atm, groupid))
	{
		papuga_ErrorCode errcode = papuga_RequestAutomaton_last_error( atm);
		if (errcode != papuga_Ok) throw papuga::runtime_error( _TXT("request automaton open group: %s"), papuga_ErrorCode_tostring(errcode));
	}
	std::vector<RequestAutomaton_FunctionDef>::const_iterator ni = nodes.begin(), ne = nodes.end();
	for (; ni != ne; ++ni)
	{
		ni->addToAutomaton( rootexpr, atm, descr, itemName);
	}
	if (!papuga_RequestAutomaton_close_group( atm))
	{
		papuga_ErrorCode errcode = papuga_RequestAutomaton_last_error( atm);
		if (errcode != papuga_Ok) throw papuga::runtime_error( _TXT("request automaton close group: %s"), papuga_ErrorCode_tostring(errcode));
	}
}

void RequestAutomaton_ResolveDef::addToAutomaton( const std::string& rootexpr, papuga_RequestAutomaton* atm, papuga_SchemaDescription* descr, MapItemIdToName itemName) const
{
	std::string fullexpr = joinExpression( rootexpr, expression);
	if (!papuga_SchemaDescription_set_resolve( descr, fullexpr.c_str(), resolvetype))
	{
		papuga_ErrorCode errcode = papuga_SchemaDescription_last_error( descr);
		if (errcode != papuga_Ok) throw papuga::runtime_error( _TXT("schema description add resolve type, expression '%s': %s"), fullexpr.c_str(), papuga_ErrorCode_tostring(errcode));
	}
}

void RequestAutomaton_ResultElementDefList::defineRoot( const char* rootexpr)
{
	std::vector<RequestAutomaton_ResultElementDef>::iterator ri = begin(), re = end();
	for (; ri != re; ++ri)
	{
		ri->inputselect = joinExpression( rootexpr, ri->inputselect);
	}
}

void RequestAutomaton_ResultDef::addToAutomaton( papuga_RequestAutomaton* atm, MapItemIdToName itemName) const
{
	papuga_RequestResultDescription* descr = papuga_create_RequestResultDescription( m_name, m_schema, m_requestmethod, m_addressvar, m_path);
	if (!descr) throw std::bad_alloc();
	RequestAutomaton_ResultElementDefList::const_iterator ei = m_elements.begin(), ee = m_elements.end();
	for (; ei != ee; ++ei)
	{
		switch (ei->type)
		{
			case RequestAutomaton_ResultElementDef::Empty:
				throw std::runtime_error(_TXT("empty element in result definition structure"));
				break;
			case RequestAutomaton_ResultElementDef::Structure:
				if (!papuga_RequestResultDescription_push_structure( descr, ei->inputselect.c_str(), ei->tagname, false)) throw std::bad_alloc();
				break;
			case RequestAutomaton_ResultElementDef::Array:
				if (!papuga_RequestResultDescription_push_structure( descr, ei->inputselect.c_str(), ei->tagname, true)) throw std::bad_alloc();
				break;
			case RequestAutomaton_ResultElementDef::Constant:
				if (!papuga_RequestResultDescription_push_constant( descr, ei->inputselect.c_str(), ei->tagname, ei->str)) throw std::bad_alloc();
				break;
			case RequestAutomaton_ResultElementDef::InputReference:
				if (!papuga_RequestResultDescription_push_input( descr, ei->inputselect.c_str(), ei->tagname, ei->itemid, ei->resolvetype)) throw std::bad_alloc();
				break;
			case RequestAutomaton_ResultElementDef::ResultReference:
				if (!papuga_RequestResultDescription_push_callresult( descr, ei->inputselect.c_str(), ei->tagname, ei->str, ei->resolvetype)) throw std::bad_alloc();
				break;
		}
	}
	if (m_contentvars.size() > papuga_RequestResultDescription_MaxNofContentVars)
	{
		throw std::runtime_error( papuga_ErrorCode_tostring( papuga_BufferOverflowError));
	}
	std::vector<const char*>::const_iterator ci = m_contentvars.begin(), ce = m_contentvars.end();
	for (; ci != ce; ++ci)
	{
		if (!papuga_RequestResultDescription_push_content_variable( descr, *ci)) throw std::bad_alloc();
	}
	if (!papuga_RequestAutomation_add_result( atm, descr)) throw std::bad_alloc();
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
RequestAutomaton_Node::RequestAutomaton_Node( int groupid, const std::initializer_list<RequestAutomaton_GroupDef::Element>& nodes)
	:type(Group),rootexpr(),thisid(getClassId())
{
	value.groupdef = new RequestAutomaton_GroupDef( groupid, std::vector<RequestAutomaton_GroupDef::Element>( nodes.begin(), nodes.end()));
}
RequestAutomaton_Node::RequestAutomaton_Node( const char* expression, const char* resultvar, const char* selfvar, const papuga_RequestMethodId& methodid, const std::initializer_list<RequestAutomaton_FunctionDef::Arg>& args)
	:type(Function),rootexpr(),thisid(getClassId())
{
	value.functiondef = new RequestAutomaton_FunctionDef( expression, "", resultvar?resultvar:"", selfvar, methodid, std::vector<RequestAutomaton_FunctionDef::Arg>( args.begin(), args.end()));
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
RequestAutomaton_Node::RequestAutomaton_Node( const char* scope_expression, const char* select_expression, const char* variable, int itemid, char resolvechr, int max_tag_diff)
	:type(Function),rootexpr(),thisid(getClassId())
{
	// Assignment implemented as function
	papuga_RequestMethodId methodid = {0,0};
	std::vector<RequestAutomaton_FunctionDef::Arg> args;
	args.push_back( RequestAutomaton_FunctionDef::Arg( itemid, resolvechr, max_tag_diff));
	value.functiondef = new RequestAutomaton_FunctionDef( scope_expression, select_expression, variable?variable:"", 0/*selfvar*/, methodid, args);
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

void RequestAutomaton_Node::addToAutomaton( const std::string& rootpath_, papuga_RequestAutomaton* atm, papuga_SchemaDescription* descr, MapItemIdToName itemName, std::set<std::string>& keyset, std::set<std::string>& accepted_root_tags) const
{
	std::string rootpath = joinExpression( rootpath_, rootexpr);
	if (rootpath_.empty() && rootexpr.size() >= 2)
	{
		if (rootexpr[0] == '/' && rootexpr[1] != '/')
		{
			char const* ei = std::strchr( rootexpr.c_str()+1, '/');
			if (!ei) ei = std::strchr( rootexpr.c_str()+1, '\0');
			accepted_root_tags.insert( std::string( rootexpr.c_str()+1, ei-rootexpr.c_str()-1));
		}
	}
	switch (type)
	{
		case Empty:
			break;
		case Function:
			value.functiondef->addToAutomaton( rootpath, atm, descr, itemName);
			break;
		case Struct:
			if (keyset.insert( value.structdef->key( rootpath)).second)
			{
				value.structdef->addToAutomaton( rootpath, atm, descr, itemName);
			}
			else
			{
				std::string key = value.structdef->key( rootpath);
				throw papuga::runtime_error( _TXT("request automaton define duplicate structure '%s'"), key.c_str());
			}
			break;
		case Value:
			if (keyset.insert( value.valuedef->key( rootpath)).second)
			{
				value.valuedef->addToAutomaton( rootpath, atm, descr, itemName);
			}
			else
			{
				std::string key = value.valuedef->key( rootpath);
				throw papuga::runtime_error( _TXT("request automaton define duplicate value '%s'"), key.c_str());
			}
			break;
		case Group:
			value.groupdef->addToAutomaton( rootpath, atm, descr, itemName);
			break;
		case NodeList:
			for (auto node: *value.nodelist)
			{
				node.addToAutomaton( rootpath, atm, descr, itemName, keyset, accepted_root_tags);
			}
			break;
		case ResolveDef:
			value.resolvedef->addToAutomaton( rootpath, atm, descr, itemName);
			break;
	}
}
#endif

RequestAutomaton::RequestAutomaton(
		const papuga_ClassDef* classdefs,
		const papuga_StructInterfaceDescription* structdefs,
		MapItemIdToName mapItemIdToName,
		bool strict, bool exclusiveAccess)
	:m_atm(papuga_create_RequestAutomaton(classdefs,structdefs,strict,exclusiveAccess))
	,m_descr(papuga_create_SchemaDescription())
	,m_mapItemIdToName(mapItemIdToName)
	,m_rootexpr(),m_rootstk()
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
					MapItemIdToName mapItemIdToName,
					bool strict, bool exclusiveAccess,
					const std::initializer_list<RequestAutomaton_EnvironmentAssigmentDef>& envdefs,
					const std::initializer_list<RequestAutomaton_ResultDef>& resultdefs,
					const std::initializer_list<InheritedDef>& inherited,
					const std::initializer_list<RequestAutomaton_Node>& nodes)
	:m_atm(papuga_create_RequestAutomaton(classdefs,structdefs,strict,exclusiveAccess))
	,m_descr(papuga_create_SchemaDescription())
	,m_mapItemIdToName(mapItemIdToName)
	,m_rootexpr(),m_rootstk()
{
	std::set<std::string> keyset;
	if (!m_atm || !m_descr)
	{
		if (m_atm) papuga_destroy_RequestAutomaton( m_atm);
		if (m_descr) papuga_destroy_SchemaDescription( m_descr);
		throw std::bad_alloc();
	}
	try
	{
		std::set<std::string> accepted_root_tags; //< set of accepted requests (identified by the root tag)
		
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
					throw papuga::runtime_error( _TXT("schema description check and compile error (expression '%s'): %s"), errexpr, papuga_ErrorCode_tostring(errcode));
				}
			}
		}
		for (auto ni : nodes)
		{
			ni.addToAutomaton( "", m_atm, m_descr, mapItemIdToName, keyset, accepted_root_tags);
		}
		for (auto ri : resultdefs)
		{
			ri.addToAutomaton( m_atm, mapItemIdToName);
		}
		for (auto ei : envdefs)
		{
			if (!papuga_RequestAutomation_add_env_assignment( m_atm, ei.variable, ei.envid, ei.argument))
			{
				throw papuga::runtime_error( _TXT("error in request automaton definition of environment variable assignments: %s"), papuga_ErrorCode_tostring(papuga_RequestAutomaton_last_error(m_atm)));
			}
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
				throw papuga::runtime_error( _TXT("schema description check and compile error (expression '%s'): %s"), errexpr, papuga_ErrorCode_tostring(errcode));
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
	RequestAutomaton_FunctionDef func( expression, "", resultvar?resultvar:"", selfvar, methodid, argvec);
	func.addToAutomaton( m_rootexpr, m_atm, m_descr, m_mapItemIdToName);
}

void RequestAutomaton::addAssignment( const char* scope_expression, const char* select_expression, const char* varname, int itemid, char resolvechr, int max_tag_diff)
{
	// Assignment implemented as function
	papuga_RequestMethodId methodid = {0,0};
	std::vector<RequestAutomaton_FunctionDef::Arg> args;
	args.push_back( RequestAutomaton_FunctionDef::Arg( itemid, resolvechr, max_tag_diff));
	RequestAutomaton_FunctionDef assignment( scope_expression, select_expression, varname?varname:"", 0/*selfvar*/, methodid, args);
	assignment.addToAutomaton( m_rootexpr, m_atm, m_descr, m_mapItemIdToName);
}

void RequestAutomaton::addEnvAssignment( const char* variable, int envid, const char* argument)
{
	if (!papuga_RequestAutomation_add_env_assignment( m_atm, variable, envid, argument))
	{
		throw papuga::runtime_error( _TXT("error in request automaton definition of environment variable assignments: %s"), papuga_ErrorCode_tostring(papuga_RequestAutomaton_last_error(m_atm)));
	}
}

void RequestAutomaton::addInheritContext( const char* typenam, const char* expression, bool required)
{
	if (!papuga_RequestAutomaton_inherit_from( m_atm, typenam, expression, required))
	{
		papuga_ErrorCode errcode = papuga_RequestAutomaton_last_error( m_atm);
		if (errcode != papuga_Ok) throw papuga::runtime_error( _TXT("request automaton add inherit context, expression '%s': %s"), expression, papuga_ErrorCode_tostring(errcode));
	}
	if (!papuga_SchemaDescription_add_element( m_descr, -1/*NullId*/, expression, papuga_TypeString, required?papuga_ResolveTypeRequired:papuga_ResolveTypeOptional, "analyzer;storage"))
	{
		papuga_ErrorCode errcode = papuga_SchemaDescription_last_error( m_descr);
		if (errcode != papuga_Ok)
		{
			const char* errexpr = papuga_SchemaDescription_error_expression( m_descr);
			if (!errexpr) errexpr = "<unknown>";
			throw papuga::runtime_error( _TXT("schema description check and compile error (expression '%s'): %s"), errexpr, papuga_ErrorCode_tostring(errcode));
		}
	}
}

void RequestAutomaton::addResult( const RequestAutomaton_ResultDef& resultdef)
{
	resultdef.addToAutomaton( m_atm, m_mapItemIdToName);
}

void RequestAutomaton::addStruct( const char* expression, int itemid, const RequestAutomaton_StructDef::Element* elems)
{
	std::vector<RequestAutomaton_StructDef::Element> elemvec;
	RequestAutomaton_StructDef::Element const* ei = elems;
	for (; ei->name; ++ei) elemvec.push_back( *ei);
	RequestAutomaton_StructDef st( expression, itemid, elemvec);
	st.addToAutomaton( m_rootexpr, m_atm, m_descr, m_mapItemIdToName);
}

void RequestAutomaton::addValue( const char* scope_expression, const char* select_expression, int itemid, papuga_Type valuetype, const char* examples)
{
	RequestAutomaton_ValueDef val( scope_expression, select_expression, itemid, valuetype, examples);
	val.addToAutomaton( m_rootexpr, m_atm, m_descr, m_mapItemIdToName);
}

void RequestAutomaton::setResolve( const char* expression, char resolvechr)
{
	if (!papuga_SchemaDescription_set_resolve( m_descr, expression, getResolveType( resolvechr)))
	{
		papuga_ErrorCode errcode = papuga_RequestAutomaton_last_error( m_atm);
		if (errcode != papuga_Ok) throw papuga::runtime_error( _TXT("request automaton set resolve: %s"), papuga_ErrorCode_tostring(errcode));
	}
}

void RequestAutomaton::openGroup( int groupid)
{
	if (!papuga_RequestAutomaton_open_group( m_atm, groupid))
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
			throw papuga::runtime_error( _TXT("schema description check and compile error (expression '%s'): %s"), errexpr, papuga_ErrorCode_tostring(errcode));
		}
	}
}

