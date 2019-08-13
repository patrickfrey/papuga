/*
 * Copyright (c) 2017 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/// \brief Automaton to execute papuga XML and JSON requests
/// \file request.cpp
#include "papuga/request.h"
#include "papuga/requestHandler.h"
#include "papuga/serialization.h"
#include "papuga/serialization.hpp"
#include "papuga/allocator.h"
#include "papuga/valueVariant.h"
#include "papuga/valueVariant.hpp"
#include "papuga/callArgs.h"
#include "papuga/classdef.h"
#include "papuga/allocator.h"
#include "papuga/stack.h"
#include "papuga/errors.h"
#include "papuga/errors.hpp"
#include "textwolf/xmlpathautomatonparse.hpp"
#include "textwolf/xmlpathselect.hpp"
#include "textwolf/charset.hpp"
#include "request_utils.hpp"
#include "requestResult_utils.hpp"
#include <stdexcept>
#include <sstream>
#include <iostream>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <algorithm>
#include <limits>
#include <set>

using namespace papuga;

namespace {
struct CString
{
	const char* value;
	CString( const char* value_) :value(value_){}
	CString( const CString& o) :value(o.value){}

	bool operator<( const CString& o) const {return std::strcmp(value,o.value) < 0;}
};

struct ValueDef
{
	int itemid;

	ValueDef( int itemid_)
		:itemid(itemid_) {}
	ValueDef( const ValueDef& o)
		:itemid(o.itemid){}
};

struct StructMemberDef
{
	const char* name;
	int itemid;
	papuga_ResolveType resolvetype;
	int max_tag_diff;

	StructMemberDef()
		:name(0),itemid(0),resolvetype(papuga_ResolveTypeRequired),max_tag_diff(0){}
	StructMemberDef( const char* name_, int itemid_, papuga_ResolveType resolvetype_, int max_tag_diff_)
		:name(name_),itemid(itemid_),resolvetype(resolvetype_),max_tag_diff(max_tag_diff_){}
	void assign( const StructMemberDef& o)
		{name=o.name;itemid=o.itemid;resolvetype=o.resolvetype;max_tag_diff=o.max_tag_diff;}
};

struct StructDef
{
	StructMemberDef* members;
	int nofmembers;
	int itemid;

	StructDef( int itemid_, StructMemberDef* members_, int nofmembers_)
		:members(members_),nofmembers(nofmembers_),itemid(itemid_){}
	StructDef( const StructDef& o)
		:members(o.members),nofmembers(o.nofmembers),itemid(o.itemid){}
};

struct CallArgDef
{
	const char* varname;
	int itemid;
	papuga_ResolveType resolvetype;
	int max_tag_diff;

	CallArgDef()
		:varname(0),itemid(0),resolvetype(papuga_ResolveTypeRequired),max_tag_diff(0){}
	CallArgDef( const char* varname_)
		:varname(varname_),itemid(0),resolvetype(papuga_ResolveTypeRequired),max_tag_diff(0){}
	CallArgDef( int itemid_, papuga_ResolveType resolvetype_, int max_tag_diff_)
		:varname(0),itemid(itemid_),resolvetype(resolvetype_),max_tag_diff(max_tag_diff_){}
	void assign( const CallArgDef& o)
		{varname=o.varname;itemid=o.itemid;resolvetype=o.resolvetype;max_tag_diff=o.max_tag_diff;}
};

struct CallDef
{
	papuga_RequestMethodId methodid;
	const char* selfvarname;
	const char* resultvarname;
	CallArgDef* args;
	int nofargs;
	int groupid;

	CallDef( const papuga_RequestMethodId* methodid_, const char* selfvarname_, const char* resultvarname_, CallArgDef* args_, int nofargs_, int groupid_)
		:selfvarname(selfvarname_),resultvarname(resultvarname_),args(args_),nofargs(nofargs_),groupid(groupid_)
	{
		methodid.classid = methodid_->classid;
		methodid.functionid  = methodid_->functionid;
	}
	CallDef( const CallDef& o)
		:selfvarname(o.selfvarname),resultvarname(o.resultvarname),args(o.args),nofargs(o.nofargs),groupid(o.groupid)
	{
		methodid.classid = o.methodid.classid;
		methodid.functionid = o.methodid.functionid;
	}
};

typedef int AtmRef;
enum AtmRefType {InstantiateValue,CollectValue,CloseStruct,MethodCall,MethodCallPrioritize,InheritFrom,ResultInstruction};
enum {MaxAtmRefType=ResultInstruction};

static AtmRef AtmRef_get( AtmRefType type, int idx)	{return (AtmRef)(((int)type<<28) | (idx+1));}
static AtmRefType AtmRef_type( AtmRef atmref)		{return (AtmRefType)((atmref>>28) & 0x7);}
static int AtmRef_index( AtmRef atmref)			{return ((int)atmref & 0x0fFFffFF)-1;}

static bool ResRef_check( int resultidx, int instridx)	{return (resultidx >= 0 && resultidx < 256 && instridx >= 0 && instridx <= 1<<16);}
static int ResRef_get( int resultidx, int instridx)	{return (resultidx << 16) + instridx;}
static int ResRef_resultidx( int idx)			{return (idx >> 16) & 0xff;}
static int ResRef_instridx( int idx)			{return idx & 0xffFF;}

#define CATCH_LOCAL_EXCEPTION(ERRCODE,RETVAL)\
	catch (const std::bad_alloc&)\
	{\
		ERRCODE = papuga_NoMemError;\
		return RETVAL;\
	}\
	catch (...)\
	{\
		ERRCODE = papuga_UncaughtException;\
		return RETVAL;\
	}

static int nofClassDefs( const papuga_ClassDef* classdefs)
{
	papuga_ClassDef const* ci = classdefs;
	int classcnt = 0;
	for (; ci->name; ++ci,++classcnt){}
	return classcnt;
}

/// @brief Description of the automaton
class AutomatonDescription
{
public:
	typedef textwolf::XMLPathSelectAutomatonParser<> XMLPathSelectAutomaton;

	AutomatonDescription( const papuga_ClassDef* classdefs_, bool strict_)
		:m_classdefs(classdefs_)
		,m_nof_classdefs(nofClassDefs(classdefs_))
		,m_calldefs(),m_structdefs(),m_valuedefs(),m_inheritdefs(),m_resultdefs(),m_resultVariables()
		,m_acceptedRootTags(),m_strict(strict_)
		,m_atm(),m_maxitemid(0)
		,m_errcode(papuga_Ok),m_groupid(-1),m_done(false)
	{
		papuga_init_Allocator( &m_allocator, m_allocatorbuf, sizeof(m_allocatorbuf));
	}
	~AutomatonDescription()
	{
		papuga_destroy_Allocator( &m_allocator);

		std::vector<papuga_RequestResultDescription*>::const_iterator ri = m_resultdefs.begin(), re = m_resultdefs.end();
		for (; ri != re; ++ri) papuga_destroy_RequestResultDescription( *ri);
	}

	const papuga_ClassDef* classdefs() const
	{
		return m_classdefs;
	}
	const char* copyIfDefined( const char* str)
	{
		if (str && str[0])
		{
			str = papuga_Allocator_copy_charp( &m_allocator, str);
			if (str)
			{
				return str;
			}
			else
			{
				m_errcode = papuga_NoMemError;
			}
		}
		return NULL;
	}

	bool inheritFrom( const char* type, const char* name_expression, bool required)
	{
		try
		{
			int evid = AtmRef_get( InheritFrom, m_inheritdefs.size());
			m_inheritdefs.push_back( InheritFromDef( type, required));
			return addExpression( evid, name_expression);
		}
		CATCH_LOCAL_EXCEPTION(m_errcode,false)
		return true;
	}

	/* @param[in] scope_expression selector of the scope where the last call outweighs others with the same destination */
	bool prioritizeLastCallInScope( const char* scope_expression)
	{
		try
		{
			if (m_calldefs.empty())
			{
				m_errcode = papuga_ExecutionOrder;
				return false;
			}
			std::string open_expression( cut_trailing_slashes( scope_expression));
			std::string close_expression( open_expression + "~");

			int evid = AtmRef_get( MethodCallPrioritize, m_calldefs.size()-1);
			return addExpression( evid, close_expression);
		}
		catch (...)
		{
			m_errcode = papuga_NoMemError;
			return false;
		}
	}

	/* @param[in] expression selector of the call scope */
	/* @param[in] nofargs number of arguments */
	bool addCall( const char* expression, const papuga_RequestMethodId* method_, const char* selfvarname_, const char* resultvarname_, int nofargs)
	{
		try
		{
			if (m_done)
			{
				m_errcode = papuga_ExecutionOrder;
				return false;
			}
			if (nofargs > papuga_MAX_NOF_ARGUMENTS)
			{
				m_errcode = papuga_NofArgsError;
				return false;
			}
			if (method_->classid < 0 || method_->classid > m_nof_classdefs)
			{
				m_errcode = papuga_AddressedItemNotFound;
				return false;
			}
			if (method_->functionid && !selfvarname_)
			{
				m_errcode = papuga_MissingSelf;
				return false;
			}
			const papuga_ClassDef* cdef = 0;
			if (method_->classid)
			{
				cdef = &m_classdefs[ method_->classid-1];
				if (method_->functionid < 0 || method_->functionid > cdef->methodtablesize)
				{
					m_errcode = papuga_AddressedItemNotFound;
					return false;
				}
			}
			std::string open_expression( cut_trailing_slashes( expression));
			std::string close_expression( open_expression + "~");
			int mm = nofargs * sizeof(CallArgDef);
			CallArgDef* car = NULL;
			if (nofargs)
			{
				void* carmem = papuga_Allocator_alloc( &m_allocator, mm, 0);
				if (!carmem)
				{
					m_errcode = papuga_NoMemError;
					return false;
				}
				car = new (carmem) CallArgDef[ nofargs];
			}
			const char* selfvarname = copyIfDefined( selfvarname_);
			const char* resultvarname = copyIfDefined( resultvarname_);
			if (m_errcode != papuga_Ok) return false;

			int evid = AtmRef_get( MethodCall, m_calldefs.size());
			m_calldefs.push_back( CallDef( method_, selfvarname, resultvarname, car, nofargs, m_groupid < 0 ? m_calldefs.size():m_groupid));
			return addExpression( evid, close_expression);
		}
		CATCH_LOCAL_EXCEPTION(m_errcode,false)
		return true;
	}

	papuga_ErrorCode lastError() const
	{
		return m_errcode;
	}

	bool setCallArg( int idx, const CallArgDef& adef_)
	{
		try
		{
			if (m_done)
			{
				m_errcode = papuga_InvalidAccess;
				return false;
			}
			if (m_calldefs.empty() || idx < 0 || idx >= m_calldefs.back().nofargs)
			{
				m_errcode = papuga_InvalidAccess;
				return false;
			}
			CallArgDef& adef = m_calldefs.back().args[ idx];
			if (adef.varname || adef.itemid)
			{
				m_errcode = papuga_DuplicateDefinition;
				return false;
			}
			adef.itemid = adef_.itemid;
			adef.varname = adef_.varname;
			adef.resolvetype = adef_.resolvetype;
			adef.max_tag_diff = adef_.max_tag_diff;

			if (adef.max_tag_diff < 0) 
			{
				m_errcode = papuga_TypeError;
				return false;
			}
			if (!adef.varname)
			{
				if (!checkItemId( adef.itemid))
				{
					return false;
				}
			}
			return true;
		}
		CATCH_LOCAL_EXCEPTION(m_errcode,false)
		return true;
	}

	bool setCallArgVar( int idx, const char* argvar)
	{
		try
		{
			if (!argvar)
			{
				m_errcode = papuga_TypeError;
				return false;
			}
			CallArgDef adef( papuga_Allocator_copy_charp( &m_allocator, argvar));
			if (!adef.varname)
			{
				m_errcode = papuga_NoMemError;
				return false;
			}
			return setCallArg( idx, adef);
		}
		CATCH_LOCAL_EXCEPTION(m_errcode,false)
		return true;
	}

	bool setCallArgItem( int idx, int itemid, papuga_ResolveType resolvetype, int max_tag_diff)
	{
		try
		{
			if (itemid <= 0)
			{
				m_errcode = papuga_TypeError;
				return false;
			}
			CallArgDef adef( itemid, resolvetype, max_tag_diff);
			return setCallArg( idx, adef);
		}
		CATCH_LOCAL_EXCEPTION(m_errcode,false)
		return true;
	}

	/* @param[in] expression selector of the structure (tag) */
	/* @param[in] itemid identifier for the item associated with this structure */
	/* @param[in] nofmembers number of members */
	bool addStructure( const char* expression, int itemid, int nofmembers)
	{
		try
		{
			if (!checkItemId( itemid))
			{
				return false;
			}
			if (m_done)
			{
				m_errcode = papuga_ExecutionOrder;
				return false;
			}
			std::string open_expression( cut_trailing_slashes( expression));
			std::string close_expression( open_expression + "~");
			int mm = nofmembers * sizeof(StructMemberDef);
			void* marmem = papuga_Allocator_alloc( &m_allocator, mm, 0);
			if (!marmem)
			{
				m_errcode = papuga_NoMemError;
				return false;
			}
			StructMemberDef* mar = new (marmem) StructMemberDef[ nofmembers];
			int evid = AtmRef_get( CloseStruct, m_structdefs.size());
			m_structdefs.push_back( StructDef( itemid, mar, nofmembers));
			return addExpression( evid, close_expression);
		}
		CATCH_LOCAL_EXCEPTION(m_errcode,false)
		return true;
	}

	bool setMember( int idx, const char* name, int itemid, papuga_ResolveType resolvetype, int max_tag_diff)
	{
		try
		{
			if (!checkItemId( itemid))
			{
				return false;
			}
			if (m_done)
			{
				m_errcode = papuga_ExecutionOrder;
				return false;
			}
			if (m_structdefs.empty() || idx < 0 || idx >= m_structdefs.back().nofmembers)
			{
				m_errcode = papuga_InvalidAccess;
				return false;
			}
			if (itemid <= 0)
			{
				m_errcode = papuga_TypeError;
				return false;
			}
			if (max_tag_diff < -1) 
			{
				m_errcode = papuga_TypeError;
				return false;
			}
			StructMemberDef& mdef = m_structdefs.back().members[ idx];
			if (mdef.itemid)
			{
				m_errcode = papuga_DuplicateDefinition;
				return false;
			}
			mdef.itemid = itemid;
			mdef.resolvetype = resolvetype;
			mdef.max_tag_diff = max_tag_diff;
			if (name && name[0])
			{
				mdef.name = papuga_Allocator_copy_charp( &m_allocator, name);
				if (!mdef.name)
				{
					m_errcode = papuga_NoMemError;
					return false;
				}
			}
			else
			{
				mdef.name = 0;
			}
			return true;
		}
		CATCH_LOCAL_EXCEPTION(m_errcode,false)
		return true;
	}

	/* @param[in] scope_expression selector of the value scope (tag) */
	/* @param[in] select_expression selector of the value content */
	/* @param[in] itemid identifier for the item associated with this value */
	bool addValue( const char* scope_expression, const char* select_expression, int itemid)
	{
		try
		{
			if (!checkItemId( itemid))
			{
				return false;
			}
			if (m_done)
			{
				m_errcode = papuga_ExecutionOrder;
				return false;
			}
			std::string open_expression( cut_trailing_slashes( scope_expression));
			std::string close_expression;

			if (isSelectAttributeExpression( open_expression))
			{
				if (select_expression[0])
				{
					m_errcode = papuga_SyntaxError;
					return false;
				}
				close_expression = open_expression;
			}
			else
			{
				close_expression = open_expression + "~";
			}
			std::string value_expression;
			if (select_expression[0] == '\0')
			{
				value_expression = open_expression;
			}
			else if (select_expression[0] == '/')
			{
				if (select_expression[1] == '/')
				{
					value_expression.append( open_expression + select_expression);
				}
				else
				{
					m_errcode = papuga_SyntaxError;
					return false;
				}
			}
			else if (select_expression[0] == '@' || select_expression[0] == '(')
			{
				value_expression.append( open_expression + select_expression);
			}
			else
			{
				value_expression.append( open_expression + "/" + select_expression);
			}
			int evid_inst = AtmRef_get( InstantiateValue, m_valuedefs.size());
			int evid_coll = AtmRef_get( CollectValue, m_valuedefs.size());
			m_valuedefs.push_back( ValueDef( itemid));
			return addExpression( evid_inst, value_expression) && addExpression( evid_coll, close_expression);
		}
		CATCH_LOCAL_EXCEPTION(m_errcode,false)
		return true;
	}

	bool openGroup()
	{
		if (m_groupid >= 0)
		{
			m_errcode = papuga_LogicError;
			return false;
		}
		m_groupid = m_calldefs.size();
		return true;
	}

	bool closeGroup()
	{
		if (m_groupid < 0)
		{
			m_errcode = papuga_LogicError;
			return false;
		}
		m_groupid = -1;
		return true;
	}

	bool addResultDescription( papuga_RequestResultDescription* descr)
	{
		try
		{
			papuga_RequestResultNodeDescription* ni = descr->nodear;
			papuga_RequestResultNodeDescription* ne = descr->nodear + descr->nodearsize;
			for (std::size_t nidx=0; ni != ne; ++ni,++nidx)
			{
				switch (ni->type)
				{
					case papuga_ResultNodeConstant:
					case papuga_ResultNodeOpenStructure:
					case papuga_ResultNodeOpenArray:
						if (!addResultInstructionTrigger( ni->inputselect, m_resultdefs.size(), nidx))
						{
							return false;
						}
						break;
					case papuga_ResultNodeResultReference:
						m_resultVariables.insert( ni->value.str);
						/* no break here! */
					case papuga_ResultNodeInputReference:
					case papuga_ResultNodeCloseStructure:
					case papuga_ResultNodeCloseArray:
					{
						std::string closeexpr = cut_trailing_slashes( ni->inputselect);
						closeexpr.push_back( '~');
						if (!addResultInstructionTrigger( closeexpr, m_resultdefs.size(), nidx))
						{
							return false;
						}
						break;
					}
				}
			}
			m_resultdefs.push_back( descr);
		}
		CATCH_LOCAL_EXCEPTION(m_errcode,false)
		return true;
	}

	bool isResultVariable( const char* name) const
	{
		return m_resultVariables.find( name) != m_resultVariables.end();
	}

	bool isValidRootTagSet( const std::set<std::string>& rootTagSet) const
	{
		if (!m_strict) return true;
		if (rootTagSet.empty() && !m_acceptedRootTags.empty()) return false;
		std::set<std::string>::const_iterator ri = rootTagSet.begin(), re = rootTagSet.end();
		for (; ri != re; ++ri)
		{
			if (m_acceptedRootTags.find( *ri) == m_acceptedRootTags.end()) return false;
		}
		return true;
	}

	bool done()
	{
		if (m_done)
		{
			m_errcode = papuga_LogicError;
			return false;
		}
		m_done = true;
		return true;
	}

	struct InheritFromDef
	{
		std::string type;
		bool required;

		InheritFromDef( const std::string& type_, bool required_)
			:type(type_),required(required_){}
		InheritFromDef( const InheritFromDef& o)
			:type(o.type),required(o.required){}
	};

	const std::vector<CallDef>& calldefs() const				{return m_calldefs;}
	const std::vector<StructDef>& structdefs() const			{return m_structdefs;}
	const std::vector<ValueDef>& valuedefs() const				{return m_valuedefs;}
	const std::vector<InheritFromDef>& inheritdefs() const			{return m_inheritdefs;}
	const std::vector<papuga_RequestResultDescription*>& resultdefs() const	{return m_resultdefs;}
	const XMLPathSelectAutomaton& atm() const				{return m_atm;}
	std::size_t maxitemid() const						{return m_maxitemid;}

private:
	bool addExpression( int eventid, const char* expression)
	{
		std::size_t expressionsize = std::strlen(expression);
		extractRootTagName( expression);
		if (0!=m_atm.addExpression( eventid, expression, expressionsize))
		{
			m_errcode = papuga_SyntaxError;
			return false;
		}
		return true;
	}

	bool addExpression( int eventid, const std::string& expression)
	{
		extractRootTagName( expression.c_str());
		if (0!=m_atm.addExpression( eventid, expression.c_str(), expression.size()))
		{
			m_errcode = papuga_SyntaxError;
			return false;
		}
		return true;
	}

	void extractRootTagName( const char* expression)
	{
		if (expression[0] == '/' && expression[1] != '/')
		{
			char const* ei = std::strchr( expression+1, '/');
			if (!ei) ei = std::strchr( expression+1, '\0');
			if (*(ei-1) == '~') --ei;
			m_acceptedRootTags.insert( std::string( expression+1, ei-expression-1));
		}
	}

	bool addResultInstructionTrigger( const char* expression, std::size_t resultidx, std::size_t instridx)
	{
		if (!ResRef_check( resultidx, instridx)) 
		{
			m_errcode = papuga_BufferOverflowError;
			return false;
		}
		int idx = ResRef_get( resultidx, instridx);
		int evid = AtmRef_get( ResultInstruction, idx);

		return addExpression( evid, expression);
	}

	bool addResultInstructionTrigger( const std::string& expression, std::size_t resultidx, std::size_t instridx)
	{
		if (!ResRef_check( resultidx, instridx)) 
		{
			m_errcode = papuga_BufferOverflowError;
			return false;
		}
		int idx = ResRef_get( resultidx, instridx);
		int evid = AtmRef_get( ResultInstruction, idx);

		return addExpression( evid, expression);
	}

	static bool isSelectAttributeExpression( const std::string& expr)
	{
		const char* si = expr.c_str();
		char const* se = si + expr.size();
		for (; se != si && *(se-1) != '/' && *(se-1) != ']' && *(se-1) != '@'; --se){}
		return (se != si && *(se-1) == '@');
	}

	bool checkItemId( int itemid)
	{
		if (itemid <= 0 || itemid >= (1<<28))
		{
			m_errcode = papuga_OutOfRangeError;
			return false;
		}
		if ((int)m_maxitemid < itemid)
		{
			m_maxitemid = itemid;
		}
		return true;
	}

	static std::string cut_trailing_slashes( const char* expression)
	{
		std::size_t size = std::strlen( expression);
		while (size && (expression[size-1] == '/' || expression[size-1] == '~')) --size;
		return std::string( expression, size);
	}

private:
	const papuga_ClassDef* m_classdefs;			//< array of classes
	int m_nof_classdefs;					//< number of classes defined in m_classdefs
	std::vector<CallDef> m_calldefs;
	std::vector<StructDef> m_structdefs;
	std::vector<ValueDef> m_valuedefs;
	std::vector<InheritFromDef> m_inheritdefs;
	std::vector<papuga_RequestResultDescription*> m_resultdefs;
	std::set<CString> m_resultVariables;
	std::set<std::string> m_acceptedRootTags;		//< set of accepted requests (identified by the root tag)
	bool m_strict;						//< false, if the automaton accepts root tags that are not declared, used for parsing a structure embedded into a request
	papuga_Allocator m_allocator;
	XMLPathSelectAutomaton m_atm;
	std::size_t m_maxitemid;
	papuga_ErrorCode m_errcode;
	int m_groupid;
	bool m_done;
	char m_allocatorbuf[ 4096];
};


typedef int ObjectRef;
static bool ObjectRef_is_value( ObjectRef objref)	{return objref > 0;}
static bool ObjectRef_is_struct( ObjectRef objref)	{return objref < 0;}
static bool ObjectRef_is_defined( ObjectRef objref)	{return objref != 0;}
static int ObjectRef_struct_id( ObjectRef objref)	{return objref < 0 ? -objref-1 : 0;}
static int ObjectRef_value_id( ObjectRef objref)	{return objref > 0 ? objref-1 : 0;}
static ObjectRef ObjectRef_value( int idx)		{return (ObjectRef)(idx+1);}
static ObjectRef ObjectRef_struct( int idx)		{return (ObjectRef)-(idx+1);}

enum ScopeKeyType {SearchScope,ValueScope,ObjectScope};

struct ScopeKey
	:public Scope
{
	unsigned char prio;

	ScopeKey( ScopeKeyType type_, int from_, int to_)
		:Scope(from_,to_),prio(type_){}
	ScopeKey( ScopeKeyType type_, const Scope& scope_)
		:Scope(scope_),prio(type_){}
	ScopeKey( const ScopeKey& o)
		:Scope(o),prio(o.prio){}

	static inline ScopeKey search( int from_)
	{
		return ScopeKey( SearchScope, from_, std::numeric_limits<int>::max());
	}
	const Scope& scope() const
	{
		return *this;
	}

	bool operator<( const ScopeKey& o) const
	{
		if (from == o.from)
		{
			if (to == o.to)
			{
				return prio < o.prio;
			}
			else
			{
				return (to > o.to);
			}
		}
		else
		{
			return (from < o.from);
		}
	}
	bool operator==( const ScopeKey& o) const
	{
		return from == o.from && to == o.to && prio == o.prio;
	}
	bool operator!=( const ScopeKey& o) const
	{
		return from != o.from || to != o.to || prio != o.prio;
	}
};

struct ObjectDescr
{
	ObjectRef objref;
	int taglevel;

	ObjectDescr( const ObjectRef& objref_, int taglevel_)
		:objref(objref_),taglevel(taglevel_){}
	ObjectDescr( const ObjectDescr& o)
		:objref(o.objref),taglevel(o.taglevel){}
};

struct Value
{
	papuga_ValueVariant content;

	explicit Value()
	{
		papuga_init_ValueVariant( &content);
	}
	Value( const papuga_ValueVariant* content_)
	{
		papuga_init_ValueVariant_value( &content, content_);
	}
	Value( const char* str, int size)
	{
		papuga_init_ValueVariant_string( &content, str, size);
	}
	Value( const Value& o)
	{
		papuga_init_ValueVariant_value( &content, &o.content);
	}
	std::string tostring() const
	{
		papuga_ErrorCode errcode = papuga_Ok;
		return ValueVariant_tostring( content, errcode);
	}
};

struct ValueNode
{
	papuga_ValueVariant value;
	int itemid;

	ValueNode( int itemid_, const papuga_ValueVariant* value_)
		:itemid(itemid_)
	{
		papuga_init_ValueVariant_value( &value, value_);
	}
	ValueNode( const ValueNode& o)
		:itemid(o.itemid)
	{
		papuga_init_ValueVariant_value( &value, &o.value);
	}
	std::string tostring() const
	{
		papuga_ErrorCode errcode = papuga_Ok;
		return ValueVariant_tostring( value, errcode);
	}
};

struct MethodCallKey
{
	int group;
	int scope;
	int elemidx;

	MethodCallKey( int group_, int scope_, int elemidx_)
		:group(group_),scope(scope_),elemidx(elemidx_)
	{}
	MethodCallKey( const MethodCallKey& o)
		:group(o.group),scope(o.scope),elemidx(o.elemidx)
	{}
	bool operator < (const MethodCallKey& o) const	{return compare(o) < 0;}
	bool operator <= (const MethodCallKey& o) const	{return compare(o) <= 0;}
	bool operator >= (const MethodCallKey& o) const	{return compare(o) >= 0;}
	bool operator > (const MethodCallKey& o) const	{return compare(o) > 0;}
	bool operator == (const MethodCallKey& o) const	{return compare(o) == 0;}
	bool operator != (const MethodCallKey& o) const	{return compare(o) != 0;}

private:
	int compare( const MethodCallKey& o) const
	{
		if (group != o.group) return (group < o.group) ? -1 : +1;
		if (scope != o.scope) return (scope < o.scope) ? -1 : +1;
		if (elemidx != o.elemidx) return (elemidx < o.elemidx) ? -1 : +1;
		return 0;
	}
};

struct MethodCallNode
	:public MethodCallKey
{
	const CallDef* def;
	Scope scope;
	int taglevel;

	MethodCallNode( const CallDef* def_, const Scope& scope_, int taglevel_, const MethodCallKey& key_)
		:MethodCallKey(key_),def(def_),scope(scope_),taglevel(taglevel_){}
	MethodCallNode( const MethodCallNode& o)
		:MethodCallKey(o),def(o.def),scope(o.scope),taglevel(o.taglevel){}
};

static void papuga_init_RequestMethodCall( papuga_RequestMethodCall* self)
{
	self->selfvarname = 0;
	self->resultvarname = 0;
	self->methodid.classid = -1;
	self->methodid.functionid  = -1;
	papuga_init_CallArgs( &self->args, self->membuf, sizeof(self->membuf));
}

class EventStack
{
public:
	EventStack()
	{
		papuga_init_Stack( &m_stk, sizeof(int)/*element size*/, 256/*node size*/, &m_mem, sizeof(m_mem));
	}
	~EventStack()
	{
		papuga_destroy_Stack( &m_stk);
	}
	void push( int ev)
	{
		int* elemptr = (int*)papuga_Stack_push( &m_stk);
		if (!elemptr) throw std::bad_alloc();
		*elemptr = ev;
	}
	int pop()
	{
		int* elemptr = (int*)papuga_Stack_pop( &m_stk);
		if (!elemptr) return 0;
		return *elemptr;
	}

private:
	papuga_Stack m_stk;
	int m_mem[ 256];
};

/* \brief Abstraction for building recursive structures */
class ValueSink
{
public:
	explicit ValueSink( papuga_Serialization* ser_)
		:allocator(ser_->allocator),ser(ser_),val(0),name(0),opentags(0){}
	ValueSink( papuga_ValueVariant* val_, papuga_Allocator* allocator_)
		:allocator(allocator_),ser(0),val(val_),name(0),opentags(0){}

	bool openSerialization()
	{
		if (ser)
		{
			if (name)
			{
				if (!papuga_Serialization_pushName_charp( ser, name)) return false;
				name = NULL;
			}
			if (!papuga_Serialization_pushOpen( ser)) return false;
			++opentags;
		}
		else
		{
			if (!val || papuga_ValueVariant_defined( val)) return false;
			ser = papuga_Allocator_alloc_Serialization( allocator);
			if (!ser) return false;
			papuga_init_ValueVariant_serialization( val, ser);
		}
		return true;
	}

	bool closeSerialization()
	{
		if (ser && opentags)
		{
			--opentags;
			return papuga_Serialization_pushClose( ser);
		}
		else
		{
			return true;
		}
	}

	bool pushName( const char* name_)
	{
		if (name) return false;
		name = name_;
		return true;
	}

	bool pushValue( const papuga_ValueVariant* val_)
	{
		if (ser)
		{
			if (name)
			{
				if (papuga_ValueVariant_defined( val_))
				{
					if (!papuga_Serialization_pushName_charp( ser, name)) return false;
					if (!papuga_Serialization_pushValue( ser, val_)) return false;
				}
				name = NULL;
				return true;
			}
			else
			{
				return papuga_Serialization_pushValue( ser, val_);
			}
		}
		else if (val)
		{
			if (name || papuga_ValueVariant_defined( val)) return false;
			papuga_init_ValueVariant_value( val, val_);
			return true;
		}
		else
		{
			return false;
		}
	}

	bool pushVoid()
	{
		papuga_ValueVariant voidelem;
		papuga_init_ValueVariant( &voidelem);
		return pushValue( &voidelem);
	}

private:
	papuga_Allocator* allocator;
	papuga_Serialization* ser;
	papuga_ValueVariant* val;
	const char* name;
	int opentags;
};

typedef std::pair<ScopeKey,ObjectDescr> ScopeObjElem;
typedef std::multimap<ScopeKey,ObjectDescr> ScopeObjMap;
typedef ScopeObjMap::const_iterator ScopeObjItr;

class AutomatonContext
{
public:
	AutomatonContext( const AutomatonDescription* atm_, papuga_RequestLogger* logger)
		:m_atm(atm_),m_logContentEvent(0),m_loggerSelf(0)
		,m_atmstate(&atm_->atm()),m_scopecnt(0),m_scopestack()
		,m_valuenodes(),m_values(),m_structs(),m_scopeobjmap( atm_->maxitemid()+1, ScopeObjMap())
		,m_methodcalls(),m_rootelements()
		,m_results( new RequestResultTemplate[ atm_->resultdefs().size()])
		,m_maskOfRequiredInheritedContexts(getRequiredInheritedContextsMask(atm_)),m_nofInheritedContexts(0)
		,m_done(false),m_errcode(papuga_Ok),m_erritemid(-1)
	{
		if (logger && logger->logContentEvent && logger->self)
		{
			m_loggerSelf = logger->self;
			m_logContentEvent = logger->logContentEvent;
		}
		if (m_atm->inheritdefs().size() >= MaxNofInheritedContexts)
		{
			throw std::bad_alloc();
		}
		m_inheritedContexts[ 0].type = 0;
		m_inheritedContexts[ 0].name = 0;
		papuga_init_Allocator( &m_allocator, m_allocator_membuf, sizeof(m_allocator_membuf));
		m_scopestack.reserve( 32);
		m_scopestack.push_back( 0);

		std::size_t ri = 0, re = atm_->resultdefs().size();
		for (; ri != re; ++ri)
		{
			m_results[ ri].setName( atm_->resultdefs()[ ri]->name);
			m_results[ ri].setTarget( atm_->resultdefs()[ ri]->schema, atm_->resultdefs()[ ri]->requestmethod, atm_->resultdefs()[ ri]->addressvar);
		}
	}
	~AutomatonContext()
	{
		delete [] m_results;
		papuga_destroy_Allocator( &m_allocator);
	}

	papuga_ErrorCode lastError() const
	{
		return m_errcode;
	}

	int lastErrorItemId() const
	{
		return m_erritemid;
	}

	bool processEvents( const textwolf::XMLScannerBase::ElementType tp, const papuga_ValueVariant* value, const char* valuestr, size_t valuelen)
	{
		AutomatonState::iterator itr = m_atmstate.push( tp, valuestr, valuelen);
		for (*itr; *itr; ++itr)
		{
			int ev = *itr;
			m_event_stacks[ AtmRef_type(ev)].push( ev);
		}
		// Ensure that events are issued in the order InstantiateValue,CollectValue,CloseStruct and MethodCall:
		int ei = 0, ee = MaxAtmRefType+1;
		for (;ei < ee; ++ei)
		{
			for (int ev=m_event_stacks[ei].pop(); ev; ev=m_event_stacks[ei].pop())
			{
				if (!processEvent( ev, value)) return false;
			}
		}
		return true;
	}

	bool pushValueAndProcessEvents( const textwolf::XMLScannerBase::ElementType tp, const papuga_ValueVariant* value)
	{
		char localbuf[ 1024];
		size_t valuelen;
		const char* valuestr = (const char*)papuga_ValueVariant_tostring_enc( value, papuga_UTF8, localbuf, sizeof(localbuf)-1, &valuelen, &m_errcode);
		if (!valuestr) return false;
		localbuf[ valuelen] = 0;	//... textwolf needs null termination

		return processEvents( tp, value, valuestr, valuelen);
	}

	bool pushEmptyAndProcessEvents( const textwolf::XMLScannerBase::ElementType tp, const papuga_ValueVariant* value)
	{
		return processEvents( tp, value, "", 0);
	}

	bool processOpenTag( const papuga_ValueVariant* tagname)
	{
		try
		{
			++m_scopecnt;
			if (m_logContentEvent) m_logContentEvent( m_loggerSelf, "open tag", -1/*itemid*/, tagname);
			if (m_scopestack.size() <= 1 && papuga_ValueVariant_isstring(tagname))
			{
				std::string elem = papuga::ValueVariant_tostring( *tagname, m_errcode);
				if (elem.empty())
				{
					if (m_errcode == papuga_Ok) m_errcode = papuga_SyntaxError;
					return false;
				}
				m_rootelements.insert( elem);
			}
			m_scopestack.push_back( m_scopecnt);
			if (!pushValueAndProcessEvents( textwolf::XMLScannerBase::OpenTag, tagname)) return false;
			return true;
		}
		CATCH_LOCAL_EXCEPTION(m_errcode,false)
	}

	bool processAttributeName( const papuga_ValueVariant* attrname)
	{
		try
		{
			++m_scopecnt;
			if (m_logContentEvent) m_logContentEvent( m_loggerSelf, "attribute name", -1/*itemid*/, attrname);
			if (!pushValueAndProcessEvents( textwolf::XMLScannerBase::TagAttribName, attrname)) return false;
			return true;
		}
		CATCH_LOCAL_EXCEPTION(m_errcode,false)
	}
	bool processAttributeValue( const papuga_ValueVariant* value)
	{
		try
		{
			++m_scopecnt;
			if (m_logContentEvent) m_logContentEvent( m_loggerSelf, "attribute value", -1/*itemid*/, value);
			if (!pushValueAndProcessEvents( textwolf::XMLScannerBase::TagAttribValue, value)) return false;
			return true;
		}
		CATCH_LOCAL_EXCEPTION(m_errcode,false)
	}

	bool processValue( const papuga_ValueVariant* value)
	{
		try
		{
			++m_scopecnt;
			if (m_logContentEvent) m_logContentEvent( m_loggerSelf, "content value", -1/*itemid*/, value);
			if (!pushEmptyAndProcessEvents( textwolf::XMLScannerBase::Content, value)) return false;
			return true;
		}
		CATCH_LOCAL_EXCEPTION(m_errcode,false)
	}

	bool processCloseTag()
	{
		static const Value empty;
		try
		{
			++m_scopecnt;
			if (m_logContentEvent) m_logContentEvent( m_loggerSelf, "close tag", -1/*itemid*/, NULL/*value*/);
			if (!pushEmptyAndProcessEvents( textwolf::XMLScannerBase::CloseTag, &empty.content)) return false;
			m_scopestack.pop_back();
			return true;
		}
		CATCH_LOCAL_EXCEPTION(m_errcode,false)
	}
	bool done()
	{
		try
		{
			if (m_done) return true;
			++m_scopecnt;
			// Order method calls according grouping and order of occurrence:
			std::sort( m_methodcalls.begin(), m_methodcalls.end());
			// Initialize list of all root elements:
			if (!m_atm->isValidRootTagSet( m_rootelements))
			{
				m_errcode = papuga_InvalidRequest;
				return false;
			}
			// End finalize:
			return m_done = true;
		}
		CATCH_LOCAL_EXCEPTION(m_errcode,false)
	}
	const MethodCallNode* methodCallNode( int idx) const
	{
		return (idx >= (int)m_methodcalls.size()) ? NULL : &m_methodcalls[ idx];
	}
	const std::vector<Value>& values() const
	{
		return m_values;
	}
	const std::vector<const StructDef*>& structs() const
	{
		return m_structs;
	}
	const std::vector<ScopeObjMap>& scopeobjmap() const
	{
		return m_scopeobjmap;
	}
	void insertObjectRef( int itemid, const ScopeKeyType& type, const Scope& scope, int taglevel_, const ObjectRef& objref)
	{
		m_scopeobjmap[ itemid].insert( ScopeObjElem( ScopeKey(type,scope), ObjectDescr( objref, taglevel_)));
	}
	Scope curscope() const
	{
		return Scope( m_scopestack.back(), m_scopecnt);
	}
	int taglevel() const
	{
		return m_scopestack.size();
	}
	bool isDone() const
	{
		return m_done;
	}
	const papuga_ClassDef* classdefs() const
	{
		return m_atm->classdefs();
	}
	std::string methodName( const papuga_RequestMethodId& mid) const
	{
		const papuga_ClassDef* cd = m_atm->classdefs();
		if (!mid.classid) return std::string();
		const char* classname = cd[ mid.classid-1].name;
		char buf[ 128];
		if (mid.functionid)
		{
			std::snprintf( buf, sizeof(buf), "%s::%s", classname, cd[ mid.classid-1].methodnames[ mid.functionid-1]);
		}
		else
		{
			std::snprintf( buf, sizeof(buf), "constructor %s", classname);
		}
		buf[ sizeof(buf)-1] = 0;
		return std::string( buf);
	}
	const papuga_RequestInheritedContextDef* getRequiredInheritedContextsDefs( papuga_ErrorCode& errcode) const
	{
		if (m_maskOfRequiredInheritedContexts)
		{
			errcode = papuga_ValueUndefined;
			return NULL;
		}
		return m_inheritedContexts;
	}

	RequestResultTemplate* results() const
	{
		return m_results;
	}
	std::size_t nofResults() const
	{
		return m_atm->resultdefs().size();
	}

	bool isResultVariable( const char* name) const
	{
		return m_atm->isResultVariable( name);
	}

	papuga_RequestEventLoggerProcedure logContentEvent() const
	{
		return m_logContentEvent;
	}
	void* loggerSelf() const
	{
		return m_loggerSelf;
	}

private:
	bool processEvent( int ev, const papuga_ValueVariant* evalue)
	{
		// Process event depending on type:
		int evidx = AtmRef_index( ev);
		switch (AtmRef_type( ev))
		{
			case InheritFrom:
			{
				pushInheritedContext( evidx, evalue);
				break;
			}
			case InstantiateValue:
			{
				papuga_ValueVariant evalue_copy;
				//PF:HACK: The const cast has no influence, as movehostobj parameter is false and the contents of evalue remain
				//	untouched, but it is still ugly and a bad hack:
				if (!papuga_Allocator_deepcopy_value( &m_allocator, &evalue_copy, const_cast<papuga_ValueVariant*>(evalue), false, &m_errcode)) return false;
				int itemid = m_atm->valuedefs()[ evidx].itemid;
				if (m_logContentEvent) m_logContentEvent( m_loggerSelf, "instantiate", itemid, &evalue_copy);
				m_valuenodes.push_back( ValueNode( itemid, &evalue_copy));
				break;
			}
			case CollectValue:
			{
				int itemid = m_atm->valuedefs()[ evidx].itemid;
				std::vector<ValueNode>::iterator vi = m_valuenodes.begin();
				int valuecnt = 0;
				std::size_t vidx = 0;
				while (vidx != m_valuenodes.size())
				{
					if (vi->itemid == itemid)
					{
						int objref = ObjectRef_value( m_values.size());
						insertObjectRef( itemid, ValueScope, curscope(), taglevel(), objref);
						m_values.push_back( Value( &vi->value));
						if (m_logContentEvent) m_logContentEvent( m_loggerSelf, "collect", itemid, &vi->value);
						m_valuenodes.erase( vi);
						vi = m_valuenodes.begin() + vidx;
						++valuecnt;
					}
					else
					{
						++vi;
						++vidx;
					}
				}
				if (valuecnt == 0)
				{
					int objref = ObjectRef_value( m_values.size());
					insertObjectRef( itemid, ValueScope, curscope(), taglevel(), objref);
					m_values.push_back( Value());
				}
				else if (valuecnt > 1)
				{
					m_erritemid = itemid;
					m_errcode = papuga_DuplicateDefinition;
					return false;
				}
				break;
			}
			case CloseStruct:
			{
				const StructDef* stdef = &m_atm->structdefs()[ evidx];
				insertObjectRef( stdef->itemid, ObjectScope, curscope(), taglevel(), ObjectRef_struct( m_structs.size()));
				m_structs.push_back( stdef);
				if (m_logContentEvent) m_logContentEvent( m_loggerSelf, "struct", stdef->itemid, 0/*value*/);
				break;
			}
			case MethodCallPrioritize:
			{
				const CallDef* calldef = &m_atm->calldefs()[ evidx];
				std::size_t mi = m_methodcalls.size();
				Scope cscope = curscope();
				bool havePrioritizedItem = false;
				for (; mi > 0 && m_methodcalls[mi-1].scope.from >= cscope.from; --mi)
				{
					const MethodCallNode& mc = m_methodcalls[mi-1];
					if (mc.scope.inside( cscope) && calldef == mc.def)
					{
						havePrioritizedItem = true;
						break;
					}
				}
				if (havePrioritizedItem)
				{
					mi = m_methodcalls.size();
					for (; mi > 0 && m_methodcalls[mi-1].scope.from >= cscope.from; --mi)
					{
						const MethodCallNode& mc = m_methodcalls[mi-1];
						if (mc.scope.inside( cscope)
							&& calldef != mc.def
							&& mc.def->resultvarname
							&& calldef->resultvarname
							&& 0==std::strcmp(mc.def->resultvarname,calldef->resultvarname))
						{
							if (m_logContentEvent)
							{
								std::string methodname = methodName( mc.def->methodid);
								if (methodname.empty())
								{
									papuga_ValueVariant vv;
									papuga_init_ValueVariant_charp( &vv, mc.def->resultvarname);
									m_logContentEvent( m_loggerSelf, "suppress assignment", -1/*/no itemid*/, &vv);
								}
								else
								{
									papuga_ValueVariant vv;
									papuga_init_ValueVariant_string( &vv, methodname.c_str(), methodname.size());
									m_logContentEvent( m_loggerSelf, "suppress call", -1/*/no itemid*/, &vv);
								}
							}
							m_methodcalls.erase( m_methodcalls.begin()+(mi-1));
						}
					}
				}
				break;
			}
			case MethodCall:
			{
				const CallDef* calldef = &m_atm->calldefs()[ evidx];
				MethodCallKey key( calldef->groupid, m_scopecnt, evidx);
				m_methodcalls.push_back( MethodCallNode( calldef, curscope(), taglevel(), key));
				if (m_done)
				{
					m_errcode = papuga_ExecutionOrder;
					return false;
				}
				break;
			}
			case ResultInstruction:
			{
				std::size_t resultidx = ResRef_resultidx( evidx);
				int instridx = ResRef_instridx( evidx);
				if (resultidx >= m_atm->resultdefs().size())
				{
					m_errcode = papuga_LogicError;
					return false;
				}
				const papuga_RequestResultDescription* resultdef = m_atm->resultdefs()[ resultidx];
				if (instridx >= resultdef->nodearsize)
				{
					m_errcode = papuga_LogicError;
					return false;
				}
				const papuga_RequestResultNodeDescription& node = resultdef->nodear[ instridx];
				switch (node.type)
				{
					case papuga_ResultNodeConstant:
						m_results[ resultidx].addResultNodeConstant( node.tagname, node.value.str);
						break;
					case papuga_ResultNodeOpenStructure:
						m_results[ resultidx].addResultNodeOpenStructure( node.tagname, false);
						break;
					case papuga_ResultNodeCloseStructure:
						m_results[ resultidx].addResultNodeCloseStructure( node.tagname, false);
						break;
					case papuga_ResultNodeOpenArray:
						m_results[ resultidx].addResultNodeOpenStructure( node.tagname, true);
						break;
					case papuga_ResultNodeCloseArray:
						m_results[ resultidx].addResultNodeCloseStructure( node.tagname, true);
						break;
					case papuga_ResultNodeInputReference:
						m_results[ resultidx].addResultNodeInputReference( curscope(), node.tagname, node.value.itemid, node.resolvetype, taglevel());
						break;
					case papuga_ResultNodeResultReference:
						m_results[ resultidx].addResultNodeResultReference( curscope(), node.tagname, node.value.str, node.resolvetype);
						break;
				}
				break;
			}
		}
		return true;
	}

	struct EventId
	{
		int scopecnt;
		int id;

		EventId()
			:scopecnt(0),id(0){}
		EventId( int scopecnt_, int id_)
			:scopecnt(scopecnt_),id(id_){}
		EventId( const EventId& o)
			:scopecnt(o.scopecnt),id(o.id){}

		bool operator==( const EventId& o) const
		{
			return id == o.id && scopecnt == o.scopecnt;
		}
	};

	static int getRequiredInheritedContextsMask( const AutomatonDescription* atm)
	{
		int rt = 0;
		std::vector<AutomatonDescription::InheritFromDef>::const_iterator hi = atm->inheritdefs().begin(), he = atm->inheritdefs().end();
		for (int hidx=0; hi != he; ++hi,++hidx)
		{
			if (hi->required)
			{
				rt |= (1 << hidx);
			}
		}
		return rt;
	}

	bool pushInheritedContext( int idx, const papuga_ValueVariant* value)
	{
		if (m_nofInheritedContexts >= MaxNofInheritedContexts)
		{
			m_errcode = papuga_BufferOverflowError;
			return false;
		}
		if (idx >= (int)m_atm->inheritdefs().size())
		{
			m_errcode = papuga_LogicError;
			return false;
		}
		if (!papuga_ValueVariant_isstring( value))
		{
			m_errcode = papuga_TypeError;
			return false;
		}
		char* contextname = papuga_Allocator_copy_string( &m_allocator, value->value.string, value->length);
		if (!contextname)
		{
			m_errcode = papuga_NoMemError;
			return false;
		}
		const AutomatonDescription::InheritFromDef& idef = m_atm->inheritdefs()[ idx];
		m_inheritedContexts[ m_nofInheritedContexts].type = idef.type.c_str();
		m_inheritedContexts[ m_nofInheritedContexts].name = contextname;
		if (idef.required)
		{
			m_maskOfRequiredInheritedContexts &= ~(1 << idx);
		}
		++m_nofInheritedContexts;
		m_inheritedContexts[ m_nofInheritedContexts].type = NULL;
		m_inheritedContexts[ m_nofInheritedContexts].name = NULL;
		return true;
	}

private:
	typedef textwolf::XMLPathSelect<textwolf::charset::UTF8> AutomatonState;

	const AutomatonDescription* m_atm;
	papuga_RequestEventLoggerProcedure m_logContentEvent;
	void* m_loggerSelf;
	AutomatonState m_atmstate;
	papuga_Allocator m_allocator;
	char m_allocator_membuf[ 4096];
	int m_scopecnt;
	std::vector<int> m_scopestack;
	std::vector<ValueNode> m_valuenodes;
	std::vector<Value> m_values;
	std::vector<const StructDef*> m_structs;
	std::vector<ScopeObjMap> m_scopeobjmap;
	std::vector<MethodCallNode> m_methodcalls;
	std::set<std::string> m_rootelements;
	RequestResultTemplate* m_results;
	enum {MaxNofInheritedContexts=31};
	int m_maskOfRequiredInheritedContexts;
	int m_nofInheritedContexts;
	papuga_RequestInheritedContextDef m_inheritedContexts[ MaxNofInheritedContexts+1];
	bool m_done;
	papuga_ErrorCode m_errcode;
	int m_erritemid;
	EventStack m_event_stacks[ MaxAtmRefType+1];
};

class RequestIterator
{
#if __cplusplus >= 201103L
	RequestIterator( const RequestIterator&) = delete;	//... non copyable
	void operator=( const RequestIterator&) = delete;	//... non copyable
#endif
public:
	explicit RequestIterator( const AutomatonContext* ctx_)
		:m_ctx(ctx_)
		,m_logContentEvent(ctx_->logContentEvent())
		,m_loggerSelf(ctx_->loggerSelf())
		,m_resolvers(ctx_->scopeobjmap().size(),ScopeObjItr())
		,m_curr_methodidx(0)
		,m_structpath()
	{
		papuga_init_RequestError( &m_errstruct);
		papuga_init_Allocator( &m_allocator, m_allocator_membuf, sizeof(m_allocator_membuf));
		papuga_init_RequestMethodCall( &m_curr_methodcall);
		std::vector<ScopeObjMap>::const_iterator mi = ctx_->scopeobjmap().begin(), me = ctx_->scopeobjmap().end();
		for (int midx=0; mi != me; ++mi,++midx)
		{
			m_resolvers[ midx] = mi->end();
		}
	}

	~RequestIterator()
	{
		papuga_destroy_Allocator( &m_allocator);
	}

	void assignErrPath()
	{
		std::string errpathstr;
		std::vector<std::string>::const_iterator ei = m_structpath.begin(), ee = m_structpath.end();
		for (; ei != ee; ++ei)
		{
			if (!errpathstr.empty()) errpathstr.push_back('/');
			errpathstr.append( *ei);
		}
		if (!m_structpath.empty())
		{
			std::size_t structpathlen = errpathstr.size() >= sizeof(m_errstruct.structpath) ? (sizeof(m_errstruct.structpath)-1) : errpathstr.size();
			std::memcpy( m_errstruct.structpath, errpathstr.c_str(), structpathlen);
			m_errstruct.structpath[ structpathlen] = 0;
		}
	}
	void assignErrMethod( const papuga_RequestMethodId& mid)
	{
		if (mid.classid)
		{
			const papuga_ClassDef* cd = m_ctx->classdefs();
			m_errstruct.classname = cd[ mid.classid-1].name;
			if (mid.functionid)
			{
				m_errstruct.methodname = cd[ mid.classid-1].methodnames[ mid.functionid-1];
			}
			else
			{
				m_errstruct.methodname = 0;
			}
		}
		else
		{
			m_errstruct.classname = 0;
			m_errstruct.methodname = 0;
		}
	}

	papuga_RequestMethodCall* nextCall( const papuga_RequestContext* context)
	{
		try
		{
			if (!m_ctx) return NULL;
			const MethodCallNode* mcnode = m_ctx->methodCallNode( m_curr_methodidx);
			if (!mcnode)
			{
				std::memset( &m_curr_methodcall, 0, sizeof(m_curr_methodcall));
				return NULL;
			}
			m_curr_methodcall.selfvarname = mcnode->def->selfvarname;
			m_curr_methodcall.resultvarname = mcnode->def->resultvarname;
			m_curr_methodcall.methodid.classid = mcnode->def->methodid.classid;
			m_curr_methodcall.methodid.functionid = mcnode->def->methodid.functionid;

			m_errstruct.scopestart = mcnode->scope.from;
			papuga_init_CallArgs( &m_curr_methodcall.args, m_curr_methodcall.membuf, sizeof(m_curr_methodcall.membuf));

			const CallDef* mcdef = mcnode->def;
			papuga_CallArgs* args = &m_curr_methodcall.args;
			int ai = 0, ae = mcdef->nofargs;
			for (; ai != ae; ++ai)
			{
				papuga_init_ValueVariant( args->argv + ai);
				if (!setCallArgValue( args->argv[ai], mcdef->args[ai], mcnode->scope, mcnode->taglevel, context))
				{
					assignErrPath();
					m_errstruct.argcnt = ai;
					assignErrMethod( mcdef->methodid);
					return NULL;
				}
			}
			for (; ai && !papuga_ValueVariant_defined( args->argv+(ai-1)); --ai){}
			/// ... remove optional arguments at the end of the argument list, to make them replaceable by default values

			args->argc = ai;
			++m_curr_methodidx;
			return &m_curr_methodcall;
		}
		catch (const std::bad_alloc&)
		{
			m_errstruct.errcode = papuga_NoMemError;
			return NULL;
		}
		catch (...)
		{
			m_errstruct.errcode = papuga_UncaughtException;
			return NULL;
		}
	}

	bool pushCallResult( const papuga_ValueVariant& result)
	{
		if (!m_curr_methodcall.resultvarname)
		{
			m_errstruct.errcode = papuga_ValueUndefined;
			return false;
		}
		bool used = false;
		if (m_curr_methodidx <= 0)
		{
			m_errstruct.errcode = papuga_ExecutionOrder;
			return false;
		}
		try
		{
			std::size_t ri = 0, re = m_ctx->nofResults();
			for (; ri != re; ++ri)
			{
				const MethodCallNode* mcnode = m_ctx->methodCallNode( m_curr_methodidx-1);
				if (!mcnode)
				{
					m_errstruct.errcode = papuga_ExecutionOrder;
					return false;
				}
				used |= m_ctx->results()[ ri].pushResult( m_curr_methodcall.resultvarname, mcnode->scope, result, m_errstruct.errcode);
			}
		}
		catch (const std::bad_alloc&)
		{
			m_errstruct.errcode = papuga_NoMemError;
			return false;
		}
		catch (...)
		{
			m_errstruct.errcode = papuga_UncaughtException;
			return false;
		}
		return used;
	}

	papuga_RequestResult* getResultArray( papuga_Allocator* allocator, int& nofResults)
	{
		int ri = 0, re = m_ctx->nofResults();
		nofResults = 0;
		for (; ri < re; ++ri)
		{
			if (!m_ctx->results()[ ri].empty()) ++nofResults;
		}
		if (nofResults == 0) return NULL;
		papuga_RequestResult* rt = (papuga_RequestResult*)papuga_Allocator_alloc( allocator, nofResults * sizeof(papuga_RequestResult), 0/*sizeof(non empty struct)*/);
		if (rt == NULL)
		{
			m_errstruct.errcode = papuga_NoMemError;
			return false;
		}
		ri = 0;
		int ridx = 0;
		for (; ri < re; ++ri)
		{
			if (!m_ctx->results()[ ri].empty())
			{
				if (!initResult( rt+ridx, allocator, ridx)) return false;
				++ridx;
			}
		}
		return rt;
	}

	bool initResult( papuga_RequestResult* result, papuga_Allocator* allocator, int idx)
	{
		try
		{
			bool rt = true;
			if (idx >= (int)m_ctx->nofResults())
			{
				m_errstruct.errcode = papuga_OutOfRangeError;
				return false;
			}
			result->name = m_ctx->results()[ idx].name();
			result->schema = m_ctx->results()[ idx].schema();
			result->requestmethod = m_ctx->results()[ idx].requestmethod();
			result->addressvar = m_ctx->results()[ idx].addressvar();
			papuga_init_Serialization( &result->serialization, allocator);

			const char* unresolvedVar = m_ctx->results()[ idx].findUnresolvedResultVariable();
			if (unresolvedVar)
			{
				m_errstruct.variable = unresolvedVar;
				m_errstruct.errcode = papuga_ValueUndefined;
				return false;
			}
			std::vector<RequestResultInputElementRef> irefs = m_ctx->results()[ idx].inputElementRefs();
			{
				std::vector<RequestResultInputElementRef>::iterator ri = irefs.begin(), re = irefs.end();
				for (; ri != re; ++ri)
				{
					TagLevelRange tagLevelRange = getTagLevelRange( ri->resolvetype, ri->taglevel, 1/*max_tag_diff*/);
					ValueSink sink( ri->value, allocator);
					if (!resolveItem( sink, ri->itemid, ri->resolvetype, ri->scope, tagLevelRange, false/*embedded*/))
					{
						if (m_errstruct.scopestart < ri->scope.from)
						{
							m_errstruct.scopestart = ri->scope.from;
						}
						return false;
					}
				}
			}{
				std::vector<bool> structStack;
				std::vector<RequestResultItem>::const_iterator
					ri = m_ctx->results()[ idx].items().begin(),
					re = m_ctx->results()[ idx].items().end();
				for (; ri != re; ++ri)
				{
					switch (ri->nodetype)
					{
						case papuga_ResultNodeConstant:
							if (ri->tagname) rt &= papuga_Serialization_pushName_charp( &result->serialization, ri->tagname);
							rt &= papuga_Serialization_pushValue( &result->serialization, &ri->value);
							break;
						case papuga_ResultNodeOpenStructure:
							rt &= papuga_Serialization_pushName_charp( &result->serialization, ri->tagname);
							rt &= papuga_Serialization_pushOpen( &result->serialization);
							break;
						case papuga_ResultNodeOpenArray:
						{
							std::vector<RequestResultItem>::const_iterator next = ri;
							++next;
							bool valuelist = next != re && next->tagname == NULL && next->nodetype != papuga_ResultNodeCloseArray;

							rt &= papuga_Serialization_pushName_charp( &result->serialization, ri->tagname);
							rt &= papuga_Serialization_pushOpen( &result->serialization);
							if (valuelist)
							{
								structStack.push_back( false);
							}
							else
							{
								structStack.push_back( true);
								rt &= papuga_Serialization_pushOpen( &result->serialization);
							}
							break;
						}
						case papuga_ResultNodeCloseStructure:
							rt &= papuga_Serialization_pushClose( &result->serialization);
							break;
						case papuga_ResultNodeCloseArray:
						{
							std::vector<RequestResultItem>::const_iterator next = ri;
							++next;
							bool reopen = (next != re && next->nodetype == papuga_ResultNodeOpenArray && 0==std::strcmp( ri->tagname, next->tagname));
							if (reopen)
							{
								++ri;
								if (structStack.back())
								{
									rt &= papuga_Serialization_pushClose( &result->serialization);
									rt &= papuga_Serialization_pushOpen( &result->serialization);
								}
							}
							else
							{
								rt &= papuga_Serialization_pushClose( &result->serialization);
								if (structStack.back())
								{
									rt &= papuga_Serialization_pushClose( &result->serialization);
								}
								structStack.pop_back();
							}
							break;
						}
						case papuga_ResultNodeInputReference:
						case papuga_ResultNodeResultReference:
						{
							if (papuga_ValueVariant_defined( &ri->value))
							{
								if (ri->tagname) rt &= papuga_Serialization_pushName_charp( &result->serialization, ri->tagname);
								papuga_ValueVariant valuecopy;
								//PF:HACK: The const cast has no influence, as movehostobj parameter is false and the contents of the
								//	source value remain untouched, but it is still ugly and a bad hack:
								papuga_ValueVariant* source = const_cast<papuga_ValueVariant*>(&ri->value);
								rt &= papuga_Allocator_deepcopy_value( allocator, &valuecopy, source, false/*movehostobj*/, &m_errstruct.errcode);
								rt &= papuga_Serialization_pushValue( &result->serialization, &valuecopy);
							}
							break;
						}
					}
				}
			}
			if (!rt && m_errstruct.errcode == papuga_Ok)
			{
				m_errstruct.errcode = papuga_NoMemError;
			}
			return rt;
		}
		catch (const std::bad_alloc&)
		{
			m_errstruct.errcode = papuga_NoMemError;
			return false;
		}
		catch (...)
		{
			m_errstruct.errcode = papuga_UncaughtException;
			return false;
		}
	}

	const papuga_RequestError* lastError() const
	{
		return m_errstruct.errcode == papuga_Ok ? NULL : &m_errstruct;
	}
	const papuga_RequestMethodCall* lastCall() const
	{
		return m_ctx->methodCallNode( m_curr_methodidx) ? &m_curr_methodcall : NULL;
	}

private:
	typedef std::pair<int,int> TagLevelRange;
	TagLevelRange getTagLevelRange( papuga_ResolveType resolvetype, int taglevel, int tagdiff)
	{
		if (tagdiff >= 0)
		{
			if (resolvetype == papuga_ResolveTypeInherited)
			{
				return std::pair<int,int>( taglevel - tagdiff, taglevel);
			}
			else
			{
				return std::pair<int,int>( taglevel, taglevel + tagdiff);
			}
		}
		else
		{
			return std::pair<int,int>( 0, std::numeric_limits<int>::max());
		}
	}

	struct ResolvedObject
	{
		ObjectRef objref;
		int taglevel;
		Scope scope;

		ResolvedObject()
			:objref(0),taglevel(0),scope(0,0){}
		ResolvedObject( const ObjectRef& objref_, int taglevel_, const Scope& scope_)
			:objref(objref_),taglevel(taglevel_),scope(scope_){}
		ResolvedObject( const ResolvedObject& o)
			:objref(o.objref),taglevel(o.taglevel),scope(o.scope){}

		bool valid() const
		{
			return ObjectRef_is_defined( objref);
		}
	};

	ResolvedObject resolveNearItemCoveringScope( const Scope& scope, const TagLevelRange& taglevelRange, int itemid)
	{
		// Seek backwards for scope overlapping search scope: 
		const ScopeObjMap& objmap = m_ctx->scopeobjmap()[ itemid];
		ScopeObjMap::const_iterator it = m_resolvers[ itemid];

		if (it == objmap.end())
		{
			if (objmap.empty()) return ResolvedObject();
			--it;
		}
		while (it->first.from > scope.from)
		{
			if (it == objmap.begin()) return ResolvedObject();
			--it;
		}
		if (it->first.to >= scope.to && it->second.taglevel >= taglevelRange.first && it->second.taglevel <= taglevelRange.second)
		{
			return ResolvedObject( it->second.objref, it->second.taglevel, it->first);
		}
		else
		{
			while (it != objmap.begin())
			{
				--it;
				if (it->first.to > scope.from) break;
				if (it->first.to >= scope.to && it->second.taglevel >= taglevelRange.first && it->second.taglevel <= taglevelRange.second)
				{
					m_resolvers[ itemid] = it;
					return ResolvedObject( it->second.objref, it->second.taglevel, it->first);
				}
			}
		}
		return ResolvedObject();
	}

	ResolvedObject resolveNearItemInsideScope( const Scope& scope, const TagLevelRange& taglevelRange, int itemid)
	{
		// Seek forwards for scope overlapped by search scope:
		const ScopeObjMap& objmap = m_ctx->scopeobjmap()[ itemid];
		ScopeObjMap::const_iterator it = m_resolvers[ itemid];

		if (it == objmap.end()) return ResolvedObject();

		if (it->first.from >= scope.from)
		{
			if (it->first.to <= scope.to && it->second.taglevel >= taglevelRange.first && it->second.taglevel <= taglevelRange.second)
			{
				return ResolvedObject( it->second.objref, it->second.taglevel, it->first);
			}
			for (++it; it != objmap.end(); ++it)
			{
				if (it->first.from > scope.to) break;
				if (it->first.to <= scope.to && it->second.taglevel >= taglevelRange.first && it->second.taglevel <= taglevelRange.second)
				{
					m_resolvers[ itemid] = it;
					return ResolvedObject( it->second.objref, it->second.taglevel, it->first);
				}
			}
		}
		return ResolvedObject();
	}

	void setResolverUpperBound( const Scope& scope, int itemid)
	{
		const ScopeObjMap& objmap = m_ctx->scopeobjmap()[ itemid];
		ScopeObjMap::const_iterator it = m_resolvers[ itemid];

		if (it == objmap.end())
		{
			m_resolvers[ itemid] = objmap.lower_bound( ScopeKey::search( scope.from));
		}
		else if (it->first.from < scope.from)
		{
			while (it->first.from < scope.from)
			{
				++it;
				if (it == objmap.end()) break;
			}
			m_resolvers[ itemid] = it;
		}
		else
		{
			while (it->first.from >= scope.from)
			{
				if (it == objmap.begin())
				{
					m_resolvers[ itemid] = it;
					return;
				}
				--it;
			}
			++it;
			m_resolvers[ itemid] = it;
		}
	}

	ResolvedObject resolveNextSameScopeItem( int itemid)
	{
		const ScopeObjMap& objmap = m_ctx->scopeobjmap()[ itemid];
		ScopeObjMap::const_iterator& curitr = m_resolvers[ itemid];
		if (curitr == objmap.end()) return ResolvedObject();
		const Scope& scope = curitr->first;

		++curitr;
		if (curitr == objmap.end() || curitr->first.scope() != scope) return ResolvedObject();
		return ResolvedObject( curitr->second.objref, curitr->second.taglevel, curitr->first);
	}

	ResolvedObject resolveNextInsideItem( const Scope& scope, const TagLevelRange& taglevelRange, int itemid)
	{
		const ScopeObjMap& objmap = m_ctx->scopeobjmap()[ itemid];
		ScopeObjMap::const_iterator& curitr = m_resolvers[ itemid];

		if (curitr == objmap.end()) return ResolvedObject();
		int nextstart = curitr->first.to+1;
		for (;;)
		{
			++curitr;
			if (curitr == objmap.end() || curitr->first.from > scope.to) return ResolvedObject();
			if (curitr->first.from >= nextstart
				&& curitr->second.taglevel >= taglevelRange.first && curitr->second.taglevel <= taglevelRange.second
				&& curitr->first.inside( scope))
			{
				return ResolvedObject( curitr->second.objref, curitr->second.taglevel, curitr->first);
			}
		}
	}

	bool hasNextCoveringItem( const Scope& scope, const TagLevelRange& taglevelRange, int itemid)
	{
		const ScopeObjMap& objmap = m_ctx->scopeobjmap()[ itemid];
		ScopeObjMap::const_iterator& curitr = m_resolvers[ itemid];

		if (curitr == objmap.end()) return false;
		for (;;)
		{
			++curitr;
			if (curitr == objmap.end() || curitr->first.from > scope.from) return false;
			if (curitr->second.taglevel >= taglevelRange.first && curitr->second.taglevel <= taglevelRange.second
				&& scope.inside( curitr->first))
			{
				return true;
			}
		}
	}

	bool build_structure( ValueSink& sink, const Scope& scope, int taglevel, int structidx)
	{
		const StructDef* stdef = m_ctx->structs()[ structidx];
		int mi = 0, me = stdef->nofmembers;
		for (; mi != me; ++mi)
		{
			TagLevelRange taglevelRange = getTagLevelRange( stdef->members[ mi].resolvetype, taglevel, stdef->members[ mi].max_tag_diff);
			if (stdef->members[ mi].name == 0)
			{
				if (!resolveItem( sink, stdef->members[ mi].itemid, stdef->members[ mi].resolvetype, scope.inner(), taglevelRange, true/*embedded*/))
				{
					m_structpath.insert( m_structpath.begin(), stdef->members[ mi].name);
					if (m_errstruct.scopestart < scope.from)
					{
						m_errstruct.scopestart = scope.from;
					}
					return false;
				}
			}
			else
			{
				sink.pushName( stdef->members[ mi].name);
				if (!resolveItem( sink, stdef->members[ mi].itemid, stdef->members[ mi].resolvetype, scope.inner(), taglevelRange, false/*embedded*/))
				{
					m_structpath.insert( m_structpath.begin(), stdef->members[ mi].name);
					if (m_errstruct.scopestart < scope.from)
					{
						m_errstruct.scopestart = scope.from;
					}
					return false;
				}
			}
		}
		return true;
	}

	bool addResolvedItemValue( ValueSink& sink, const ResolvedObject& resolvedObj, bool embedded)
	{
		if (ObjectRef_is_value( resolvedObj.objref))
		{
			int valueidx = ObjectRef_value_id( resolvedObj.objref);
			const papuga_ValueVariant* value = &m_ctx->values()[ valueidx].content;
			if (!papuga_ValueVariant_defined( value))
			{
				return false;
			}
			if (!sink.pushValue( value))
			{
				m_errstruct.errcode = papuga_NoMemError;
				return false;
			}
		}
		else if (ObjectRef_is_struct( resolvedObj.objref))
		{
			int structidx = ObjectRef_struct_id( resolvedObj.objref);
			if (embedded)
			{
				if (!build_structure( sink, resolvedObj.scope, resolvedObj.taglevel, structidx)) return false;
			}
			else
			{
				if (!sink.openSerialization())
				{
					m_errstruct.errcode = papuga_NoMemError;
					return false;
				}
				if (!build_structure( sink, resolvedObj.scope, resolvedObj.taglevel, structidx)) return false;
				if (!sink.closeSerialization())
				{
					m_errstruct.errcode = papuga_NoMemError;
					return false;
				}
			}
		}
		else
		{
			if (!sink.pushVoid())
			{
				m_errstruct.errcode = papuga_NoMemError;
				return false;
			}
		}
		return true;
	}

	void logObjectEvent( const char* title, int itemid, const ObjectRef& objref)
	{
		if (ObjectRef_is_struct( objref))
		{
			int structidx = ObjectRef_struct_id( objref);
			char buf[ 64];
			std::snprintf( buf, sizeof(buf), "{%d}", structidx);
			papuga_ValueVariant ov;
			papuga_init_ValueVariant_charp( &ov, buf);
			m_logContentEvent( m_loggerSelf, title, itemid, &ov);
		}
		else if (ObjectRef_is_value( objref))
		{
			int valueidx = ObjectRef_value_id( objref);
			m_logContentEvent( m_loggerSelf, title, itemid, &m_ctx->values()[ valueidx].content);
		}
		else
		{
			m_logContentEvent( m_loggerSelf, title, itemid, NULL/*value*/);
		}
	}

	/* \brief Build the data requested by an item id and a scope in a manner defined by a class of resolving a reference */
	bool resolveItem( ValueSink& sink, int itemid, papuga_ResolveType resolvetype, const Scope& scope, const TagLevelRange& taglevelRange, bool embedded)
	{
		setResolverUpperBound( scope, itemid);
		switch (resolvetype)
		{
			case papuga_ResolveTypeRequired:
			case papuga_ResolveTypeOptional:
			{
				ResolvedObject resolvedObj = resolveNearItemInsideScope( scope, taglevelRange, itemid);
				if (m_logContentEvent)
				{
					if (resolvedObj.valid())
					{
						logObjectEvent( "resolved required", itemid, resolvedObj.objref);
					}
					else if (resolvetype == papuga_ResolveTypeRequired)
					{
						m_logContentEvent( m_loggerSelf, "unresolved required", itemid, 0);
					}
				}
				// We try to get a valid node candidate preferring value nodes if defined
				while (resolvedObj.valid() && !addResolvedItemValue( sink, resolvedObj, embedded))
				{
					if (!m_errstruct.itemid)
					{
						m_errstruct.itemid = itemid;
					}
					if (m_errstruct.errcode != papuga_Ok) return false;
					resolvedObj = resolveNextSameScopeItem( itemid);
				}
				if (!resolvedObj.valid())
				{
					if (resolvetype == papuga_ResolveTypeRequired)
					{
						if (!m_errstruct.itemid)
						{
							m_errstruct.itemid = itemid;
						}
						m_errstruct.errcode = papuga_ValueUndefined;
						return false;
					}
					else
					{
						if (m_errstruct.errcode != papuga_Ok)
						{
							return false;
						}
						if (!m_errstruct.itemid)
						{
							m_errstruct.itemid = itemid;
						}
						sink.pushVoid();
						// ... we get here only if we found a value that matches, but was undefined
					}
				}
				ResolvedObject resolvedObjNext = resolveNextInsideItem( scope, taglevelRange, itemid);
				if (resolvedObjNext.valid())
				{
					if (!m_errstruct.itemid)
					{
						m_errstruct.itemid = itemid;
					}
					m_errstruct.errcode = papuga_AmbiguousReference;
					return false;
				}
			}
			break;
			case papuga_ResolveTypeInherited:
			{
				ResolvedObject resolvedObj = resolveNearItemCoveringScope( scope, taglevelRange, itemid);
				if (m_logContentEvent)
				{
					if (resolvedObj.valid())
					{
						logObjectEvent( "resolved inherited", itemid, resolvedObj.objref);
					}
					else
					{
						m_logContentEvent( m_loggerSelf, "unresolved inherited", itemid, 0);
					}
				}
				if (ObjectRef_is_value( resolvedObj.objref))
				{
					int valueidx = ObjectRef_value_id( resolvedObj.objref);
					if (!sink.pushValue( &m_ctx->values()[ valueidx].content))
					{
						m_errstruct.errcode = papuga_NoMemError;
						return false;
					}
					if (hasNextCoveringItem( scope, taglevelRange, itemid))
					{
						if (!m_errstruct.itemid)
						{
							m_errstruct.itemid = itemid;
						}
						m_errstruct.errcode = papuga_AmbiguousReference;
						return false;
					}
				}
				else if (ObjectRef_is_struct( resolvedObj.objref))
				{
					if (!m_errstruct.itemid)
					{
						m_errstruct.itemid = itemid;
					}
					m_errstruct.errcode = papuga_InvalidAccess;
					return false;
				}
				else
				{
					if (!m_errstruct.itemid)
					{
						m_errstruct.itemid = itemid;
					}
					m_errstruct.errcode = papuga_ValueUndefined;
					return false;
				}
			}
			break;
			case papuga_ResolveTypeArrayNonEmpty:
			case papuga_ResolveTypeArray:
			{
				ResolvedObject resolvedObj = resolveNearItemInsideScope( scope, taglevelRange, itemid);
				if (m_logContentEvent)
				{
					if (resolvedObj.valid())
					{
						logObjectEvent( "resolved first of array", itemid, resolvedObj.objref);
					}
					else
					{
						m_logContentEvent( m_loggerSelf, "empty array", itemid, 0);
					}
				}
				if (!sink.openSerialization())
				{
					m_errstruct.errcode = papuga_NoMemError;
					return false;
				}
				int arrayElementCount = 0;
				while (resolvedObj.valid())
				{
					// We try to get a valid node candidate preferring value nodes if defined
					while (!addResolvedItemValue( sink, resolvedObj, false/*embedded*/))
					{
						resolvedObj = resolveNextSameScopeItem( itemid);
						if (!resolvedObj.valid()) break;
					}
					if (!resolvedObj.valid())
					{
						if (m_errstruct.errcode != papuga_Ok)
						{
							if (!m_errstruct.itemid)
							{
								m_errstruct.itemid = itemid;
							}
							return false;
						}
						sink.pushVoid();
						// ... we get here only if we found a value that matches, but was undefined
					}
					++arrayElementCount;
					resolvedObj = resolveNextInsideItem( scope, taglevelRange, itemid);
				}
				if (arrayElementCount == 0 && resolvetype == papuga_ResolveTypeArrayNonEmpty)
				{
					if (!m_errstruct.itemid)
					{
						m_errstruct.itemid = itemid;
					}
					m_errstruct.errcode = papuga_ValueUndefined;
					return false;
				}
				if (!sink.closeSerialization())
				{
					if (!m_errstruct.itemid)
					{
						m_errstruct.itemid = itemid;
					}
					m_errstruct.errcode = papuga_NoMemError;
					return false;
				}
			}
			break;
		}
		return true;
	}

	bool setCallArgValue( papuga_ValueVariant& arg, const CallArgDef& argdef, const Scope& scope, int taglevel, const papuga_RequestContext* context)
	{
		if (argdef.varname)
		{
			const papuga_ValueVariant* value = papuga_RequestContext_get_variable( context, argdef.varname);
			if (!value)
			{
				m_errstruct.variable = argdef.varname;
				m_errstruct.errcode = papuga_ValueUndefined;
				return false;
			}
			papuga_init_ValueVariant_value( &arg, value);
		}
		else
		{
			TagLevelRange tagLevelRange = getTagLevelRange( argdef.resolvetype, taglevel, argdef.max_tag_diff);
			ValueSink sink( &arg, &m_allocator);
			if (!resolveItem( sink, argdef.itemid, argdef.resolvetype, scope, tagLevelRange, false/*embedded*/))
			{
				if (m_errstruct.scopestart < scope.from)
				{
					m_errstruct.scopestart = scope.from;
				}
				return false;
			}
		}
		return true;
	}

private:
	papuga_RequestMethodCall m_curr_methodcall;
	const AutomatonContext* m_ctx;
	papuga_RequestEventLoggerProcedure m_logContentEvent;
	void* m_loggerSelf;
	std::vector<ScopeObjItr> m_resolvers;
	int m_curr_methodidx;
	papuga_Allocator m_allocator;
	char m_allocator_membuf[ 4096];
	std::vector<std::string> m_structpath;
	papuga_RequestError m_errstruct;
};

}//anonymous namespace

extern "C" void papuga_init_RequestError( papuga_RequestError* self)
{
	self->errcode = papuga_Ok;
	self->scopestart = -1;
	self->argcnt = -1;
	self->classname = 0;
	self->methodname = 0;
	self->variable = 0;
	self->itemid = -1;
	self->structpath[0] = '\0';
	self->errormsg[0] = 0;
}

extern "C" const char* papuga_ResolveTypeName( papuga_ResolveType resolvetype)
{
	static const char* ar[] = {"required","optional","inherited","array"};
	return ar[ resolvetype];
}

struct papuga_RequestAutomaton
{
	AutomatonDescription atm;
	const papuga_StructInterfaceDescription* structdefs;
};

extern "C" papuga_RequestAutomaton* papuga_create_RequestAutomaton(
		const papuga_ClassDef* classdefs,
		const papuga_StructInterfaceDescription* structdefs,
		bool strict)
{
	papuga_RequestAutomaton* rt = (papuga_RequestAutomaton*)std::calloc( 1, sizeof(*rt));
	if (!rt) return NULL;
	try
	{
		new (&rt->atm) AutomatonDescription( classdefs, strict);
		rt->structdefs = structdefs;
		return rt;
	}
	catch (...)
	{
		std::free( rt);
		return NULL;
	}
}

extern "C" void papuga_destroy_RequestAutomaton( papuga_RequestAutomaton* self)
{
	self->atm.~AutomatonDescription();
	std::free( self);
}


extern "C" papuga_ErrorCode papuga_RequestAutomaton_last_error( const papuga_RequestAutomaton* self)
{
	return self->atm.lastError();
}

extern "C" bool papuga_RequestAutomaton_inherit_from(
		papuga_RequestAutomaton* self,
		const char* type,
		const char* name_expression,
		bool required)
{
	return self->atm.inheritFrom( type, name_expression, required);
}

extern "C" bool papuga_RequestAutomaton_add_call(
		papuga_RequestAutomaton* self,
		const char* expression,
		const papuga_RequestMethodId* method,
		const char* selfvarname,
		const char* resultvarname,
		int nargs)
{
	return self->atm.addCall( expression, method, selfvarname, resultvarname, nargs);
}

extern "C" bool papuga_RequestAutomaton_set_call_arg_var( papuga_RequestAutomaton* self, int idx, const char* varname)
{
	return self->atm.setCallArgVar( idx, varname);
}

extern "C" bool papuga_RequestAutomaton_set_call_arg_item( papuga_RequestAutomaton* self, int idx, int itemid, papuga_ResolveType resolvetype, int max_tag_diff)
{
	return self->atm.setCallArgItem( idx, itemid, resolvetype, max_tag_diff);
}

extern "C" bool papuga_RequestAutomaton_prioritize_last_call(
		papuga_RequestAutomaton* self,
		const char* scope_expression)
{
	return self->atm.prioritizeLastCallInScope( scope_expression);
}

extern "C" bool papuga_RequestAutomaton_open_group( papuga_RequestAutomaton* self)
{
	return self->atm.openGroup();
}

extern "C" bool papuga_RequestAutomaton_close_group( papuga_RequestAutomaton* self)
{
	return self->atm.closeGroup();
}

extern "C" bool papuga_RequestAutomaton_add_structure(
		papuga_RequestAutomaton* self,
		const char* expression,
		int itemid,
		int nofmembers)
{
	return self->atm.addStructure( expression, itemid, nofmembers);
}

extern "C" bool papuga_RequestAutomaton_set_structure_element(
		papuga_RequestAutomaton* self,
		int idx,
		const char* name,
		int itemid,
		papuga_ResolveType resolvetype,
		int max_tag_diff)
{
	return self->atm.setMember( idx, name, itemid, resolvetype, max_tag_diff);
}

extern "C" bool papuga_RequestAutomaton_add_value(
		papuga_RequestAutomaton* self,
		const char* scope_expression,
		const char* select_expression,
		int itemid)
{
	return self->atm.addValue( scope_expression, select_expression, itemid);
}

extern "C" bool papuga_RequestAutomation_add_result(
		papuga_RequestAutomaton* self,
		papuga_RequestResultDescription* descr)
{
	return self->atm.addResultDescription( descr);
}

extern "C" bool papuga_RequestAutomaton_done( papuga_RequestAutomaton* self)
{
	return self->atm.done();
}

struct papuga_Request
{
	AutomatonContext ctx;
	const papuga_StructInterfaceDescription* structdefs;
};

extern "C" papuga_Request* papuga_create_Request( const papuga_RequestAutomaton* atm, papuga_RequestLogger* logger)
{
	papuga_Request* rt = (papuga_Request*)std::calloc( 1, sizeof(*rt));
	if (!rt) return NULL;
	try
	{
		new (&rt->ctx) AutomatonContext( &atm->atm, logger);
		rt->structdefs = atm->structdefs;
		return rt;
	}
	catch (...)
	{
		std::free( rt);
		return NULL;
	}
}

extern "C" void papuga_destroy_Request( papuga_Request* self)
{
	self->ctx.~AutomatonContext();
	std::free( self);
}

extern "C" const papuga_RequestInheritedContextDef* papuga_Request_get_inherited_contextdefs( const papuga_Request* self, papuga_ErrorCode* errcode)
{
	return self->ctx.getRequiredInheritedContextsDefs( *errcode);
}

extern "C" const papuga_ClassDef* papuga_Request_classdefs( const papuga_Request* self)
{
	return self->ctx.classdefs();
}

extern "C" bool papuga_Request_feed_open_tag( papuga_Request* self, const papuga_ValueVariant* tagname)
{
	return self->ctx.processOpenTag( tagname);
}

extern "C" bool papuga_Request_feed_close_tag( papuga_Request* self)
{
	return self->ctx.processCloseTag();
}

extern "C" bool papuga_Request_feed_attribute_name( papuga_Request* self, const papuga_ValueVariant* attrname)
{
	return self->ctx.processAttributeName( attrname);
}

extern "C" bool papuga_Request_feed_attribute_value( papuga_Request* self, const papuga_ValueVariant* value)
{
	return self->ctx.processAttributeValue( value);
}

extern "C" bool papuga_Request_feed_content_value( papuga_Request* self, const papuga_ValueVariant* value)
{
	return self->ctx.processValue( value);
}

extern "C" bool papuga_Request_done( papuga_Request* self)
{
	return self->ctx.done();
}

extern "C" papuga_ErrorCode papuga_Request_last_error( const papuga_Request* self)
{
	return self->ctx.lastError();
}

extern "C" int papuga_Request_last_error_itemid( const papuga_Request* self)
{
	return self->ctx.lastErrorItemId();
}

extern "C" bool papuga_Request_is_result_variable( const papuga_Request* self, const char* varname)
{
	return self->ctx.isResultVariable( varname);
}


struct papuga_RequestIterator
{
	RequestIterator itr;
};

extern "C" papuga_RequestIterator* papuga_create_RequestIterator( papuga_Allocator* allocator, const papuga_Request* request, papuga_ErrorCode* errcode)
{
	papuga_RequestIterator* rt = (papuga_RequestIterator*)papuga_Allocator_alloc( allocator, sizeof(*rt), 0);
	if (!rt) return NULL;
	try
	{
		if (request->ctx.isDone())
		{
			new (&rt->itr) RequestIterator( &request->ctx);
		}
		else
		{
			*errcode = papuga_ExecutionOrder;
			rt = NULL;
		}
		return rt;
	}
	catch (...)
	{
		*errcode = papuga_NoMemError;
		return NULL;
	}
}

extern "C" void papuga_destroy_RequestIterator( papuga_RequestIterator* self)
{
	self->itr.~RequestIterator();
}

extern "C" const papuga_RequestMethodCall* papuga_RequestIterator_next_call( papuga_RequestIterator* self, const papuga_RequestContext* context)
{
	return self->itr.nextCall( context);
}

extern "C" bool papuga_RequestIterator_push_call_result( papuga_RequestIterator* self, const papuga_ValueVariant* result)
{
	return self->itr.pushCallResult( *result);
}

extern "C" papuga_RequestResult* papuga_get_RequestResult_array( papuga_RequestIterator* self, papuga_Allocator* allocator, int* nofResults)
{
	return self->itr.getResultArray( allocator, *nofResults);
}

extern "C" const papuga_RequestError* papuga_RequestIterator_get_last_error( papuga_RequestIterator* self)
{
	return self->itr.lastError();
}

extern "C" const papuga_StructInterfaceDescription* papuga_Request_struct_descriptions( const papuga_Request* self)
{
	return self->structdefs;
}


