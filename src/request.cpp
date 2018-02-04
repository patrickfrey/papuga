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
#include "papuga/serialization.h"
#include "papuga/serialization.hpp"
#include "papuga/allocator.h"
#include "papuga/valueVariant.h"
#include "papuga/valueVariant.hpp"
#include "papuga/callArgs.h"
#include "papuga/classdef.h"
#include "papuga/allocator.h"
#include "papuga/stack.h"
#include "papuga/errors.hpp"
#include "textwolf/xmlpathautomatonparse.hpp"
#include "textwolf/xmlpathselect.hpp"
#include "textwolf/charset.hpp"
#include <stdexcept>
#include <sstream>
#include <iostream>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <cmath>
#include <algorithm>
#include <limits>

#define PAPUGA_LOWLEVEL_DEBUG

using namespace papuga;

namespace {
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

	StructMemberDef( const char* name_, int itemid_, papuga_ResolveType resolvetype_, int max_tag_diff_)
		:name(name_),itemid(itemid_),resolvetype(resolvetype_),max_tag_diff(max_tag_diff_){}
	StructMemberDef( const StructMemberDef& o)
		:name(o.name),itemid(o.itemid),resolvetype(o.resolvetype),max_tag_diff(o.max_tag_diff){}
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

	CallArgDef( const char* varname_)
		:varname(varname_),itemid(0),resolvetype(papuga_ResolveTypeRequired),max_tag_diff(0){}
	CallArgDef( int itemid_, papuga_ResolveType resolvetype_, int max_tag_diff_)
		:varname(0),itemid(itemid_),resolvetype(resolvetype_),max_tag_diff(max_tag_diff_){}
	CallArgDef( const CallArgDef& o)
		:varname(o.varname),itemid(o.itemid),resolvetype(o.resolvetype),max_tag_diff(o.max_tag_diff){}
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
		methodid.functionid  = methodid_->functionid ;
	}
	CallDef( const CallDef& o)
		:selfvarname(o.selfvarname),resultvarname(o.resultvarname),args(o.args),nofargs(o.nofargs),groupid(o.groupid)
	{
		methodid.classid = o.methodid.classid;
		methodid.functionid = o.methodid.functionid;
	}

#ifdef PAPUGA_LOWLEVEL_DEBUG
	std::string tostring() const
	{
		std::ostringstream out;
		if (resultvarname) out << resultvarname << " = ";
		if (selfvarname) out << selfvarname << "->";
		out << "[" << methodid.classid << "," << methodid.functionid << "] (";
		int ai = 0, ae = nofargs;
		for (; ai != ae; ++ai)
		{
			out << (ai ? ", " : " ");
			if (args[ai].varname)
			{
				out << "?" << args[ai].varname;
			}
			else
			{
				out << args[ai].itemid << " " << papuga_ResolveTypeName( args[ai].resolvetype);
			}
		}
		out << ");";
		return out.str();
	}
#endif
};

typedef int AtmRef;
enum AtmRefType {InstantiateValue,CollectValue,CloseStruct,MethodCall};
enum {MaxAtmRefType=MethodCall};
const char* atmRefTypeName( AtmRefType t)
{
	const char* ar[ MaxAtmRefType+1] = {"InstantiateValue","CollectValue","CloseStruct","MethodCall"};
	return ar[t];
}
static AtmRef AtmRef_get( AtmRefType type, int idx)	{return (AtmRef)(((int)type<<28) | (idx+1));}
static AtmRefType AtmRef_type( AtmRef atmref)		{return (AtmRefType)((atmref>>28) & 0x7);}
static int AtmRef_index( AtmRef atmref)			{return ((int)atmref & 0x0fFFffFF)-1;}

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

	explicit AutomatonDescription( const papuga_ClassDef* classdefs_, const char* resultname_)
		:m_classdefs(classdefs_),m_resultname(resultname_)
		,m_nof_classdefs(nofClassDefs(classdefs_))
		,m_calldefs(),m_structdefs(),m_valuedefs(),m_atm(),m_maxitemid(0)
		,m_errcode(papuga_Ok),m_groupid(-1),m_done(false)
	{
		papuga_init_Allocator( &m_allocator, m_allocatorbuf, sizeof(m_allocatorbuf));
	}

	const papuga_ClassDef* classdefs() const
	{
		return m_classdefs;
	}
	const char* resultname() const
	{
		return m_resultname;
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

	/* @param[in] expression selector of the call scope */
	/* @param[in] nofargs number of arguments */
	bool addCall( const char* expression, const papuga_RequestMethodId* method_, const char* selfvarname_, const char* resultvarname_, int nofargs)
	{
		try
		{
#ifdef PAPUGA_LOWLEVEL_DEBUG
			const char* methodname;
			const char* classname;
#endif
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
			if (method_->classid == 0 || method_->classid > m_nof_classdefs)
			{
				m_errcode = papuga_AddressedItemNotFound;
				return false;
			}
			const papuga_ClassDef* cdef = &m_classdefs[ method_->classid-1];
			if (method_->functionid > cdef->methodtablesize)
			{
				m_errcode = papuga_AddressedItemNotFound;
				return false;
			}
#ifdef PAPUGA_LOWLEVEL_DEBUG
			classname = cdef->name;
			methodname = method_->functionid == 0 ? "new" : cdef->methodnames[ method_->functionid-1];
			fprintf( stderr, "automaton add call expression='%s', class=%s, method=%s, self='%s', result='%s', nofargs=%d\n",
					expression, classname, methodname, selfvarname_?selfvarname_:"", resultvarname_?resultvarname_:"", nofargs);
#endif
			std::string open_expression( cut_trailing_slashes( expression));
			std::string close_expression( open_expression + "~");
			int mm = nofargs * sizeof(CallArgDef);
			CallArgDef* car = NULL;
			if (nofargs)
			{
				car = (CallArgDef*)papuga_Allocator_alloc( &m_allocator, mm, 0);
				if (!car)
				{
					m_errcode = papuga_NoMemError;
					return false;
				}
				std::memset( car, 0, mm);
			}
			const char* selfvarname = copyIfDefined( selfvarname_);
			const char* resultvarname = copyIfDefined( resultvarname_);
			if (m_errcode != papuga_Ok) return false;

			int evid = AtmRef_get( MethodCall, m_calldefs.size());
			m_atm.addExpression( evid, close_expression.c_str(), close_expression.size());
#ifdef PAPUGA_LOWLEVEL_DEBUG
			fprintf( stderr, "automaton add event [%s %d] call expression='%s'\n",
				 atmRefTypeName(MethodCall), (int)m_calldefs.size(), close_expression.c_str());
#endif
			m_calldefs.push_back( CallDef( method_, selfvarname, resultvarname, car, nofargs, m_groupid < 0 ? m_calldefs.size():m_groupid));
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
#ifdef PAPUGA_LOWLEVEL_DEBUG
			fprintf( stderr, "automaton set call argument variable=%s\n", argvar);
#endif
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
#ifdef PAPUGA_LOWLEVEL_DEBUG
			fprintf( stderr, "automaton set call argument item id=%d resolve=%s, max tag diff %d\n", itemid, papuga_ResolveTypeName( resolvetype), max_tag_diff);
#endif
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
#ifdef PAPUGA_LOWLEVEL_DEBUG
			fprintf( stderr, "automaton add structure expression='%s', item id=%d, nofmembers=%d\n", expression, itemid, nofmembers);
#endif
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
			StructMemberDef* mar = (StructMemberDef*)papuga_Allocator_alloc( &m_allocator, mm, 0);
			if (!mar)
			{
				m_errcode = papuga_NoMemError;
				return false;
			}
			std::memset( mar, 0, mm);
			int evid = AtmRef_get( CloseStruct, m_structdefs.size());
			m_atm.addExpression( evid, close_expression.c_str(), close_expression.size());
#ifdef PAPUGA_LOWLEVEL_DEBUG
			fprintf( stderr, "automaton add event [%s %d] structure close expression='%s'\n",
				 atmRefTypeName(CloseStruct), (int)m_structdefs.size(), close_expression.c_str());
#endif
			m_structdefs.push_back( StructDef( itemid, mar, nofmembers));
		}
		CATCH_LOCAL_EXCEPTION(m_errcode,false)
		return true;
	}

	bool setMember( int idx, const char* name, int itemid, papuga_ResolveType resolvetype, int max_tag_diff)
	{
		try
		{
	#ifdef PAPUGA_LOWLEVEL_DEBUG
			fprintf( stderr, "automaton set structure member name='%s', item id=%d, resolve=%s, max tag diff %d\n", name, itemid, papuga_ResolveTypeName( resolvetype), max_tag_diff);
	#endif
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
			if (name)
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
#ifdef PAPUGA_LOWLEVEL_DEBUG
			fprintf( stderr, "automaton add value scope='%s', select='%s', item id=%d\n", scope_expression, select_expression, itemid);
#endif
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
			std::string close_expression( open_expression + "~");
			std::string value_expression;
			if (select_expression[0] == '/')
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
			m_atm.addExpression( evid_inst, value_expression.c_str(), value_expression.size());
			m_atm.addExpression( evid_coll, close_expression.c_str(), close_expression.size());
#ifdef PAPUGA_LOWLEVEL_DEBUG
			fprintf( stderr, "automaton add event [%s %d] instantiate value expression='%s'\n",
				 atmRefTypeName( InstantiateValue), (int)m_valuedefs.size(), value_expression.c_str());
			fprintf( stderr, "automaton add event [%s %d] collect value expression='%s'\n",
				 atmRefTypeName( CollectValue), (int)m_valuedefs.size(), close_expression.c_str());
#endif
			m_valuedefs.push_back( ValueDef( itemid));
		}
		CATCH_LOCAL_EXCEPTION(m_errcode,false)
		return true;
	}

	bool openGroup()
	{
#ifdef PAPUGA_LOWLEVEL_DEBUG
		fprintf( stderr, "automaton open group\n");
#endif
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
#ifdef PAPUGA_LOWLEVEL_DEBUG
		fprintf( stderr, "automaton close group\n");
#endif
		if (m_groupid < 0)
		{
			m_errcode = papuga_LogicError;
			return false;
		}
		m_groupid = -1;
		return true;
	}

	bool done()
	{
#ifdef PAPUGA_LOWLEVEL_DEBUG
		fprintf( stderr, "automaton done\n");
		fflush( stderr);
#endif
		if (m_done)
		{
			m_errcode = papuga_LogicError;
			return false;
		}
		m_done = true;
		return true;
	}

	const std::vector<CallDef>& calldefs() const			{return m_calldefs;}
	const std::vector<StructDef>& structdefs() const		{return m_structdefs;}
	const std::vector<ValueDef>& valuedefs() const			{return m_valuedefs;}
	const XMLPathSelectAutomaton& atm() const			{return m_atm;}
	std::size_t maxitemid() const					{return m_maxitemid;}

private:
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
		while (size && expression[size-1] == '/') --size;
		return std::string( expression, size);
	}

private:
	const papuga_ClassDef* m_classdefs;			//< array of classes
	const char* m_resultname;				//< label of result (top level tag for XML)
	int m_nof_classdefs;					//< number of classes defined in m_classdefs
	std::vector<CallDef> m_calldefs;
	std::vector<StructDef> m_structdefs;
	std::vector<ValueDef> m_valuedefs;
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

#ifdef PAPUGA_LOWLEVEL_DEBUG
static std::string ObjectRef_descr( ObjectRef objref)
{
	std::ostringstream out;
	if (ObjectRef_is_struct( objref))
	{
		out << "struct " << ObjectRef_struct_id( objref);
	}
	else if (ObjectRef_is_value( objref))
	{
		out << "value " << ObjectRef_value_id( objref);
	}
	else
	{
		out << "NULL";
	}
	return out.str();
}
#endif

struct Scope
{
	int from;
	int to;

	Scope( int from_, int to_)
		:from(from_),to(to_){}
	Scope( const Scope& o)
		:from(o.from),to(o.to){}

	bool inside( const Scope& o) const
	{
		return (from >= o.from && to <= o.to);
	}
	Scope inner() const
	{
		return Scope( from+1, to);
	}
	bool operator==( const Scope& o) const
	{
		return from == o.from && to == o.to;
	}
	bool operator!=( const Scope& o) const
	{
		return from != o.from || to != o.to;
	}
};

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
		if (group != o.group) return -(int)( group < o.group);
		if (scope != o.scope) return -(int)( scope < o.scope);
		if (elemidx != o.elemidx) return -(int)( elemidx < o.elemidx);
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
	self->eventcnt = 0;
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


class AutomatonContext
{
public:
	explicit AutomatonContext( const AutomatonDescription* atm_)
		:m_atm(atm_),m_atmstate(&atm_->atm()),m_scopecnt(0),m_scopestack()
		,m_valuenodes(),m_values(),m_structs(),m_scopeobjmap( atm_->maxitemid()+1, ScopeObjMap()),m_methodcalls()
		,m_done(false),m_errcode(papuga_Ok)
	{
		papuga_init_Allocator( &m_allocator, m_allocator_membuf, sizeof(m_allocator_membuf));
		m_scopestack.reserve( 32);
		m_scopestack.push_back( 0);
	}
	~AutomatonContext()
	{
		papuga_destroy_Allocator( &m_allocator);
	}

	papuga_ErrorCode lastError() const
	{
		return m_errcode;
	}

	bool processEvents( const textwolf::XMLScannerBase::ElementType tp, const papuga_ValueVariant* value, const char* valuestr, size_t valuelen)
	{
		AutomatonState::iterator itr = m_atmstate.push( tp, valuestr, valuelen);
		for (*itr; *itr; ++itr)
		{
			int ev = *itr;
			m_event_stacks[ AtmRef_type(ev)].push( ev);
#ifdef PAPUGA_LOWLEVEL_DEBUG
			fprintf( stderr, "triggered event [%s %d]\n",
				 atmRefTypeName( AtmRef_type( ev)), AtmRef_index( ev));
#endif
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

#ifdef PAPUGA_LOWLEVEL_DEBUG
		fprintf( stderr, "push element type %s value '%s'\n",
			 textwolf::XMLScannerBase::getElementTypeName( tp), valuestr);
#endif
		return processEvents( tp, value, valuestr, valuelen);
	}

	bool pushEmptyAndProcessEvents( const textwolf::XMLScannerBase::ElementType tp, const papuga_ValueVariant* value)
	{
#ifdef PAPUGA_LOWLEVEL_DEBUG
		fprintf( stderr, "push element type %s\n",
			 textwolf::XMLScannerBase::getElementTypeName( tp));
#endif
		return processEvents( tp, value, "", 0);
	}

#ifdef PAPUGA_LOWLEVEL_DEBUG
	void DEBUG_logProcessEvent( const char* name, const papuga_ValueVariant* value)
	{
		papuga_ErrorCode errcode = papuga_Ok;
		std::string valuestr = ValueVariant_tostring( *value, errcode);
		fprintf( stderr, "scope [%d,%d]: process %s '%s'\n", curscope().from, curscope().to, name, valuestr.c_str());
	}
#else
#define DEBUG_logProcessEvent( name, value)
#endif

	bool processOpenTag( const papuga_ValueVariant* tagname)
	{
		try
		{
			++m_scopecnt;
			DEBUG_logProcessEvent( "open tag", tagname);
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
			DEBUG_logProcessEvent( "attribute name", attrname);
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
			DEBUG_logProcessEvent( "attribute value", value);
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
			DEBUG_logProcessEvent( "content value", value);
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
			DEBUG_logProcessEvent( "close tag", &empty.content);
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
			++m_scopecnt;
			if (m_done) return true;
			std::sort( m_methodcalls.begin(), m_methodcalls.end());
#ifdef PAPUGA_LOWLEVEL_DEBUG
			printScopeObjMap( std::cerr);
#endif
			return m_done = true;
		}
		CATCH_LOCAL_EXCEPTION(m_errcode,false)
	}

	typedef std::pair<ScopeKey,ObjectDescr> ScopeObjElem;
	typedef std::multimap<ScopeKey,ObjectDescr> ScopeObjMap;
	typedef ScopeObjMap::const_iterator ScopeObjItr;

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
	void insertObjectRef( int itemid, const ScopeKeyType& type, const Scope& scope, int taglevel, const ObjectRef& objref)
	{
		m_scopeobjmap[ itemid].insert( ScopeObjElem( ScopeKey(type,scope), ObjectDescr( objref, taglevel)));
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
	const char* resultname() const
	{
		return m_atm->resultname();
	}

	class Iterator
	{
	public:
		explicit Iterator( papuga_ErrorCode errcode_)
			:m_ctx(0),m_resolvers(),m_curr_methodidx(0),m_errcode(errcode_)
		{
			papuga_init_Allocator( &m_allocator, m_allocator_membuf, sizeof(m_allocator_membuf));
			papuga_init_RequestMethodCall( &m_curr_methodcall);
		}
		explicit Iterator( const AutomatonContext* ctx_)
			:m_ctx(ctx_),m_resolvers(ctx_->scopeobjmap().size(),ScopeObjItr()),m_curr_methodidx(0),m_errcode(papuga_Ok)
		{
			papuga_init_Allocator( &m_allocator, m_allocator_membuf, sizeof(m_allocator_membuf));
			papuga_init_RequestMethodCall( &m_curr_methodcall);
			std::vector<ScopeObjMap>::const_iterator mi = ctx_->scopeobjmap().begin(), me = ctx_->scopeobjmap().end();
			for (int midx=0; mi != me; ++mi,++midx)
			{
				m_resolvers[ midx] = mi->end();
			}
		}

		~Iterator()
		{
			papuga_destroy_Allocator( &m_allocator);
		}

		papuga_RequestMethodCall* next( const papuga_RequestVariable* varlist)
		{
			if (!m_ctx) return NULL;
			const MethodCallNode* mcnode = m_ctx->methodCallNode( m_curr_methodidx);
			if (!mcnode)
			{
				std::memset( &m_curr_methodcall, 0, sizeof(m_curr_methodcall));
				m_curr_methodcall.eventcnt = -1;
				return NULL;
			}
			m_curr_methodcall.selfvarname = mcnode->def->selfvarname;
			m_curr_methodcall.resultvarname = mcnode->def->resultvarname;
			m_curr_methodcall.methodid.classid = mcnode->def->methodid.classid;
			m_curr_methodcall.methodid.functionid = mcnode->def->methodid.functionid;
			m_curr_methodcall.eventcnt = mcnode->scope.from;
			m_curr_methodcall.argcnt = -1;
			papuga_init_CallArgs( &m_curr_methodcall.args, m_curr_methodcall.membuf, sizeof(m_curr_methodcall.membuf));

			const CallDef* mcdef = mcnode->def;
			papuga_CallArgs* args = &m_curr_methodcall.args;
			int ai = 0, ae = mcdef->nofargs;
			for (; ai != ae; ++ai)
			{
				papuga_init_ValueVariant( args->argv + ai);
				if (!setCallArgValue( args->argv[ai], mcdef->args[ai], mcnode->scope, mcnode->taglevel, varlist))
				{
					m_curr_methodcall.argcnt = ai;
					return NULL;
				}
			}
			args->argc = mcdef->nofargs;
			++m_curr_methodidx;
			return &m_curr_methodcall;
		}

		const MethodCallNode* nextNode()
		{
			if (!m_ctx) return NULL;
			const MethodCallNode* mcnode = m_ctx->methodCallNode( m_curr_methodidx);
			if (!mcnode) return NULL;
			++m_curr_methodidx;
			return mcnode;
		}

		bool printMethodCallNode( std::ostream& out, const MethodCallNode* mcnode)
		{
			const CallDef* mcdef = mcnode->def;
			int cidx = mcdef->methodid.classid-1;
			int midx = mcdef->methodid.functionid-1;
			if (mcdef->resultvarname)
			{
				out << "RES " << mcdef->resultvarname << std::endl;
			}
			if (mcdef->selfvarname)
			{
				out << "OBJ " << mcdef->selfvarname << std::endl;
			}
			if (midx == -1)
			{
				out << "NEW " << m_ctx->classdefs()[cidx].name << std::endl;
			}
			else
			{
				out << "CALL " << m_ctx->classdefs()[cidx].name << "::" << m_ctx->classdefs()[cidx].methodnames[ midx] << std::endl;
			}
			int ai = 0, ae = mcdef->nofargs;
			for (; ai != ae; ++ai)
			{
				out << "ARG " << (ai+1) << std::endl;
				const CallArgDef& argdef = mcdef->args[ai];
				if (argdef.varname)
				{
					out << "\t$" << argdef.varname << std::endl;
				}
				else
				{
					TagLevelRange tagLevelRange = getTagLevelRange( argdef.resolvetype, mcnode->taglevel, argdef.max_tag_diff);
					papuga_ValueVariant argval;
					papuga_init_ValueVariant( &argval);
					ValueSink sink( &argval, &m_allocator);
					if (!resolveItem( sink, argdef.itemid, argdef.resolvetype, mcnode->scope, tagLevelRange))
					{
						return false;
					}
					switch (argval.valuetype)
					{
						case papuga_TypeVoid:		out << "\tNULL" << std::endl; break;
						case papuga_TypeDouble:
						case papuga_TypeInt:
						case papuga_TypeBool:
						case papuga_TypeString:		out << "\t'" << papuga::ValueVariant_tostring( argval, m_errcode) << "'"<< std::endl; break;
						case papuga_TypeHostObject:	out << "\t[Object " << argval.value.hostObject->classid << "]" << std::endl; break;
						case papuga_TypeSerialization:	out << papuga::Serialization_tostring( *argval.value.serialization, "\t", m_errcode) << std::endl; break;
						case papuga_TypeIterator:	out << "\t[Iterator]" << std::endl; break;
					}
				}
			}
			return true;
		}
		papuga_ErrorCode lastError() const
		{
			return m_errcode;
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
				sink.pushName( stdef->members[ mi].name);
				if (!resolveItem( sink, stdef->members[ mi].itemid, stdef->members[ mi].resolvetype, scope.inner(), taglevelRange))
				{
#ifdef PAPUGA_LOWLEVEL_DEBUG
					std::cerr << "resolving structure member '" << stdef->members[ mi].name << "' failed." << std::endl;
#endif
					return false;
				}
			}
			return true;
		}

		bool addResolvedItemValue( ValueSink& sink, const ResolvedObject& resolvedObj)
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
					m_errcode = papuga_NoMemError;
					return false;
				}
			}
			else if (ObjectRef_is_struct( resolvedObj.objref))
			{
				int structidx = ObjectRef_struct_id( resolvedObj.objref);
				if (!sink.openSerialization())
				{
					m_errcode = papuga_NoMemError;
					return false;
				}
				if (!build_structure( sink, resolvedObj.scope, resolvedObj.taglevel, structidx)) return false;
				if (!sink.closeSerialization())
				{
					m_errcode = papuga_NoMemError;
					return false;
				}
			}
			else
			{
				if (!sink.pushVoid())
				{
					m_errcode = papuga_NoMemError;
					return false;
				}
			}
			return true;
		}

		/* \brief Build the data requested by an item id and a scope in a manner defined by a class of resolving a reference */
		bool resolveItem( ValueSink& sink, int itemid, papuga_ResolveType resolvetype, const Scope& scope, const TagLevelRange& taglevelRange)
		{
			setResolverUpperBound( scope, itemid);
			switch (resolvetype)
			{
				case papuga_ResolveTypeRequired:
				case papuga_ResolveTypeOptional:
				{
					ResolvedObject resolvedObj = resolveNearItemInsideScope( scope, taglevelRange, itemid);
#ifdef PAPUGA_LOWLEVEL_DEBUG
					if (resolvedObj.valid())
					{
						std::string objdescr = ObjectRef_descr( resolvedObj.objref);
						fprintf( stderr, "search %s item %d in scope [%d,%d] taglevel [%d,%d] found %s in scope [%d,%d] taglevel %d\n",
							 papuga_ResolveTypeName(resolvetype), itemid, scope.from, scope.to,
							 taglevelRange.first, taglevelRange.second,
							 objdescr.c_str(), resolvedObj.scope.from, resolvedObj.scope.to, resolvedObj.taglevel);
					}
					else
					{
						fprintf( stderr, "search %s item %d in scope [%d,%d] taglevel [%d,%d] failed\n",
							 papuga_ResolveTypeName(resolvetype), itemid,
							 scope.from, scope.to, taglevelRange.first, taglevelRange.second);
					}
#endif
					// We try to get a valid node candidate preferring value nodes if defined
					while (resolvedObj.valid() && !addResolvedItemValue( sink, resolvedObj))
					{
						if (m_errcode != papuga_Ok) return false;
						resolvedObj = resolveNextSameScopeItem( itemid);
					}
					if (!resolvedObj.valid())
					{
						if (resolvetype == papuga_ResolveTypeRequired)
						{
							m_errcode = papuga_ValueUndefined;
							return false;
						}
						else
						{
							if (m_errcode != papuga_Ok) return false;
							sink.pushVoid();
							// ... we get here only if we found a value that matches, but was undefined
						}
					}
					ResolvedObject resolvedObjNext = resolveNextInsideItem( scope, taglevelRange, itemid);
					if (resolvedObjNext.valid())
					{
#ifdef PAPUGA_LOWLEVEL_DEBUG
						fprintf( stderr, "search %s item %d in scope [%d,%d] taglevel [%d,%d] found another in scope [%d,%d] taglevel %d\n",
							 papuga_ResolveTypeName(resolvetype), itemid,
							 scope.from, scope.to,
							 taglevelRange.first, taglevelRange.second,
							 resolvedObjNext.scope.from, resolvedObjNext.scope.to, resolvedObjNext.taglevel);
#endif
						m_errcode = papuga_AmbiguousReference;
						return false;
					}
				}
				break;
				case papuga_ResolveTypeInherited:
				{
					ResolvedObject resolvedObj = resolveNearItemCoveringScope( scope, taglevelRange, itemid);
#ifdef PAPUGA_LOWLEVEL_DEBUG
					if (resolvedObj.valid())
					{
						std::string objdescr = ObjectRef_descr( resolvedObj.objref);
						fprintf( stderr, "search %s item %d in scope [%d,%d] taglevel [%d,%d] found %s in scope [%d,%d] taglevel %d\n",
							 papuga_ResolveTypeName(resolvetype), itemid,
							 scope.from, scope.to,
							 taglevelRange.first, taglevelRange.second,
							 objdescr.c_str(),
							 resolvedObj.scope.from, resolvedObj.scope.to, resolvedObj.taglevel);
					}
					else
					{
						fprintf( stderr, "search %s item %d in scope [%d,%d] failed\n",
							 papuga_ResolveTypeName(resolvetype), itemid, scope.from, scope.to);
					}
#endif
					if (ObjectRef_is_value( resolvedObj.objref))
					{
						int valueidx = ObjectRef_value_id( resolvedObj.objref);
						if (!sink.pushValue( &m_ctx->values()[ valueidx].content))
						{
							m_errcode = papuga_NoMemError;
							return false;
						}
						if (hasNextCoveringItem( scope, taglevelRange, itemid))
						{
#ifdef PAPUGA_LOWLEVEL_DEBUG
							fprintf( stderr, "search %s item in scope [%d,%d] taglevel [%d,%d] found another in scope [%d,%d] taglevel %d\n",
								 papuga_ResolveTypeName(resolvetype),
								 scope.from, scope.to,
								 taglevelRange.first, taglevelRange.second,
								 resolvedObj.scope.from, resolvedObj.scope.to, resolvedObj.taglevel);
#endif
							m_errcode = papuga_AmbiguousReference;
							return false;
						}
					}
					else if (ObjectRef_is_struct( resolvedObj.objref))
					{
						m_errcode = papuga_InvalidAccess;
						return false;
					}
					else
					{
						m_errcode = papuga_ValueUndefined;
						return false;
					}
				}
				break;
				case papuga_ResolveTypeArray:
				{
					ResolvedObject resolvedObj = resolveNearItemInsideScope( scope, taglevelRange, itemid);
					if (!sink.openSerialization())
					{
						m_errcode = papuga_NoMemError;
						return false;
					}
#ifdef PAPUGA_LOWLEVEL_DEBUG
					int arrayElementCount = 0;
#endif
					while (resolvedObj.valid())
					{
						// We try to get a valid node candidate preferring value nodes if defined
						while (!addResolvedItemValue( sink, resolvedObj))
						{
							resolvedObj = resolveNextSameScopeItem( itemid);
							if (!resolvedObj.valid()) break;
						}
						if (!resolvedObj.valid())
						{
							if (m_errcode != papuga_Ok) return false;
							sink.pushVoid();
							// ... we get here only if we found a value that matches, but was undefined
						}
#ifdef PAPUGA_LOWLEVEL_DEBUG
						++arrayElementCount;
						std::string objdescr = ObjectRef_descr( resolvedObj.objref);
						fprintf( stderr, "search %s item %d in scope [%d,%d] taglevel [%d,%d] found %s in scope [%d,%d] taglevel %d\n",
							 papuga_ResolveTypeName(resolvetype), itemid,
							 scope.from, scope.to,
							 taglevelRange.first, taglevelRange.second,
							 objdescr.c_str(), resolvedObj.scope.from, resolvedObj.scope.to, resolvedObj.taglevel);
#endif
						resolvedObj = resolveNextInsideItem( scope, taglevelRange, itemid);
					}
#ifdef PAPUGA_LOWLEVEL_DEBUG
					fprintf( stderr, "search %s item %d in scope [%d,%d] taglevel [%d,%d] found %d elements\n", 
						 papuga_ResolveTypeName(resolvetype), itemid,
						 scope.from, scope.to,
						 taglevelRange.first, taglevelRange.second,
						 arrayElementCount);
#endif
					if (!sink.closeSerialization())
					{
						m_errcode = papuga_NoMemError;
						return false;
					}
				}
				break;
			}
			return true;
		}

		bool setCallArgValue( papuga_ValueVariant& arg, const CallArgDef& argdef, const Scope& scope, int taglevel, const papuga_RequestVariable* varlist)
		{
			if (argdef.varname)
			{
				papuga_RequestVariable const* vl = varlist;
				while (vl && 0!=std::strcmp(vl->name, argdef.varname)) vl=vl->next;
				if (!vl)
				{
					m_errcode = papuga_ValueUndefined;
					return false;
				}
				papuga_init_ValueVariant_value( &arg, &vl->value);
			}
			else
			{
				TagLevelRange tagLevelRange = getTagLevelRange( argdef.resolvetype, taglevel, argdef.max_tag_diff);
				ValueSink sink( &arg, &m_allocator);
				if (!resolveItem( sink, argdef.itemid, argdef.resolvetype, scope, tagLevelRange))
				{
					return false;
				}
			}
			return true;
		}

	private:
		papuga_RequestMethodCall m_curr_methodcall;
		const AutomatonContext* m_ctx;
		std::vector<ScopeObjItr> m_resolvers;
		int m_curr_methodidx;
		papuga_Allocator m_allocator;
		char m_allocator_membuf[ 4096];
		papuga_ErrorCode m_errcode;
	};

private:
#ifdef PAPUGA_LOWLEVEL_DEBUG
	void printScopeObjMap( std::ostream& out)
	{
		out << "scope object map:" << std::endl;
		std::vector<ScopeObjMap>::const_iterator ii = m_scopeobjmap.begin(), ie = m_scopeobjmap.end();
		int itemidx = 0;
		for (; ii != ie; ++ii,++itemidx)
		{
			if (!ii->empty())
			{
				ScopeObjMap::const_iterator mi = ii->begin(), me = ii->end();
				for (; mi != me; ++mi)
				{
					const ObjectDescr& objdescr = mi->second;
					const ScopeKey& key = mi->first;
					out << "\t"
						<< "item " << itemidx
						<< " prio " << (int)key.prio
						<< ", scope [" << key.from << "," << key.to << "] "
						<< ObjectRef_descr( objdescr.objref);
;
					if (ObjectRef_is_value( objdescr.objref))
					{
						int objindex = ObjectRef_value_id( objdescr.objref);
						if (papuga_ValueVariant_defined( &m_values[ objindex].content))
						{
							std::string valuestr = ValueVariant_tostring( m_values[ objindex].content, m_errcode);
							if (m_errcode != papuga_Ok) throw papuga::error_exception( m_errcode, "debug dump scope object map");
							out << " taglevel " << objdescr.taglevel << " value '" << valuestr << "'";
						}
						else
						{
							out << " value undefined";
						}
					}
					out << std::endl;
				}
			}
		}
	}
#endif

	bool processEvent( int ev, const papuga_ValueVariant* evalue)
	{
		// Process event depending on type:
		int evidx = AtmRef_index( ev);
		switch (AtmRef_type( ev))
		{
			case InstantiateValue:
			{
#ifdef PAPUGA_LOWLEVEL_DEBUG
				papuga_ErrorCode errcode = papuga_Ok;
				std::string evaluestr = ValueVariant_tostring( *evalue, errcode);
				fprintf( stderr, "process event [%s %d] value='%s' item id=%d taglevel=%d\n",
					 atmRefTypeName(InstantiateValue), evidx, evaluestr.c_str(),
					 m_atm->valuedefs()[ evidx].itemid, taglevel());
#endif
				papuga_ValueVariant evalue_copy;
				//PF:HACK: The const cast has no influence, as movehostobj parameter is false and the contents of evalue remain
				//	untouched, but it is still ugly and a bad hack:
				if (!papuga_Allocator_deepcopy_value( &m_allocator, &evalue_copy, const_cast<papuga_ValueVariant*>(evalue), false, &m_errcode)) return false;
				m_valuenodes.push_back( ValueNode( m_atm->valuedefs()[ evidx].itemid, &evalue_copy));
				break;
			}
			case CollectValue:
			{
				int itemid = m_atm->valuedefs()[ evidx].itemid;
#ifdef PAPUGA_LOWLEVEL_DEBUG
				fprintf( stderr, "process event [%s %d] item id=%d scope=[%d,%d] taglevel=%d\n",
					 atmRefTypeName( CollectValue), evidx, itemid, curscope().from, curscope().to, taglevel());
#endif
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
#ifdef PAPUGA_LOWLEVEL_DEBUG
						std::string vstr = vi->tostring();
						std::string objdescr = ObjectRef_descr( objref);
						fprintf( stderr, "collect value item id=%d %s scope=[%d,%d] taglevel=%d value='%s'\n",
							 itemid, objdescr.c_str(), curscope().from, curscope().to, taglevel(), vstr.c_str());
#endif
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
#ifdef PAPUGA_LOWLEVEL_DEBUG
					std::string objdescr = ObjectRef_descr( objref);
					fprintf( stderr, "collect void value item id=%d %s scope=[%d,%d] taglevel=%d\n",
						 itemid, objdescr.c_str(), curscope().from, curscope().to, taglevel());
#endif
				}
				else if (valuecnt > 1)
				{
					m_errcode = papuga_DuplicateDefinition;
					return false;
				}
				break;
			}
			case CloseStruct:
			{
#ifdef PAPUGA_LOWLEVEL_DEBUG
				fprintf( stderr, "process event [%s %d] item id=%d scope=[%d,%d] taglevel=%d\n",
					 atmRefTypeName(CloseStruct), evidx, m_atm->valuedefs()[ evidx].itemid,
					 curscope().from, curscope().to, taglevel());
#endif
				const StructDef* stdef = &m_atm->structdefs()[ evidx];
				insertObjectRef( stdef->itemid, ObjectScope, curscope(), taglevel(), ObjectRef_struct( m_structs.size()));
				m_structs.push_back( stdef);
				break;
			}
			case MethodCall:
			{
#ifdef PAPUGA_LOWLEVEL_DEBUG
				std::string calldefstr = m_atm->calldefs()[ evidx].tostring();
				fprintf( stderr, "process event [%s %d] method call: %s scope=[%d,%d] taglevel=%d\n",
					 atmRefTypeName(MethodCall), evidx, calldefstr.c_str(), curscope().from, curscope().to, taglevel());
#endif
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

private:
	typedef textwolf::XMLPathSelect<textwolf::charset::UTF8> AutomatonState;

	const AutomatonDescription* m_atm;
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
	bool m_done;
	papuga_ErrorCode m_errcode;
	EventStack m_event_stacks[ MaxAtmRefType+1];
};

}//anonymous namespace


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

extern "C" papuga_RequestAutomaton* papuga_create_RequestAutomaton( const papuga_ClassDef* classdefs, const papuga_StructInterfaceDescription* structdefs, const char* resultname)
{
	papuga_RequestAutomaton* rt = (papuga_RequestAutomaton*)std::calloc( 1, sizeof(*rt));
	if (!rt) return NULL;
	try
	{
		new (&rt->atm) AutomatonDescription( classdefs, resultname);
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

extern "C" bool papuga_RequestAutomaton_add_call(
		papuga_RequestAutomaton* self,
		const char* expression,
		const papuga_RequestMethodId* method,
		const char* selfvarname,
		const char* resultvarname,
		int nargs)
{
	if (method->functionid && !selfvarname) return false;
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

extern "C" bool papuga_RequestAutomaton_done( papuga_RequestAutomaton* self)
{
	return self->atm.done();
}

struct papuga_Request
{
	AutomatonContext ctx;
	const papuga_StructInterfaceDescription* structdefs;
};

extern "C" papuga_Request* papuga_create_Request( const papuga_RequestAutomaton* atm)
{
	papuga_Request* rt = (papuga_Request*)std::calloc( 1, sizeof(*rt));
	if (!rt) return NULL;
	try
	{
		new (&rt->ctx) AutomatonContext( &atm->atm);
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

struct papuga_RequestIterator
{
	AutomatonContext::Iterator itr;
};

extern "C" papuga_RequestIterator* papuga_create_RequestIterator( papuga_Allocator* allocator, const papuga_Request* request)
{
	papuga_RequestIterator* rt = (papuga_RequestIterator*)papuga_Allocator_alloc( allocator, sizeof(*rt), 0);
	if (!rt) return NULL;
	try
	{
		if (request->ctx.isDone())
		{
			new (&rt->itr) AutomatonContext::Iterator( &request->ctx);
		}
		else
		{
			new (&rt->itr) AutomatonContext::Iterator( papuga_ExecutionOrder);
		}
		return rt;
	}
	catch (...)
	{
		return NULL;
	}
}

extern "C" const papuga_RequestMethodCall* papuga_RequestIterator_next_call( papuga_RequestIterator* self, const papuga_RequestVariable* varlist)
{
	return self->itr.next( varlist);
}

papuga_ErrorCode papuga_RequestIterator_get_last_error( papuga_RequestIterator* self, const papuga_RequestMethodCall** call)
{
	*call = self->itr.lastCall();
	return self->itr.lastError();
}

extern "C" const char* papuga_Request_tostring( const papuga_Request* self, papuga_Allocator* allocator, papuga_ErrorCode* errcode)
{
	try
	{
		AutomatonContext::Iterator itr( &self->ctx);
		std::ostringstream out;
		const MethodCallNode* callnode = 0;
		while (0 != (callnode = itr.nextNode()))
		{
			if (!itr.printMethodCallNode( out, callnode)) break;
			out << std::endl;
		}
		if (*errcode == papuga_Ok)
		{
			*errcode = itr.lastError();
		}
		if (*errcode != papuga_Ok) return NULL;
		std::string rtbuf( out.str());
		const char* rt = papuga_Allocator_copy_string( allocator, rtbuf.c_str(), rtbuf.size());
		if (!rt)
		{
			*errcode = papuga_NoMemError;
		}
		return rt;
	}
	catch (const std::bad_alloc&)
	{
		*errcode = papuga_NoMemError;
		return NULL;
	}
	catch (...)
	{
		*errcode = papuga_UncaughtException;
		return NULL;
	}
}

extern "C" const char* papuga_Request_resultname( const papuga_Request* self)
{
	return self->ctx.resultname();
}

extern "C" const papuga_StructInterfaceDescription* papuga_Request_struct_descriptions( const papuga_Request* self)
{
	return self->structdefs;
}


