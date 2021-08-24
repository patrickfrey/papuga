/*
 * Copyright (c) 2017 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/*
* \brief Request context data and the collection of it
* \file requestContext.cpp
*/
#include "papuga/requestContext.h"
#include "papuga/allocator.h"
#include "papuga/serialization.h"
#include "papuga/serialization.hpp"
#include "papuga/valueVariant.h"
#include "papuga/valueVariant.hpp"
#include "papuga/errors.h"
#include "papuga/callResult.h"
#include "private/shared_ptr.hpp"
#include "private/unordered_map.hpp"
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <list>
#include <map>
#include <stdexcept>
#include <cstdio>
#include <iostream>
#include <sstream>

/* @brief Hook for GETTEXT */
#define _TXT(x) x
#undef PAPUGA_LOWLEVEL_DEBUG

struct RequestVariable
{
	RequestVariable()
	{
		init();
		name = "";
	}
	explicit RequestVariable( const char* name_)
	{
		init();
		name = papuga_Allocator_copy_charp( &allocator, name_);
		if (!name) throw std::bad_alloc();
	}
	~RequestVariable()
	{
		papuga_destroy_Allocator( &allocator);
	}

	papuga_Allocator allocator;
	const char* name;				/*< name of variable associated with this value */
	papuga_ValueVariant value;			/*< variable value associated with this name */
	int allocatormem[ 128];				/*< memory buffer used for allocator */

private:
	void init()
	{
		papuga_init_Allocator( &allocator, allocatormem, sizeof(allocatormem));
		name = NULL;
		papuga_init_ValueVariant( &value);
	}
};

struct RequestVariableRef
{
	papuga::shared_ptr<RequestVariable> ptr;	/*< pointer to variable */
	int inheritcnt;					/*< counter of inheritance */

	RequestVariableRef( const char* name)
		:ptr(new RequestVariable(name)),inheritcnt(0){}
	RequestVariableRef( const RequestVariableRef& o, bool incrementInheritCnt=false)
		:ptr(o.ptr),inheritcnt(o.inheritcnt)
	{
		if (incrementInheritCnt) ++inheritcnt;
	}
};

static bool isLocalVariable( const char* name)
{
	return name[0] == '_';
}

/*
 * @brief Defines a map of variables of a request as list
 * @remark A request does not need too many variables, maybe 2 or 3, so a list is fine for search
 */
struct RequestVariableMap
{
public:
	RequestVariableMap()
		:m_impl(){}
	RequestVariableMap( const RequestVariableMap& o)
		:m_impl(o.m_impl){}
	~RequestVariableMap(){}

	RequestVariable* create( const char* name)
	{
		m_impl.push_back( RequestVariableRef( name));
		return m_impl.back().ptr.get();
	}
	void append( const RequestVariableMap& map)
	{
		m_impl.insert( m_impl.end(), map.m_impl.begin(), map.m_impl.end());
	}
	bool merge( const RequestVariableMap& map)
	{
		std::vector<RequestVariableRef>::const_iterator vi = map.m_impl.begin(), ve = map.m_impl.end();
		for (; vi != ve; ++vi)
		{
			const papuga_ValueVariant* var = findVariable( vi->ptr->name);
			if (var)
			{
				if (var != &vi->ptr->value) return false;
			}
			else
			{
				m_impl.push_back( RequestVariableRef( *vi, true));
			}
		}
		return true;
	}

	typedef std::vector<RequestVariableRef>::const_iterator const_iterator;
	const_iterator begin() const
	{
		return m_impl.begin();
	}
	const_iterator end() const
	{
		return m_impl.end();
	}
	void removeLocalVariables()
	{
		std::size_t vidx = 0;
		while (vidx < m_impl.size())
		{
			if (isLocalVariable( m_impl[vidx].ptr->name))
			{
				m_impl.erase( m_impl.begin()+vidx);
			}
			else
			{
				++vidx;
			}
		}
	}
	const papuga_ValueVariant* findVariable( const char* name) const
	{
		std::vector<RequestVariableRef>::const_iterator vi = m_impl.begin(), ve = m_impl.end();
		for (; vi != ve; ++vi)
		{
			if (0==std::strcmp( vi->ptr->name, name)) break;
		}
		return (vi == ve) ? NULL : &vi->ptr->value;
	}
	RequestVariable* createVariable( const char* name)
	{
		std::vector<RequestVariableRef>::iterator vi = m_impl.begin(), ve = m_impl.end();
		for (; vi != ve; ++vi) if (0==std::strcmp( vi->ptr->name, name)) break;
		if (vi == ve)
		{
			return create( name);
		}
		else
		{
			*vi = RequestVariableRef( name);
			return vi->ptr.get();
		}
	}
	const char** listVariables( int max_inheritcnt, char const** buf, size_t bufsize) const
	{
		size_t bufpos = 0;
		std::vector<RequestVariableRef>::const_iterator vi = m_impl.begin(), ve = m_impl.end();
		for (; vi != ve; ++vi)
		{
			if (bufpos >= bufsize) return NULL;
			if (max_inheritcnt >= 0 && vi->inheritcnt > max_inheritcnt) continue;
			buf[ bufpos++] = vi->ptr->name;
		}
		if (bufpos >= bufsize) return NULL;
		buf[ bufpos] = NULL;
		return buf;
	}
private:
	std::vector<RequestVariableRef> m_impl;
};

/*
 * @brief Defines the context of a request
 */
struct papuga_RequestContext
{
	papuga_ErrorCode errcode;		//< last error in the request context
	RequestVariableMap varmap;		//< map of variables defined in this context

	explicit papuga_RequestContext()
		:errcode(papuga_Ok),varmap()
	{
	}
	~papuga_RequestContext()
	{}

	std::string tostring( const char* indent, papuga_StructInterfaceDescription* structdefs) const
	{
		std::ostringstream out;
		RequestVariableMap::const_iterator vi = varmap.begin(), ve = varmap.end();
		for (; vi != ve; ++vi)
		{
			papuga_ErrorCode errcode_local = papuga_Ok;
			out << indent << vi->ptr->name << " #" << vi->ptr.use_count() << "=" << papuga::ValueVariant_todump( vi->ptr->value, structdefs, true/*deterministic*/, errcode_local) << "\n";
		}
		return out.str();
	}
private:
	papuga_RequestContext( const papuga_RequestContext&){};	//... non copyable
	void operator=( const papuga_RequestContext&){};	//... non copyable
};

struct SymKey
{
	const char* str;
	std::size_t len;

	SymKey()
		:str(0),len(0){}
	SymKey( const char* str_, std::size_t len_)
		:str(str_),len(len_){}
	SymKey( const SymKey& o)
		:str(o.str),len(o.len){}

	static SymKey create( char* keybuf, std::size_t keybufsize, const char* type, const char* name)
	{
		std::size_t keysize = std::snprintf( keybuf, keybufsize, "%s/%s", type, name);
		if (keybufsize <= keysize) throw std::bad_alloc();
		return SymKey( keybuf, keysize);
	}
	bool operator < (const SymKey& o) const
	{
		return 0>std::memcmp( str, o.str, (len < o.len ? len : o.len)+1);
	}
};
struct MapSymKeyEqual
{
	bool operator()( const SymKey& a, const SymKey& b) const
	{
		return a.len == b.len && std::memcmp( a.str, b.str, a.len) == 0;
	}
};
struct SymKeyHashFunc
{
	int operator()( const SymKey& key)const
	{
		static const int har[16] = {2, 3, 5, 7, 11, 13, 17, 19, 23, 29, 31, 37, 41, 43, 47, 53};
		int hh = key.len + 1013 + key.len;
		unsigned char const* ki = (unsigned char const*)key.str;
		const unsigned char* ke = ki + key.len;
		int kidx = 0;
		for (; ki < ke; ++ki,++kidx)
		{
			int aa = (*ki+hh+kidx) & 0xFF;
			hh = ((hh << 3) + (hh >> 7)) * har[ aa & 15] + har[ (aa >> 4) & 15];
		}
		return hh;
	}
};

typedef papuga::shared_ptr<papuga_RequestContext> RequestContextRef;
typedef papuga::unordered_map<SymKey,RequestContextRef,SymKeyHashFunc,MapSymKeyEqual> RequestContextTab;
struct RequestContextMap
{
	std::list<std::string> keylist;
	RequestContextTab tab;

	~RequestContextMap(){}
	RequestContextMap()
		:keylist(),tab(){}
	RequestContextMap( const RequestContextMap& o)
		:keylist(),tab()
	{
		RequestContextTab::const_iterator ti = o.tab.begin(), te = o.tab.end();
		for (; ti != te; ++ti)
		{
			tab[ allocKey( ti->first)] = ti->second;
		}
	}
	void addOwnership( const SymKey& key, papuga_RequestContext* context)
	{
		tab[ allocKey( key)].reset( context);
	}
	const papuga_RequestContext* operator[]( const SymKey& key) const
	{
		RequestContextTab::const_iterator mi = tab.find( key);
		return mi == tab.end() ? NULL : mi->second.get();
	}

	std::string tostring( papuga_StructInterfaceDescription* structdefs) const
	{
		std::map<SymKey,papuga_RequestContext*> deterministicMap;
		std::ostringstream out;
		auto ci = tab.begin(), ce = tab.end();
		for (; ci != ce; ++ci)
		{
			deterministicMap[ ci->first] = &*ci->second;
		}
		auto di = deterministicMap.begin(), de = deterministicMap.end();
		for (; di != de; ++di)
		{
			out << std::string(di->first.str,di->first.len) << ":" << std::endl;
			out << di->second->tostring( "\t", structdefs) << std::endl;
		}
		return out.str();
	}

private:
	SymKey allocKey( const SymKey& key)
	{
		keylist.push_back( std::string( key.str, key.len));
		return SymKey( keylist.back().c_str(), keylist.back().size());
	}
};

typedef papuga::shared_ptr<RequestContextMap> RequestContextMapRef;

// \brief Add with ownership
static void RequestContextMap_transfer( RequestContextMapRef& cm, const char* type, const char* name, papuga_RequestContext* context)
{
	// Updates in the context map are very rare, so a copy of the whole map on an update is feasible and avoids a lock on read
	char keybuf[ 256];
	SymKey key = SymKey::create( keybuf, sizeof(keybuf), type, name);
	RequestContextMapRef cm_ref( cm);
	RequestContextMapRef cm_copy( new RequestContextMap( *cm_ref));
	context->varmap.removeLocalVariables();
	cm_copy->addOwnership( key, context);
	cm = cm_copy;
}

static bool RequestContextMap_delete( RequestContextMapRef& cm, const char* type, const char* name)
{
	// Updates in the context map are very rare, so a copy of the whole map on a delete is feasible and avoids a lock on read
	char keybuf[ 256];
	SymKey key = SymKey::create( keybuf, sizeof(keybuf), type, name);
	RequestContextMapRef cm_ref( cm);
	RequestContextMapRef cm_copy( new RequestContextMap( *cm_ref));
	if (cm_copy->tab.erase( key) > 0)
	{
		cm = cm_copy;
		return true;
	}
	return false;
}

struct papuga_RequestContextPool
{
	RequestContextMapRef contextmap;
	papuga_Allocator allocator;
	int allocator_membuf[ 1024];

	explicit papuga_RequestContextPool()
		:contextmap(new RequestContextMap())
	{
		papuga_init_Allocator( &allocator, allocator_membuf, sizeof(allocator_membuf));
	}

	~papuga_RequestContextPool()
	{
		papuga_destroy_Allocator( &allocator);
	}
};

extern "C" papuga_RequestContext* papuga_create_RequestContext()
{
	try
	{
		return new papuga_RequestContext();
	}
	catch (...)
	{
		return NULL;
	}

}

extern "C" void papuga_destroy_RequestContext( papuga_RequestContext* self)
{
	delete self;
}

extern "C" papuga_ErrorCode papuga_RequestContext_last_error( papuga_RequestContext* self, bool clear)
{
	if (clear)
	{
		papuga_ErrorCode rt = self->errcode; self->errcode = papuga_Ok; return rt;
	}
	else
	{
		return self->errcode;
	}
}

extern "C" bool papuga_RequestContext_define_variable( papuga_RequestContext* self, const char* name, papuga_ValueVariant* value)
{
	try
	{
		if (name && (((name[0]|32) >= 'a' && (name[0]|32) <= 'z') || (name[0] >= '0' && name[0] <= '9') || name[0] == '_'))
		{
			RequestVariable* var = self->varmap.createVariable( name);
			return papuga_Allocator_deepcopy_value( &var->allocator, &var->value, value, true/*movehostobj*/, &self->errcode);
		}
		else
		{
			self->errcode = papuga_SyntaxError;
			return false;
		}
	}
	catch (...)
	{
		self->errcode = papuga_NoMemError;
		return false;
	}
}

extern "C" const papuga_ValueVariant* papuga_RequestContext_get_variable( const papuga_RequestContext* self, const char* name)
{
	return self->varmap.findVariable( name);
}

extern "C" const char** papuga_RequestContext_list_variables( const papuga_RequestContext* self, int max_inheritcnt, char const** buf, size_t bufsize)
{
	return self->varmap.listVariables( max_inheritcnt, buf, bufsize);
}

extern "C" bool papuga_RequestContext_inherit( papuga_RequestContext* self, const papuga_RequestContextPool* handler, const char* type, const char* name)
{
	try
	{
		char keybuf[ 256];
		SymKey key = SymKey::create( keybuf, sizeof(keybuf), type, name);

		RequestContextMapRef contextmap( handler->contextmap);	//... keep instance of context map alive until job is done
		const papuga_RequestContext* context = (*contextmap)[ key];

		if (!context)
		{
			self->errcode = papuga_AddressedItemNotFound;
			return false;
		}
		if (!self->varmap.merge( context->varmap))
		{
			self->errcode = papuga_DuplicateDefinition;
			return false;
		}
		return true;
	}
	catch (...)
	{
		self->errcode = papuga_NoMemError;
		return false;
	}
}

extern "C" papuga_RequestContextPool* papuga_create_RequestContextPool()
{
	try
	{
		return new papuga_RequestContextPool();
	}
	catch (...)
	{
		return NULL;
	}
}

extern "C" void papuga_destroy_RequestContextPool( papuga_RequestContextPool* self)
{
	delete self;
}

extern "C" bool papuga_RequestContextPool_remove_context( papuga_RequestContextPool* self, const char* type, const char* name, papuga_ErrorCode* errcode)
{
	try
	{
		return RequestContextMap_delete( self->contextmap, type, name);
	}
	catch (...)
	{
		*errcode = papuga_NoMemError;
		return false;
	}
	return true;
}

extern "C" bool papuga_RequestContextPool_transfer_context( papuga_RequestContextPool* self, const char* type, const char* name, papuga_RequestContext* context, papuga_ErrorCode* errcode)
{
	try
	{
		RequestContextMap_transfer( self->contextmap, type, name, context);
		return true;
	}
	catch (...)
	{
		*errcode = papuga_NoMemError;
		return false;
	}
	return true;
}

extern "C" const char* papuga_RequestContext_debug_tostring( const papuga_RequestContext* context, papuga_Allocator* allocator, papuga_StructInterfaceDescription* structdefs)
{
	try
	{
		char* rt;
		std::string dump = context->tostring("",structdefs);
		if (allocator)
		{
			rt = papuga_Allocator_copy_string( allocator, dump.c_str(), dump.size());
		}
		else
		{
			rt = (char*)std::malloc( dump.size()+1);
			if (!rt) return 0;
			std::memcpy( rt,  dump.c_str(), dump.size()+1);
		}
		return rt;
	}
	catch (...)
	{
		return 0;
	}
}

extern "C" const char* papuga_RequestContextPool_debug_contextmap_tostring( const papuga_RequestContextPool* self, papuga_Allocator* allocator, papuga_StructInterfaceDescription* structdefs)
{
	try
	{
		char* rt;
		RequestContextMapRef contextmap( self->contextmap);
		std::string dump = contextmap->tostring( structdefs);
		if (allocator)
		{
			rt = papuga_Allocator_copy_string( allocator, dump.c_str(), dump.size());
		}
		else
		{
			rt = (char*)std::malloc( dump.size()+1);
			if (!rt) return 0;
			std::memcpy( rt,  dump.c_str(), dump.size()+1);
		}
		return rt;
	}
	catch (...)
	{
		return 0;
	}
}

