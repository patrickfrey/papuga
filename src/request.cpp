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
	const char* defaultvalue;

	CallArgDef( const char* varname_, int itemid_, bool inherited_, const char* defaultvalue_)
		:varname(varname_),itemid(itemid_),inherited(inherited_),defaultvalue(defaultvalue_){}
	CallArgDef( const CallArgDef& o)
		:varname(o.varname),itemid(o.itemid),inherited(o.inherited),defaultvalue(o.defaultvalue){}
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
				out << args[ai].itemid << (args[ai].inherited?" inherited":"");
			}
		}
		out << ");";
		return out.str();
	}
#endif
};

typedef int AtmRef;
enum AtmRefType {InstantiateValue,CollectValue,CloseStruct,MethodCall};
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

	explicit AutomatonDescription( const papuga_ClassDef* classdefs_)
		:m_classdefs(classdefs_),m_nof_classdefs(nofClassDefs(classdefs_))
		,m_calldefs(),m_structdefs(),m_valuedefs(),m_atm(),m_maxitemid(0)
		,m_errcode(papuga_Ok),m_groupid(-1),m_done(false)
	{
		papuga_init_Allocator( &m_allocator, m_allocatorbuf, sizeof(m_allocatorbuf));
	}

	const papuga_ClassDef* classdefs() const
	{
		return m_classdefs;
	}
	int nof_classdefs() const
	{
		return m_nof_classdefs;
	}

	/* @param[in] expression selector of the call scope */
	/* @param[in] nofargs number of arguments */
	bool addCall( const char* expression, const papuga_RequestMethodId* method_, const char* selfvarname_, const char* resultvarname_, int nofargs)
	{
		try
		{
			const char* methodname;
			const char* classname;
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
					expression, classname, methodname, selfvarname_, resultvarname_, nofargs);
#endif
			std::string open_expression( cut_trailing_slashes( expression));
			std::string close_expression( open_expression + "~");
			int mm = nofargs * sizeof(CallArgDef);
			CallArgDef* car = (CallArgDef*)papuga_Allocator_alloc( &m_allocator, mm, 0);
			const char* selfvarname = papuga_Allocator_copy_charp( &m_allocator, selfvarname_);
			const char* resultvarname = papuga_Allocator_copy_charp( &m_allocator, resultvarname_);
			if (!car || !selfvarname || !resultvarname)
			{
				m_errcode = papuga_NoMemError;
				return false;
			}
			std::memset( car, 0, mm);
			m_atm.addExpression( AtmRef_get( MethodCall, m_calldefs.size()), close_expression.c_str(), close_expression.size());
#ifdef PAPUGA_LOWLEVEL_DEBUG
			fprintf( stderr, "automaton add event call expression='%s' [%d,%d]\n", close_expression.c_str(), (int)MethodCall, (int)m_calldefs.size());
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
			adef.defaultvalue =adef_.defaultvalue;
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
			CallArgDef adef( papuga_Allocator_copy_charp( &m_allocator, argvar), 0, false, NULL);
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

	bool setCallArgItem( int idx, int itemid, bool inherited, const char* defaultvalue)
	{
		try
		{
#ifdef PAPUGA_LOWLEVEL_DEBUG
			fprintf( stderr, "automaton set call argument itemid=%d%s\n", itemid, inherited ? ", inherited":"");
#endif
			if (itemid <= 0)
			{
				m_errcode = papuga_TypeError;
				return false;
			}
			CallArgDef adef( 0, itemid, inherited, defaultvalue ? papuga_Allocator_copy_charp( &m_allocator, defaultvalue) : NULL);
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
			fprintf( stderr, "automaton add structure expression='%s', itemid=%d, nofmembers=%d\n", expression, itemid, nofmembers);
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
			StructMemberDef* mar = (StructMemberDef*)papuga_Allocator_alloc( &m_allocator, mm, sizeof(StructMemberDef));
			if (!mar)
			{
				m_errcode = papuga_NoMemError;
				return false;
			}
			std::memset( mar, 0, mm);
			m_atm.addExpression( AtmRef_get( CloseStruct, m_structdefs.size()), close_expression.c_str(), close_expression.size());
#ifdef PAPUGA_LOWLEVEL_DEBUG
			fprintf( stderr, "automaton add event structure close expression='%s' [%d,%d]\n", close_expression.c_str(), (int)CloseStruct, (int)m_structdefs.size());
#endif
			m_structdefs.push_back( StructDef( itemid, mar, nofmembers));
		}
		CATCH_LOCAL_EXCEPTION(m_errcode,false)
		return true;
	}

	bool setMember( int idx, const char* name, int itemid, bool inherited)
	{
		try
		{
	#ifdef PAPUGA_LOWLEVEL_DEBUG
			fprintf( stderr, "automaton set structure member name='%s', itemid=%d%s\n", name, itemid, inherited ? ", inherited":"");
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
			fprintf( stderr, "automaton add value scope='%s', select='%s', itemid=%d\n", scope_expression, select_expression, itemid);
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
			m_atm.addExpression( AtmRef_get( InstantiateValue, m_valuedefs.size()), value_expression.c_str(), value_expression.size());
			m_atm.addExpression( AtmRef_get( CollectValue, m_valuedefs.size()), close_expression.c_str(), close_expression.size());
#ifdef PAPUGA_LOWLEVEL_DEBUG
			fprintf( stderr, "automaton add event instantiate value expression='%s' [%d,%d]\n", value_expression.c_str(), (int)InstantiateValue, (int)m_valuedefs.size());
			fprintf( stderr, "automaton add event collect value expression='%s' [%d,%d]\n", close_expression.c_str(), (int)CollectValue, (int)m_valuedefs.size());
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
	self->selfvarname = 0;
	self->resultvarname = 0;
	self->methodid.classid = -1;
	self->methodid.functionid  = -1;
	self->eventcnt = 0;
	papuga_init_CallArgs( &self->args, self->membuf, sizeof(self->membuf));
}

class AutomatonContext
{
public:
	explicit AutomatonContext( const AutomatonDescription* atm_)
		:m_atm(atm_),m_atmstate(&atm_->atm()),scopecnt(0),m_scopestack()
		,m_valuenodes(),m_values(),m_structs(),m_scopeobjmap( atm_->maxitemid()+1, ScopeObjMap()),m_methodcalls()
		,m_done(false),m_errcode(papuga_Ok)
	{
		papuga_init_Allocator( &m_allocator, m_allocator_membuf, sizeof(m_allocator_membuf));
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

	struct LocalBuffer
	{
		char numbuf[ 128];
		const char* valuestr;
		std::size_t valuesize;
		papuga_ValueVariant value;
		bool deepcopy;

		LocalBuffer( const papuga_ValueVariant* value_, papuga_Allocator& allocator, papuga_ErrorCode& errcode)
			:deepcopy(true)
		{
			if (!papuga_ValueVariant_isatomic( value_))
			{
				papuga_init_ValueVariant( &value);
				valuestr = NULL;
				valuesize = 0;
				errcode = papuga_AtomicValueExpected;
			}
			else if (papuga_ValueVariant_isstring( value_))
			{
				valuestr = papuga_ValueVariant_tostring( value_, &allocator, &valuesize, &errcode);
				if (!valuestr)
				{
					valuesize = 0;
					return;
				}
				papuga_init_ValueVariant_string( &value, valuestr, valuesize);
				deepcopy = (value.value.string == value_->value.string);
			}
			else
			{
				valuestr = papuga_ValueVariant_toascii( numbuf, sizeof(numbuf), value_);
				if (!valuestr)
				{
					papuga_init_ValueVariant( &value);
					valuesize = 0;
					errcode = papuga_BufferOverflowError;
					return;
				}
				valuesize = strlen(valuestr);
				papuga_init_ValueVariant_copy( &value, value_);
			}
		}
	};

	bool processOpenTag( const papuga_ValueVariant* tagname)
	{
		try
		{
#ifdef PAPUGA_LOWLEVEL_DEBUG
			papuga_ErrorCode errcode = papuga_Ok;
			std::string tagnamestr = ValueVariant_tostring( *tagname, errcode);
			fprintf( stderr, "process open tag '%s'\n", tagnamestr.c_str());
#endif
			LocalBuffer localbuf( tagname, m_allocator, m_errcode);
			if (!localbuf.valuestr) return false;
			AutomatonState::iterator itr = m_atmstate.push( textwolf::XMLScannerBase::OpenTag, localbuf.valuestr, localbuf.valuesize);
			for (; *itr; ++itr) processEvent( *itr, &localbuf.value, localbuf.deepcopy);
			m_scopestack.push_back( scopecnt);
			return true;
		}
		CATCH_LOCAL_EXCEPTION(m_errcode,false)
	}

	bool processAttributeName( const papuga_ValueVariant* attrname)
	{
		try
		{
#ifdef PAPUGA_LOWLEVEL_DEBUG
			papuga_ErrorCode errcode = papuga_Ok;
			std::string attrnamestr = ValueVariant_tostring( *attrname, errcode);
			fprintf( stderr, "process attribute name '%s'\n", attrnamestr.c_str());
#endif
			LocalBuffer localbuf( attrname, m_allocator, m_errcode);
			if (!localbuf.valuestr) return false;
			AutomatonState::iterator itr = m_atmstate.push( textwolf::XMLScannerBase::TagAttribName, localbuf.valuestr, localbuf.valuesize);
			for (; *itr; ++itr) processEvent( *itr, &localbuf.value, localbuf.deepcopy);
			return true;
		}
		CATCH_LOCAL_EXCEPTION(m_errcode,false)
	}
	bool processAttributeValue( const papuga_ValueVariant* value)
	{
		try
		{
#ifdef PAPUGA_LOWLEVEL_DEBUG
			papuga_ErrorCode errcode = papuga_Ok;
			std::string valuestr = ValueVariant_tostring( *value, errcode);
			fprintf( stderr, "process attribute value '%s'\n", valuestr.c_str());
#endif
			LocalBuffer localbuf( value, m_allocator, m_errcode);
			if (!localbuf.valuestr) return false;
			AutomatonState::iterator itr = m_atmstate.push( textwolf::XMLScannerBase::TagAttribValue, localbuf.valuestr, localbuf.valuesize);
			for (; *itr; ++itr) processEvent( *itr, &localbuf.value, localbuf.deepcopy);
			return true;
		}
		CATCH_LOCAL_EXCEPTION(m_errcode,false)
	}

	bool processValue( const papuga_ValueVariant* value)
	{
		try
		{
#ifdef PAPUGA_LOWLEVEL_DEBUG
			papuga_ErrorCode errcode = papuga_Ok;
			std::string valuestr = ValueVariant_tostring( *value, errcode);
			fprintf( stderr, "process value '%s'\n", valuestr.c_str());
#endif
			LocalBuffer localbuf( value, m_allocator, m_errcode);
			if (!localbuf.valuestr) return false;
			AutomatonState::iterator itr = m_atmstate.push( textwolf::XMLScannerBase::Content, localbuf.valuestr, localbuf.valuesize);
			for (; *itr; ++itr) processEvent( *itr, &localbuf.value, localbuf.deepcopy);
			return true;
		}
		CATCH_LOCAL_EXCEPTION(m_errcode,false)
	}

	bool processCloseTag()
	{
		try
		{
#ifdef PAPUGA_LOWLEVEL_DEBUG
			fprintf( stderr, "process close tag\n");
#endif
			static Value empty;
			AutomatonState::iterator itr = m_atmstate.push( textwolf::XMLScannerBase::CloseTag, "", 0);
			for (; *itr; ++itr) processEvent( *itr, &empty.content, true);
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
			std::sort( m_methodcalls.begin(), m_methodcalls.end());
			return m_done = true;
		}
		CATCH_LOCAL_EXCEPTION(m_errcode,false)
	}

	typedef std::pair<Scope,ObjectRef> ScopeObjElem;
	typedef std::map<Scope,ObjectRef> ScopeObjMap;
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
	bool isDone() const
	{
		return m_done;
	}
	const papuga_ClassDef* classdefs() const
	{
		return m_atm->classdefs();
	}
	int nof_classdefs() const
	{
		return m_atm->nof_classdefs();
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
			if (!mcnode) return NULL;
			m_curr_methodcall.selfvarname = mcnode->def->selfvarname;
			m_curr_methodcall.resultvarname = mcnode->def->resultvarname;
			m_curr_methodcall.methodid.classid = mcnode->def->methodid.classid;
			m_curr_methodcall.methodid.functionid = mcnode->def->methodid.functionid;
			m_curr_methodcall.eventcnt = mcnode->scope.from;
			papuga_init_CallArgs( &m_curr_methodcall.args, m_curr_methodcall.membuf, sizeof(m_curr_methodcall.membuf));
			if (setCallArgs( &m_curr_methodcall.args, *mcnode, varlist))
			{
				++m_curr_methodidx;
				return &m_curr_methodcall;
			}
			else
			{
				return NULL;
			}
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
				papuga_ValueVariant argval;
				const CallArgDef& argdef = mcdef->args[ai];
				if (argdef.varname)
				{
					out << "\t$" << argdef.varname << std::endl;
				}
				else
				{
					if (!initResolvedItemValue( argval, &m_allocator, argdef.itemid, argdef.inherited, mcnode->scope, argdef.defaultvalue))
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
		papuga_Allocator* allocator()
		{
			return &m_allocator;
		}

	private:
		ObjectRef resolveNearItemCoveringScope( const Scope& scope, int itemid)
		{
			// Seek backwards for scope overlapping search scope: 
			const ScopeObjMap& objmap = m_ctx->scopeobjmap()[ itemid];
			ScopeObjMap::const_iterator it = m_resolvers[ itemid];
	
			if (it == objmap.end())
			{
				if (objmap.empty()) return 0;
				--it;
			}
			while (it->first.from > scope.from)
			{
				if (it == objmap.begin()) return 0;
				--it;
			}
			if (it->first.to >= scope.to)
			{
				return it->second;
			}
			else
			{
				while (it != objmap.begin())
				{
					--it;
					if (it->first.to > scope.from) break;
					if (it->first.to >= scope.to)
					{
						m_resolvers[ itemid] = it;
						return it->second;
					}
				}
			}
			return 0;
		}
	
		ObjectRef resolveNearItemInsideScope( const Scope& scope, int itemid)
		{
			// Seek forwards for scope overlapped by search scope:
			const ScopeObjMap& objmap = m_ctx->scopeobjmap()[ itemid];
			ScopeObjMap::const_iterator it = m_resolvers[ itemid];
			if (it == objmap.end()) return 0;
	
			if (it->first.from >= scope.from)
			{
				if (it->first.to <= scope.to)
				{
					return it->second;
				}
				else
				{
					for (++it; it != objmap.end(); ++it)
					{
						if (it->first.from > scope.to) break;
						if (it->first.to <= scope.to)
						{
							m_resolvers[ itemid] = it;
							return it->second;
						}
					}
				}
			}
			return 0;
		}
	
		void setResolverUpperBound( const Scope& scope, int itemid)
		{
			const ScopeObjMap& objmap = m_ctx->scopeobjmap()[ itemid];
			ScopeObjMap::const_iterator it = m_resolvers[ itemid];

			if (it == objmap.end())
			{
				Scope searchScope( scope.from, 0);
				m_resolvers[ itemid] = objmap.lower_bound( searchScope);
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
	
		ObjectRef resolveNextItem( const Scope& scope, int itemid)
		{
			const ScopeObjMap& objmap = m_ctx->scopeobjmap()[ itemid];
			ScopeObjMap::const_iterator& curitr = m_resolvers[ itemid];

			if (curitr == objmap.end()) return 0;
			int nextstart = curitr->first.to+1;
			for (;;)
			{
				++curitr;
				if (curitr == objmap.end() || curitr->first.from > scope.to) return 0;
				if (curitr->first.from >= nextstart && curitr->first.inside( scope)) return curitr->second;
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
					if (papuga_ValueVariant_defined( &m_ctx->values()[ valueidx].content))
					{
						rt &= papuga_Serialization_pushName_charp( ser, name);
						rt &= papuga_Serialization_pushValue( ser, &m_ctx->values()[ valueidx].content);
					}
				}
				else
				{
					if (papuga_ValueVariant_defined( &m_ctx->values()[ valueidx].content))
					{
						rt &= papuga_Serialization_pushValue( ser, &m_ctx->values()[ valueidx].content);
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
			const StructDef* stdef = m_ctx->structs()[ structidx];
			int mi = 0, me = stdef->nofmembers;
			for (; mi != me; ++mi)
			{
				int itemid = stdef->members[ mi].itemid;
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
						const Scope& subscope( m_resolvers[ itemid]->first);
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
	
		bool initResolvedItemValue( papuga_ValueVariant& arg, papuga_Allocator* allocator, int itemid, bool inherited, const Scope& scope, const char* defaultvalue)
		{
			setResolverUpperBound( scope, itemid);
			ObjectRef objref = (inherited) ? resolveNearItemCoveringScope( scope, itemid) : resolveNearItemInsideScope( scope, itemid);
			if (ObjectRef_is_value( objref))
			{
				int valueidx = ObjectRef_value_id( objref);
				papuga_init_ValueVariant_copy( &arg, &m_ctx->values()[ valueidx].content);
			}
			else if (ObjectRef_is_struct( objref))
			{
				int structidx = ObjectRef_struct_id( objref);
				papuga_Serialization* ser = papuga_Allocator_alloc_Serialization( allocator);
				papuga_init_ValueVariant_serialization( &arg, ser);
				if (!build_structure( ser, scope, structidx)) return false;
			}
			else if (defaultvalue)
			{
				papuga_init_ValueVariant_charp( &arg, defaultvalue);
			}
			else
			{
				m_errcode = papuga_ValueUndefined;
				return false;
			}
			return true;
		}

		bool setCallArgValue( papuga_ValueVariant& arg, const CallArgDef& argdef, const Scope& scope, const papuga_RequestVariable* varlist)
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
				papuga_init_ValueVariant_copy( &arg, &vl->value);
			}
			else
			{
				if (!initResolvedItemValue( arg, &m_allocator, argdef.itemid, argdef.inherited, scope, argdef.defaultvalue))
				{
					return false;
				}
			}
			return true;
		}

		bool setCallArgs( papuga_CallArgs* args, const MethodCallNode& mcnode, const papuga_RequestVariable* varlist)
		{
			const CallDef* mcdef = mcnode.def;
			int ai = 0, ae = mcdef->nofargs;
			for (; ai != ae; ++ai)
			{
				if (!setCallArgValue( args->argv[ai], mcdef->args[ai], mcnode.scope, varlist)) return false;
			}
			args->argc = mcdef->nofargs;
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
	bool processEvent( int ev, const papuga_ValueVariant* evalue, bool deepcopy)
	{
		int evidx = AtmRef_index( ev);
		++scopecnt;
		switch (AtmRef_type( ev))
		{
			case InstantiateValue:
			{
#ifdef PAPUGA_LOWLEVEL_DEBUG
				papuga_ErrorCode errcode = papuga_Ok;
				std::string evaluestr = ValueVariant_tostring( *evalue, errcode);
				fprintf( stderr, "process event [%d,%d] instantiate value='%s', itemid=%d\n", (int)InstantiateValue, evidx, evaluestr.c_str(), m_atm->valuedefs()[ evidx].itemid);
#endif
				if (!deepcopy)
				{
					m_valuenodes.push_back( ValueNode( m_atm->valuedefs()[ evidx].itemid, Value( evalue)));
				}
				else if (papuga_ValueVariant_isstring( evalue) && (evalue->encoding == papuga_UTF8 || evalue->encoding == papuga_Binary))
				{
					char* valuestrcopy = papuga_Allocator_copy_string( &m_allocator, evalue->value.string, evalue->length);
					m_valuenodes.push_back( ValueNode( m_atm->valuedefs()[ evidx].itemid, Value( valuestrcopy, evalue->length)));
				}
				else
				{
					m_errcode = papuga_LogicError;
					//... conversion to 'UTF8' happens earlier in LocalBuffer (papuga_ValueVariant_tostring), 'Binary' is untouched
					return false;
				}
				break;
			}
			case CollectValue:
			{
				int itemid = m_atm->valuedefs()[ evidx].itemid;
#ifdef PAPUGA_LOWLEVEL_DEBUG
				fprintf( stderr, "process event [%d,%d] collect item id=%d\n", (int)CollectValue, evidx, itemid);
#endif
				std::vector<ValueNode>::iterator vi = m_valuenodes.begin();
				int valuecnt = 0;
				int vidx = 0;
				while (vidx != m_valuenodes.size())
				{
					if (vi->itemid == itemid)
					{
						m_scopeobjmap[ itemid].insert( ScopeObjElem( Scope( m_scopestack.back(), scopecnt), ObjectRef_value( m_values.size())));
						m_values.push_back( vi->value);
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
					m_scopeobjmap[ itemid].insert( ScopeObjElem( Scope( m_scopestack.back(), scopecnt), ObjectRef_value( m_values.size())));
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
#ifdef PAPUGA_LOWLEVEL_DEBUG
				fprintf( stderr, "process event [%d,%d] close struct itemid=%d\n", (int)CloseStruct, evidx, m_atm->valuedefs()[ evidx].itemid);
#endif
				const StructDef* stdef = &m_atm->structdefs()[ evidx];
				int itemid = stdef->itemid;
				m_scopeobjmap[ itemid].insert( ScopeObjElem( Scope( m_scopestack.back(), scopecnt), ObjectRef_struct( m_structs.size())));
				m_structs.push_back( stdef);
			}
			case MethodCall:
			{
#ifdef PAPUGA_LOWLEVEL_DEBUG
				std::string calldefstr = m_atm->calldefs()[ evidx].tostring();
				fprintf( stderr, "process event [%d,%d] method call: %s\n", (int)MethodCall, evidx, calldefstr.c_str());
#endif
				const CallDef* calldef = &m_atm->calldefs()[ evidx];
				MethodCallKey key( calldef->groupid, scopecnt, evidx);
				Scope scope( m_scopestack.back(), scopecnt);
				m_methodcalls.push_back( MethodCallNode( calldef, scope, key));
				if (m_done)
				{
					m_errcode = papuga_ExecutionOrder;
					return false;
				}
			}
		}
		return true;
	}

private:
	typedef textwolf::XMLPathSelect<textwolf::charset::UTF8> AutomatonState;

	const AutomatonDescription* m_atm;
	AutomatonState m_atmstate;
	int scopecnt;
	papuga_Allocator m_allocator;
	char m_allocator_membuf[ 4096];
	std::vector<int> m_scopestack;
	std::vector<ValueNode> m_valuenodes;
	std::vector<Value> m_values;
	std::vector<const StructDef*> m_structs;
	std::vector<ScopeObjMap> m_scopeobjmap;
	std::vector<MethodCallNode> m_methodcalls;
	bool m_done;
	papuga_ErrorCode m_errcode;
};

}//anonymous namespace



struct papuga_RequestAutomaton
{
	AutomatonDescription atm;
};

extern "C" papuga_RequestAutomaton* papuga_create_RequestAutomaton( const papuga_ClassDef* classdefs)
{
	papuga_RequestAutomaton* rt = (papuga_RequestAutomaton*)std::calloc( 1, sizeof(*rt));
	if (!rt) return NULL;
	try
	{
		new (&rt->atm) AutomatonDescription( classdefs);
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

extern "C" bool papuga_RequestAutomaton_set_call_arg_item( papuga_RequestAutomaton* self, int idx, int itemid, bool inherited, const char* defaultvalue)
{
	return self->atm.setCallArgItem( idx, itemid, inherited, defaultvalue);
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

