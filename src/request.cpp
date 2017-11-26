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
#include "papuga/allocator.h"
#include "papuga/valueVariant.h"
#include "papuga/callArgs.h"
#include "textwolf/xmlpathautomatonparse.hpp"
#include "textwolf/xmlpathselect.hpp"
#include "textwolf/charset.hpp"
#include <stdexcept>
#include <sstream>
#include <iostream>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <algorithm>

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
	bool inherited;

	StructMemberDef( const char* name_, int itemid_, bool inherited_)
		:name(name_),itemid(itemid_),inherited(inherited_){}
	StructMemberDef( const StructMemberDef& o)
		:name(o.name),itemid(o.itemid),inherited(o.inherited){}
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
	bool inherited;

	CallArgDef( const char* varname_, int itemid_, bool inherited_)
		:varname(varname_),itemid(itemid_),inherited(inherited_){}
	CallArgDef( const CallArgDef& o)
		:varname(o.varname),itemid(o.itemid),inherited(o.inherited){}
};

struct CallDef
{
	const char* classname;
	const char* methodname;
	const char* selfvarname;
	const char* resultvarname;
	CallArgDef* args;
	int nofargs;
	int groupid;

	CallDef( const char* classname_, const char* methodname_, const char* selfvarname_, const char* resultvarname_, CallArgDef* args_, int nofargs_, int groupid_)
		:classname(classname_),methodname(methodname_),selfvarname(selfvarname_),resultvarname(resultvarname_),args(args_),nofargs(nofargs_),groupid(groupid_){}
	CallDef( const CallDef& o)
		:classname(o.classname),methodname(o.methodname),selfvarname(o.selfvarname),resultvarname(o.resultvarname),args(o.args),nofargs(o.nofargs),groupid(o.groupid){}
};

typedef int AtmRef;
enum AtmRefType {InstantiateValue,CollectValue,CloseStruct,MethodCall};
static AtmRef AtmRef_get( AtmRefType type, int idx)	{return (AtmRef)(((int)type<<28) | idx);}
static AtmRefType AtmRef_type( AtmRef atmref)		{return (AtmRefType)((atmref>>28) & 0x7);}
static int AtmRef_index( AtmRef atmref)			{return ((int)atmref & (((int)atmref<<28)-1));}

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

		
/// @brief Description of the automaton
class AutomatonDescription
{
public:
	typedef textwolf::XMLPathSelectAutomatonParser<> XMLPathSelectAutomaton;

	AutomatonDescription()
		:m_calldefs(),m_structdefs(),m_valuedefs(),m_atm(),m_maxitemid(0),m_errcode(papuga_Ok),m_groupid(-1),m_done(false)
	{
		papuga_init_Allocator( &m_allocator, m_allocatorbuf, sizeof(m_allocatorbuf));
	}

	/* @param[in] expression selector of the call scope */
	/* @param[in] args {0,NULL} terminated list of arguments */
	bool addCall( const char* expression, const char* classname_, const char* methodname_, const char* selfvarname_, const char* resultvarname_, int nofargs)
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
			std::string open_expression( cut_trailing_slashes( expression));
			std::string close_expression( open_expression + "~");
			int mm = nofargs * sizeof(CallArgDef);
			CallArgDef* car = (CallArgDef*)papuga_Allocator_alloc( &m_allocator, mm, sizeof(CallArgDef));
			const char* classname = papuga_Allocator_copy_charp( &m_allocator, classname_);
			const char* methodname = papuga_Allocator_copy_charp( &m_allocator, methodname_);
			const char* selfvarname = papuga_Allocator_copy_charp( &m_allocator, selfvarname_);
			const char* resultvarname = papuga_Allocator_copy_charp( &m_allocator, resultvarname_);
			if (!car || !classname || !methodname || !selfvarname || !resultvarname)
			{
				m_errcode = papuga_NoMemError;
				return false;
			}
			std::memset( car, 0, mm);
			m_atm.addExpression( AtmRef_get( MethodCall, m_calldefs.size()), close_expression.c_str(), close_expression.size());
			m_calldefs.push_back( CallDef( classname, methodname, selfvarname, resultvarname, car, nofargs, m_groupid < 0 ? m_calldefs.size():m_groupid));
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
		return true;
	}

	bool setCallArgVar( int idx, const char* argvar)
	{
		if (!argvar)
		{
			m_errcode = papuga_TypeError;
			return false;
		}
		CallArgDef adef( papuga_Allocator_copy_charp( &m_allocator, argvar), 0, false);
		if (!adef.varname)
		{
			m_errcode = papuga_NoMemError;
			return false;
		}
		return setCallArg( idx, adef);
	}

	bool setCallArgItem( int idx, int itemid, bool inherited)
	{
		if (itemid <= 0)
		{
			m_errcode = papuga_TypeError;
			return false;
		}
		CallArgDef adef( 0, itemid, inherited);
		return setCallArg( idx, adef);
	}

	/* @param[in] expression selector of the structure (tag) */
	/* @param[in] itemid identifier for the item associated with this structure */
	/* @param[in] members {NULL,0} terminated list of members */
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
			StructMemberDef* mar = (StructMemberDef*)papuga_Allocator_alloc( &m_allocator, mm, sizeof(StructMemberDef));
			if (!mar)
			{
				m_errcode = papuga_NoMemError;
				return false;
			}
			std::memset( mar, 0, mm);
			m_atm.addExpression( AtmRef_get( CloseStruct, m_structdefs.size()), close_expression.c_str(), close_expression.size());
			m_structdefs.push_back( StructDef( itemid, mar, nofmembers));
		}
		CATCH_LOCAL_EXCEPTION(m_errcode,false)
		return true;
	}

	bool setMember( int idx, const char* name, int itemid, bool inherited)
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
		StructMemberDef& mdef = m_structdefs.back().members[ idx];
		if (mdef.itemid)
		{
			m_errcode = papuga_DuplicateDefinition;
			return false;
		}
		mdef.itemid = itemid;
		mdef.inherited = inherited;
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
			else
			{
				value_expression.append( open_expression + "/" + select_expression);
			}
			m_atm.addExpression( AtmRef_get( InstantiateValue, m_valuedefs.size()), value_expression.c_str(), value_expression.size());
			m_atm.addExpression( AtmRef_get( CollectValue, m_valuedefs.size()), close_expression.c_str(), close_expression.size());
			m_valuedefs.push_back( ValueDef( itemid));
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
static int ObjectRef_struct_id( ObjectRef objref)	{return objref < 0 ? -objref-1 : 0;}
static int ObjectRef_value_id( ObjectRef objref)	{return objref > 0 ? objref-1 : 0;}
static ObjectRef ObjectRef_value( int idx)		{return (ObjectRef)(idx+1);}
static ObjectRef ObjectRef_struct( int idx)		{return (ObjectRef)-(idx+1);}

struct Scope
{
	int from;
	int to;

	Scope( int from_, int to_)
		:from(from_),to(to_){}
	Scope( const Scope& o)
		:from(o.from),to(o.to){}

	bool operator<( const Scope& o) const
	{
		if (from == o.from)
		{
			return (to < o.to);
		}
		else
		{
			return (from < o.from);
		}
	}
	bool operator==( const Scope& o) const
	{
		return from == o.from && to == o.to;
	}
	bool operator!=( const Scope& o) const
	{
		return from != o.from || to != o.to;
	}
	bool inside( const Scope& o) const
	{
		return (from >= o.from && to <= o.to);
	}
};

struct Value
{
	papuga_ValueVariant content;

	Value()
	{
		papuga_init_ValueVariant( &content);
	}
	Value( const papuga_ValueVariant* content_)
	{
		papuga_init_ValueVariant_copy( &content, content_);
	}
	Value( const char* str, int size)
	{
		papuga_init_ValueVariant_string( &content, str, size);
	}
	Value( const Value& o)
	{
		papuga_init_ValueVariant_copy( &content, &o.content);
	}
};

struct ValueNode
{
	Value value;
	int itemid;

	ValueNode( int itemid_, const Value& value_)
		:value(value_),itemid(itemid_) {}
	ValueNode( const ValueNode& o)
		:value(o.value),itemid(o.itemid){}
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

	MethodCallNode( const CallDef* def_, const Scope& scope_, const MethodCallKey& key_)
		:MethodCallKey(key_),def(def_),scope(scope_){}
	MethodCallNode( const MethodCallNode& o)
		:MethodCallKey(o),def(o.def),scope(o.scope){}
};

static void papuga_init_RequestMethodCall( papuga_RequestMethodCall* self)
{
	self->classname = 0;
	self->methodname = 0;
	papuga_init_CallArgs( &self->args, self->membuf, sizeof(self->membuf));
}

class AutomatonContext
{
public:
	explicit AutomatonContext( const AutomatonDescription* atm_)
		:m_atm(atm_),m_atmstate(&atm_->atm()),scopecnt(0),m_scopestack()
		,m_valuenodes(),m_values(),m_structs(),m_resolvers( atm_->maxitemid()+1, ScopeObjResolver()),m_methodcalls(),m_variablemap()
		,m_curr_methodidx(0),m_errcode(papuga_Ok)
	{
		papuga_init_Allocator( &m_allocator, m_allocator_membuf, sizeof(m_allocator_membuf));
		papuga_init_RequestMethodCall( &m_curr_methodcall);
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

	bool setVariableValue( const char* name, const papuga_ValueVariant* value)
	{
		try
		{
			m_variablemap[ name] = Value( value);
			return true;
		}
		CATCH_LOCAL_EXCEPTION(m_errcode,false)
	}

	bool processOpenTag( const char* namestr, std::size_t namesize)
	{
		try
		{
			AutomatonState::iterator itr = m_atmstate.push( textwolf::XMLScannerBase::OpenTag, namestr, namesize);
			for (; *itr; ++itr) processEvent( *itr, namestr, namesize);
			m_scopestack.push_back( scopecnt);
			return true;
		}
		CATCH_LOCAL_EXCEPTION(m_errcode,false)
	}

	bool processAttributeName( const char* namestr, std::size_t namesize)
	{
		try
		{
			AutomatonState::iterator itr = m_atmstate.push( textwolf::XMLScannerBase::TagAttribName, namestr, namesize);
			for (; *itr; ++itr) processEvent( *itr, namestr, namesize);
			return true;
		}
		CATCH_LOCAL_EXCEPTION(m_errcode,false)
	}
	bool processAttributeValue( const char* valuestr, std::size_t valuesize)
	{
		try
		{
			AutomatonState::iterator itr = m_atmstate.push( textwolf::XMLScannerBase::TagAttribValue, valuestr, valuesize);
			for (; *itr; ++itr) processEvent( *itr, valuestr, valuesize);
			return true;
		}
		CATCH_LOCAL_EXCEPTION(m_errcode,false)
	}

	bool processValue( const char* valuestr, std::size_t valuesize)
	{
		try
		{
			AutomatonState::iterator itr = m_atmstate.push( textwolf::XMLScannerBase::Content, valuestr, valuesize);
			for (; *itr; ++itr) processEvent( *itr, valuestr, valuesize);
			return true;
		}
		CATCH_LOCAL_EXCEPTION(m_errcode,false)
	}

	bool processCloseTag()
	{
		try
		{
			AutomatonState::iterator itr = m_atmstate.push( textwolf::XMLScannerBase::CloseTag, "", 0);
			for (; *itr; ++itr) processEvent( *itr, "", 0);
			m_scopestack.pop_back();
			return true;
		}
		CATCH_LOCAL_EXCEPTION(m_errcode,false)
	}

	papuga_RequestMethodCall* nextMethodCall()
	{
		try
		{
			if (m_curr_methodidx >= (int)m_methodcalls.size())
			{
				return NULL;
			}
			if (m_curr_methodidx == 0)
			{
				std::sort( m_methodcalls.begin(), m_methodcalls.end());
			}
			const MethodCallNode& mcnode = m_methodcalls[ m_curr_methodidx++];
			m_curr_methodcall.classname = mcnode.def->classname;
			m_curr_methodcall.methodname = mcnode.def->methodname;
			papuga_init_CallArgs( &m_curr_methodcall.args, m_curr_methodcall.membuf, sizeof(m_curr_methodcall.membuf));
			if (set_callArgs( &m_curr_methodcall.args, mcnode))
			{
				return &m_curr_methodcall;
			}
			else
			{
				m_errcode = m_curr_methodcall.args.errcode;
				return NULL;
			}
			
		}
		CATCH_LOCAL_EXCEPTION(m_errcode,NULL)
	}

private:
	ObjectRef resolveNearItemCoveringScope( const Scope& scope, int itemid)
	{
		// Seek backwards for scope overlapping search scope: 
		ScopeObjResolver& resolver = m_resolvers[ itemid];
		ScopeObjMap::const_iterator it = resolver.current;

		if (it == resolver.map.end())
		{
			if (resolver.map.empty()) return 0;
			--it;
		}
		while (it->first.from > scope.from)
		{
			if (it == resolver.map.begin()) return 0;
			--it;
		}
		if (it->first.to >= scope.to)
		{
			return it->second;
		}
		else
		{
			while (it != resolver.map.begin())
			{
				--it;
				if (it->first.to > scope.from) break;
				if (it->first.to >= scope.to)
				{
					resolver.current = it;
					return it->second;
				}
			}
		}
		return 0;
	}

	ObjectRef resolveNearItemInsideScope( const Scope& scope, int itemid)
	{
		// Seek forwards for scope overlapped by search scope:
		ScopeObjResolver& resolver = m_resolvers[ itemid];
		ScopeObjMap::const_iterator it = resolver.current;
		if (it == resolver.map.end()) return 0;

		if (it->first.from >= scope.from)
		{
			if (it->first.to <= scope.to)
			{
				return it->second;
			}
			else
			{
				for (++it; it != resolver.map.end(); ++it)
				{
					if (it->first.from > scope.to) break;
					if (it->first.to <= scope.to)
					{
						resolver.current = it;
						return it->second;
					}
				}
			}
		}
		return 0;
	}

	void setResolverUpperBound( const Scope& scope, int itemid)
	{
		ScopeObjResolver& resolver = m_resolvers[ itemid];
		if (resolver.current == resolver.map.end())
		{
			resolver.current = resolver.map.lower_bound( scope);
		}
		else if (resolver.current->first.from < scope.from)
		{
			while (resolver.current->first.from < scope.from)
			{
				++resolver.current;
				if (resolver.current == resolver.map.end()) return;
			}
		}
		else
		{
			while (resolver.current->first.from >= scope.from)
			{
				if (resolver.current == resolver.map.begin()) return;
				--resolver.current;
			}
			++resolver.current;
		}
	}

	ObjectRef resolveNextItem( const Scope& scope, int itemid)
	{
		ScopeObjResolver& resolver = m_resolvers[ itemid];
		if (resolver.current == resolver.map.end()) return 0;
		int nextstart = resolver.current->first.to+1;
		for (;;)
		{
			++resolver.current;
			if (resolver.current == resolver.map.end() || resolver.current->first.from > scope.to) return 0;
			if (resolver.current->first.from >= nextstart && resolver.current->first.inside( scope)) return resolver.current->second;
		}
	}

	bool add_structure_member( papuga_Serialization* ser, const Scope& scope, const char* name, ObjectRef objref)
	{
		bool rt = true;
		if (ObjectRef_is_value( objref))
		{
			int valueidx = ObjectRef_value_id( objref);
			if (name)
			{
				if (papuga_ValueVariant_defined( &m_values[ valueidx].content))
				{
					rt &= papuga_Serialization_pushName_charp( ser, name);
					rt &= papuga_Serialization_pushValue( ser, &m_values[ valueidx].content);
				}
			}
			else
			{
				if (papuga_ValueVariant_defined( &m_values[ valueidx].content))
				{
					rt &= papuga_Serialization_pushValue( ser, &m_values[ valueidx].content);
				}
			}
		}
		else if (ObjectRef_is_struct( objref))
		{
			int structidx = ObjectRef_struct_id( objref);
			if (name)
			{
				rt &= papuga_Serialization_pushName_charp( ser, name);
			}
			rt &= papuga_Serialization_pushOpen( ser);
			rt &= build_structure( ser, scope, structidx);
			rt &= papuga_Serialization_pushClose( ser);
		}
		else
		{
			m_errcode = papuga_ValueUndefined;
			return false;
		}
		return rt;
	}

	bool build_structure( papuga_Serialization* ser, const Scope& scope, int structidx)
	{
		bool rt = true;
		const StructDef* stdef = m_structs[ structidx];
		int mi = 0, me = stdef->nofmembers;
		for (; mi != me; ++mi)
		{
			int itemid = stdef->members[ mi].itemid;
			ScopeObjResolver& resolver = m_resolvers[ itemid];
			setResolverUpperBound( scope, itemid);

			if (stdef->members[ mi].inherited)
			{
				ObjectRef objref = resolveNearItemCoveringScope( scope, itemid);
				if (objref)
				{
					if (ObjectRef_is_value(objref))
					{
						rt &= add_structure_member( ser,  scope, stdef->members[ mi].name, objref);
					}
					else
					{
						m_errcode = papuga_InvalidAccess;
						return false;
					}
				}
				else
				{
					m_errcode = papuga_ValueUndefined;
					return false;
				}
			}
			else
			{
				ObjectRef objref = resolveNearItemInsideScope( scope, itemid);
				if (objref) while (objref)
				{
					const Scope& subscope( resolver.current->first);
					if (subscope != scope)
					{
						rt &= add_structure_member( ser, subscope, stdef->members[ mi].name, objref);
					}
					objref = resolveNextItem( scope, itemid);
				}
				else
				{
					m_errcode = papuga_ValueUndefined;
					return false;
				}
			}
		}
		return rt;
	}

	bool set_callArgs( papuga_CallArgs* args, const MethodCallNode& mcnode)
	{
		const CallDef* mcdef = mcnode.def;
		int ai = 0, ae = mcdef->nofargs;
		for (; ai != ae; ++ai)
		{
			const CallArgDef& argdef = mcdef->args[ai];
			papuga_ValueVariant& arg = args->argv[ai];
			if (argdef.varname)
			{
				VariableMap::const_iterator vi = m_variablemap.find( argdef.varname);
				if (vi == m_variablemap.end())
				{
					m_errcode = papuga_ValueUndefined;
					return false;
				}
				papuga_init_ValueVariant_copy( &arg, &vi->second.content);
			}
			else
			{
				setResolverUpperBound( mcnode.scope, argdef.itemid);
				ObjectRef objref = (argdef.inherited)
						? resolveNearItemCoveringScope( mcnode.scope, argdef.itemid)
						: resolveNearItemInsideScope( mcnode.scope, argdef.itemid);
				if (ObjectRef_is_value( objref))
				{
					int valueidx = ObjectRef_value_id( objref);
					papuga_init_ValueVariant_copy( &arg, &m_values[ valueidx].content);
				}
				else if (ObjectRef_is_struct( objref))
				{
					int structidx = ObjectRef_struct_id( objref);
					papuga_Serialization* ser = papuga_Allocator_alloc_Serialization( &args->allocator);
					papuga_init_ValueVariant_serialization( &arg, ser);
					if (!build_structure( ser, mcnode.scope, structidx)) return false;
				}
				else
				{
					m_errcode = papuga_ValueUndefined;
					return false;
				}
			}
		}
		return true;
	}

	bool processEvent( int ev, const char* valuestr, std::size_t valuesize)
	{
		int evidx = AtmRef_index( ev);
		++scopecnt;
		switch (AtmRef_type( ev))
		{
			case InstantiateValue:
			{
				char* valuestrcopy = papuga_Allocator_copy_string( &m_allocator, valuestr, valuesize);
				m_valuenodes.push_back( ValueNode( m_atm->valuedefs()[ evidx].itemid, Value( valuestrcopy, valuesize)));
				break;
			}
			case CollectValue:
			{
				int itemid = m_atm->valuedefs()[ evidx].itemid;
				std::vector<ValueNode>::iterator vi = m_valuenodes.begin(), ve = m_valuenodes.end();
				int valuecnt = 0;
				while (vi != ve)
				{
					if (vi->itemid == itemid)
					{
						m_resolvers[ itemid].insert( m_scopestack.back(), scopecnt, ObjectRef_value( m_values.size()));
						m_values.push_back( vi->value);
						m_valuenodes.erase( vi);
						++valuecnt;
					}
					else
					{
						++vi;
					}
				}
				if (valuecnt == 0)
				{
					m_resolvers[ itemid].insert( m_scopestack.back(), scopecnt, ObjectRef_value( m_values.size()));
					m_values.push_back( Value());
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
				const StructDef* stdef = &m_atm->structdefs()[ evidx];
				int itemid = stdef->itemid;
				m_resolvers[ itemid].insert( m_scopestack.back(), scopecnt, ObjectRef_struct( m_structs.size()));
				m_structs.push_back( stdef);
			}
			case MethodCall:
			{
				const CallDef* calldef = &m_atm->calldefs()[ evidx];
				MethodCallKey key( calldef->groupid, scopecnt, evidx);
				Scope scope( m_scopestack.back(), scopecnt);
				m_methodcalls.push_back( MethodCallNode( calldef, scope, key));
				if (m_curr_methodidx > 0)
				{
					m_errcode = papuga_ExecutionOrder;
					return false;
				}
			}
		}
		return true;
	}

private:
	typedef std::pair<Scope,ObjectRef> ScopeObjElem;
	typedef std::map<Scope,ObjectRef> ScopeObjMap;
	struct ScopeObjResolver
	{
		ScopeObjMap map;
		ScopeObjMap::const_iterator current;

		ScopeObjResolver()
			:map(),current(){}
		ScopeObjResolver( const ScopeObjResolver& o)
			:map(o.map),current(){}

		void insert( int scopebegin, int scopeend, const ObjectRef& objref)
		{
			map.insert( ScopeObjElem( Scope( scopebegin, scopeend), objref));
			current = map.end();
		}
	};

	typedef textwolf::XMLPathSelect<textwolf::charset::UTF8> AutomatonState;
	typedef std::map<std::string,Value> VariableMap;

	const AutomatonDescription* m_atm;
	AutomatonState m_atmstate;
	int scopecnt;
	papuga_Allocator m_allocator;
	char m_allocator_membuf[ 4096];
	std::vector<int> m_scopestack;
	std::vector<ValueNode> m_valuenodes;
	std::vector<Value> m_values;
	std::vector<const StructDef*> m_structs;
	std::vector<ScopeObjResolver> m_resolvers;
	std::vector<MethodCallNode> m_methodcalls;
	VariableMap m_variablemap;
	papuga_RequestMethodCall m_curr_methodcall;
	int m_curr_methodidx;
	papuga_ErrorCode m_errcode;
};

}//anonymous namespace



struct papuga_RequestAutomaton
{
	AutomatonDescription atm;
};

extern "C" papuga_RequestAutomaton* papuga_create_RequestAutomaton()
{
	papuga_RequestAutomaton* rt = (papuga_RequestAutomaton*)std::calloc( 1, sizeof(*rt));
	if (!rt) return NULL;
	try
	{
		new (&rt->atm) AutomatonDescription();
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
		const char* classname,
		const char* methodname,
		const char* selfvarname,
		const char* resultvarname,
		int nargs)
{
	return self->atm.addCall( expression, classname, methodname, selfvarname, resultvarname, nargs);
}

extern "C" bool papuga_RequestAutomaton_set_call_arg_var( papuga_RequestAutomaton* self, int idx, const char* varname)
{
	return self->atm.setCallArgVar( idx, varname);
}

extern "C" bool papuga_RequestAutomaton_set_call_arg_item( papuga_RequestAutomaton* self, int idx, int itemid, bool inherited)
{
	return self->atm.setCallArgItem( idx, itemid, inherited);
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
		bool inherited)
{
	return self->atm.setMember( idx, name, itemid, inherited);
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
};

extern "C" papuga_Request* papuga_create_Request( const papuga_RequestAutomaton* atm)
{
	papuga_Request* rt = (papuga_Request*)std::calloc( 1, sizeof(*rt));
	if (!rt) return NULL;
	try
	{
		new (&rt->ctx) AutomatonContext( &atm->atm);
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

extern "C" bool papuga_Request_set_variable_value( papuga_Request* self, const char* varname, const papuga_ValueVariant* value)
{
	return self->ctx.setVariableValue( varname, value);
}

extern "C" bool papuga_Request_feed_open_tag( papuga_Request* self, const char* tagname, size_t tagnamesize)
{
	return self->ctx.processOpenTag( tagname, tagnamesize);
}

extern "C" bool papuga_Request_feed_close_tag( papuga_Request* self)
{
	return self->ctx.processCloseTag();
}

extern "C" bool papuga_Request_feed_attribute_name( papuga_Request* self, const char* attrname, size_t attrnamesize)
{
	return self->ctx.processAttributeName( attrname, attrnamesize);
}

extern "C" bool papuga_Request_feed_attribute_value( papuga_Request* self, const char* valueptr, size_t valuesize)
{
	return self->ctx.processAttributeValue( valueptr, valuesize);
}

extern "C" bool papuga_Request_feed_content_value( papuga_Request* self, const char* valueptr, size_t valuesize)
{
	return self->ctx.processValue( valueptr, valuesize);
}

extern "C" papuga_ErrorCode papuga_Request_last_error( const papuga_Request* self)
{
	return self->ctx.lastError();
}

extern "C" papuga_RequestMethodCall* papuga_Request_next_call( papuga_Request* self)
{
	return self->ctx.nextMethodCall();
}

