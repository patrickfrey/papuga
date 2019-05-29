/*
 * Copyright (c) 2017 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/*
* \brief Automaton to describe and build papuga XML and JSON requests
* \file request.h
*/
#include "papuga/requestHandler.h"
#include "papuga/request.h"
#include "papuga/classdef.h"
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
#include <stdexcept>
#include <cstdio>
#include <iostream>
#include <sstream>

/* @brief Hook for GETTEXT */
#define _TXT(x) x
#undef PAPUGA_LOWLEVEL_DEBUG

template <typename TYPE>
static TYPE* alloc_type( papuga_Allocator* allocator)
{
	TYPE* rt = (TYPE*)papuga_Allocator_alloc( allocator, sizeof(TYPE), 0);
	if (rt) std::memset( rt, 0, sizeof(TYPE));
	return rt;
}
template <typename TYPE>
static TYPE* add_list( TYPE* lst, TYPE* elem)
{
	if (!lst) return elem;
	TYPE* vv = lst;
	while (vv->next) vv=vv->next;
	vv->next = elem;
	return lst;
}

struct RequestSchemaList
{
	struct RequestSchemaList* next;			/*< pointer next element in the single linked list */
	const char* type;				/*< type name of the context this schema is valid for */
	const char* name;				/*< name of the schema */
	const papuga_RequestAutomaton* automaton;	/*< automaton of the schema */
	const papuga_SchemaDescription* description;	/*< description of the schema */
};

struct RequestMethodList
{
	struct RequestMethodList* next;			/*< pointer next element in the single linked list */
	const char* name;				/*< name of the schema */
	papuga_RequestMethodDescription method;		/*< method description */
};

struct RequestVariable
{
	RequestVariable()
	{
		init();
		name = "";
		isArray = false;
	}
	explicit RequestVariable( const char* name_)
	{
		init();
		name = papuga_Allocator_copy_charp( &allocator, name_);
		if (!name) throw std::bad_alloc();
		isArray = false;
	}
	~RequestVariable()
	{
		papuga_destroy_Allocator( &allocator);
	}

	papuga_Allocator allocator;
	const char* name;				/*< name of variable associated with this value */
	papuga_ValueVariant value;			/*< variable value associated with this name */
	bool isArray;					/*< true, if the variable content is a list of values (different way of merging with input int the result) */
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
	const papuga_ValueVariant* findVariable( const char* name, int* isArray) const
	{
		const RequestVariable* var = 0;
		std::vector<RequestVariableRef>::const_iterator vi = m_impl.begin(), ve = m_impl.end();
		for (; vi != ve; ++vi)
		{
			var = vi->ptr.get();
			if (0==std::strcmp( var->name, name)) break;
		}
		if (vi == ve) return NULL;
		if (isArray) *isArray = var->isArray;
		return &var->value;
	}
	RequestVariable* getOrCreate( const char* name)
	{
		std::vector<RequestVariableRef>::const_iterator vi = m_impl.begin(), ve = m_impl.end();
		for (; vi != ve; ++vi) if (0==std::strcmp( vi->ptr->name, name)) break;
		if (vi == ve)
		{
			return create( name);
		}
		else
		{
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

typedef papuga::shared_ptr<RequestVariableMap> RequestVariableMapRef;


/*
 * @brief Defines the context of a request
 */
struct papuga_RequestContext
{
	std::string classname;
	papuga_ErrorCode errcode;		/*< last error in the request context */
	RequestVariableMap varmap;		/*< map of variables defined in this context */

	explicit papuga_RequestContext( const char* classname_)
		:classname(classname_?classname_:""),errcode(papuga_Ok),varmap(){}
	papuga_RequestContext( const papuga_RequestContext& o)
		:classname(o.classname),errcode(o.errcode),varmap(o.varmap){}
	~papuga_RequestContext(){}

	std::string tostring() const
	{
		std::ostringstream out;
		out << classname << ":\n";
		RequestVariableMap::const_iterator vi = varmap.begin(), ve = varmap.end();
		for (; vi != ve; ++vi)
		{
			papuga_ErrorCode errcode_local = papuga_Ok;
			out << vi->ptr->name << "=" << papuga::ValueVariant_tostring( vi->ptr->value, errcode_local) << "\n";
		}
		return out.str();
	}
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

	std::string tostring() const
	{
		std::ostringstream out;
		RequestContextTab::const_iterator ci = tab.begin(), ce = tab.end();
		for (; ci != ce; ++ci)
		{
			out << std::string(ci->first.str,ci->first.len) << ":" << std::endl;
			out << ci->second->tostring() << std::endl;
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
	cm_copy->addOwnership( key, context);
	context->varmap.removeLocalVariables();
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


static int nofClassDefs( const papuga_ClassDef* classdefs)
{
	papuga_ClassDef const* ci = classdefs;
	int classcnt = 0;
	for (; ci->name; ++ci,++classcnt){}
	return classcnt;
}

struct papuga_RequestHandler
{
	RequestContextMapRef contextmap;
	RequestSchemaList* schemas;
	RequestMethodList** classmethodmap;
	int classmethodmapsize;
	const papuga_ClassDef* classdefs;
	papuga_Allocator allocator;
	int allocator_membuf[ 1024];

	explicit papuga_RequestHandler( const papuga_ClassDef* classdefs_)
		:contextmap(new RequestContextMap()),schemas(NULL),classmethodmap(NULL),classmethodmapsize(nofClassDefs(classdefs_)),classdefs(classdefs_)
	{
		papuga_init_Allocator( &allocator, allocator_membuf, sizeof(allocator_membuf));
		std::size_t classmethodmapmem = (classmethodmapsize+1) * sizeof(RequestMethodList*);
		classmethodmap = (RequestMethodList**)papuga_Allocator_alloc( &allocator, classmethodmapmem, sizeof(RequestMethodList*));
		if (!classmethodmap) throw std::bad_alloc();
		std::memset( classmethodmap, 0, classmethodmapmem);
	}

	~papuga_RequestHandler()
	{
		papuga_destroy_Allocator( &allocator);
	}
};

extern "C" papuga_RequestContext* papuga_create_RequestContext( const char* classname)
{
	try
	{
		return new papuga_RequestContext( classname);
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

extern "C" const char* papuga_RequestContext_classname( const papuga_RequestContext* self)
{
	return self->classname.empty() ? NULL : self->classname.c_str();
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

extern "C" bool papuga_RequestContext_add_variable( papuga_RequestContext* self, const char* name, papuga_ValueVariant* value)
{
	try
	{
		RequestVariable* var = self->varmap.getOrCreate( name);
		return papuga_Allocator_deepcopy_value( &var->allocator, &var->value, value, true/*movehostobj*/, &self->errcode);
	}
	catch (...)
	{
		self->errcode = papuga_NoMemError;
		return false;
	}
}

extern "C" const papuga_ValueVariant* papuga_RequestContext_get_variable( const papuga_RequestContext* self, const char* name, int* isArray)
{
	return self->varmap.findVariable( name, isArray);
}

extern "C" const char** papuga_RequestContext_list_variables( const papuga_RequestContext* self, int max_inheritcnt, char const** buf, size_t bufsize)
{
	return self->varmap.listVariables( max_inheritcnt, buf, bufsize);
}

extern "C" bool papuga_RequestContext_inherit( papuga_RequestContext* self, const papuga_RequestHandler* handler, const char* type, const char* name)
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

extern "C" papuga_RequestHandler* papuga_create_RequestHandler( const papuga_ClassDef* classdefs)
{
	try
	{
		return new papuga_RequestHandler( classdefs);
	}
	catch (...)
	{
		return NULL;
	}
}

extern "C" void papuga_destroy_RequestHandler( papuga_RequestHandler* self)
{
	delete self;
}

extern "C" bool papuga_RequestHandler_remove_context( papuga_RequestHandler* self, const char* type, const char* name, papuga_ErrorCode* errcode)
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

extern "C" bool papuga_RequestHandler_transfer_context( papuga_RequestHandler* self, const char* type, const char* name, papuga_RequestContext* context, papuga_ErrorCode* errcode)
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

extern "C" bool papuga_RequestHandler_add_schema( papuga_RequestHandler* self, const char* type, const char* name, const papuga_RequestAutomaton* automaton, const papuga_SchemaDescription* description)
{
	RequestSchemaList* listitem = alloc_type<RequestSchemaList>( &self->allocator);
	if (!listitem) return false;
	listitem->type = papuga_Allocator_copy_charp( &self->allocator, type);
	listitem->name = papuga_Allocator_copy_charp( &self->allocator, name);
	if (!listitem->type || !listitem->name) return false;
	listitem->automaton = automaton;
	listitem->description = description;
	self->schemas = add_list( self->schemas, listitem);
	return true;
}

static RequestSchemaList* find_schema( RequestSchemaList* ll, const char* type, const char* name)
{
	for (; ll; ll = ll->next){if ((ll->name[0]+ll->type[0]) == (name[0]+type[0]) && 0==std::strcmp( ll->name, name) && 0==std::strcmp( ll->type, type)) break;}
	return ll;
}

static const char** RequestHandler_list_all_schema_names( const papuga_RequestHandler* self, char const** buf, size_t bufsize)
{
	size_t bufpos = 0;
	RequestSchemaList const* sl = self->schemas;
	for (; sl; sl = sl->next)
	{
		size_t bi = 0;
		for (; bi < bufpos; ++bi){if (0==std::strcmp(buf[bi],sl->name)) break;}
		if (bi < bufpos) continue;
		if (bufpos >= bufsize) return NULL;
		buf[ bufpos++] = sl->name;
	}
	if (bufpos >= bufsize) return NULL;
	buf[ bufpos] = NULL;
	return buf;
}

const char** papuga_RequestHandler_list_schema_types( const papuga_RequestHandler* self, char const** buf, size_t bufsize)
{
	size_t bufpos = 0;
	RequestSchemaList const* sl = self->schemas;
	for (; sl; sl = sl->next)
	{
		size_t bi = 0;
		for (; bi < bufpos; ++bi){if (0==std::strcmp(buf[bi],sl->type)) break;}
		if (bi < bufpos) continue;
		if (bufpos >= bufsize) return NULL;
		buf[ bufpos++] = sl->type;
	}
	if (bufpos >= bufsize) return NULL;
	buf[ bufpos] = NULL;
	return buf;
}

extern "C" const char** papuga_RequestHandler_list_schema_names( const papuga_RequestHandler* self, const char* type, char const** buf, size_t bufsize)
{
	size_t bufpos = 0;
	RequestSchemaList const* sl = self->schemas;

	if (!type)
	{
		return RequestHandler_list_all_schema_names( self, buf, bufsize);
	}
	for (; sl; sl = sl->next)
	{
		if (!type || 0==std::strcmp(type, sl->type))
		{
			if (bufpos >= bufsize) return NULL;
			buf[ bufpos++] = sl->name;
		}
	}
	if (bufpos >= bufsize) return NULL;
	buf[ bufpos] = NULL;
	return buf;
}

extern "C" const papuga_RequestAutomaton* papuga_RequestHandler_get_automaton( const papuga_RequestHandler* self, const char* type, const char* name)
{
	const RequestSchemaList* sl = find_schema( self->schemas, type?type:"", name);
	return sl ? sl->automaton : NULL;
}

extern "C" const papuga_SchemaDescription* papuga_RequestHandler_get_description( const papuga_RequestHandler* self, const char* type, const char* name)
{
	const RequestSchemaList* sl = find_schema( self->schemas, type?type:"", name);
	return sl ? sl->description : NULL;
}

extern "C" bool papuga_RequestHandler_add_method( papuga_RequestHandler* self, const char* name, const papuga_RequestMethodDescription* method)
{
	int nofparams = 0;
	if (method->id.classid == 0 || method->id.classid > self->classmethodmapsize) return false;
	while (method->paramtypes[nofparams]) ++nofparams;
	RequestMethodList** mlst = self->classmethodmap + (method->id.classid-1);
	RequestMethodList* listitem = alloc_type<RequestMethodList>( &self->allocator);
	if (!listitem) return false;
	std::memcpy( &listitem->method, method, sizeof( listitem->method));
	listitem->name = papuga_Allocator_copy_charp( &self->allocator, name);
	listitem->method.paramtypes = (int*)papuga_Allocator_alloc( &self->allocator, (nofparams+1) * sizeof(int), sizeof(int));
	if (!listitem->name || !listitem->method.paramtypes) return false;
	std::memcpy( listitem->method.paramtypes, (const void*)method->paramtypes, (nofparams+1) * sizeof(int));
	*mlst = add_list( *mlst, listitem);
	return true;
}

extern "C" const papuga_RequestMethodDescription* papuga_RequestHandler_get_method( const papuga_RequestHandler* self, int classid, const char* name, bool with_content)
{
	if (classid == 0 || classid > self->classmethodmapsize) return NULL;
	RequestMethodList const* ml = self->classmethodmap[ classid-1];
	for (; ml; ml = ml->next){if (ml->method.has_content == with_content && ml->name[0] == name[0] && 0==std::strcmp( ml->name, name)) break;}
	return ml ? &ml->method : NULL;
}

const char** papuga_RequestHandler_list_methods( const papuga_RequestHandler* self, int classid, char const** buf, size_t bufsize)
{
	if (classid == 0 || classid > self->classmethodmapsize) return NULL;
	size_t bufpos = 0;
	RequestMethodList const* ml = self->classmethodmap[ classid-1];

	for (; ml; ml = ml->next)
	{
		if (bufpos >= bufsize) return NULL;
		buf[ bufpos++] = ml->name;
	}
	if (bufpos >= bufsize) return NULL;
	buf[ bufpos] = NULL;
	return buf;
}

static void reportMethodCallError( papuga_ErrorBuffer* errorbuf, const papuga_Request* request, const papuga_RequestMethodCall* call, const char* msg)
{
	const papuga_ClassDef* classdef = papuga_Request_classdefs( request);
	const char* classname = classdef[ call->methodid.classid-1].name;
	if (call->methodid.functionid)
	{
		const char* methodname = classdef[ call->methodid.classid-1].methodnames[ call->methodid.functionid-1];
		papuga_ErrorBuffer_reportError( errorbuf, _TXT( "error calling method %s::%s: %s"), classname, methodname, msg);
	}
	else
	{
		papuga_ErrorBuffer_reportError( errorbuf, _TXT( "error calling constructor of %s: %s"), classname, msg);
	}
}

static void reportMethodResolveError( papuga_ErrorBuffer* errorbuf, const papuga_Request* request, const papuga_RequestMethodError* err, const char* msg)
{
	const papuga_ClassDef* classdef = papuga_Request_classdefs( request);
	const char* classname = classdef[ err->methodid.classid-1].name;
	if (err->methodid.functionid)
	{
		const char* methodname = classdef[ err->methodid.classid-1].methodnames[ err->methodid.functionid-1];
		if (err->argcnt >= 0)
		{
			papuga_ErrorBuffer_reportError( errorbuf, _TXT( "error resolving argument %d (path '/%s') of the method %s::%s  \"%s\""), err->argcnt+1, err->argpath?err->argpath:"", classname, methodname, msg);
		}
		else
		{
			papuga_ErrorBuffer_reportError( errorbuf, _TXT( "error for object: %s"), classname, msg);
		}
	}
	else
	{
		if (err->argcnt >= 0)
		{
			papuga_ErrorBuffer_reportError( errorbuf, _TXT( "error resolving argument %d (path '/%s') of the constructor of %s: %s"), err->argcnt+1, err->argpath?err->argpath:"", classname, msg);
		}
		else
		{
			papuga_ErrorBuffer_reportError( errorbuf, _TXT( "error resolving constructor of %s: %s"), classname, msg);
		}
	}
}

static bool RequestVariable_add_result( RequestVariable& var, papuga_ValueVariant& resultvalue, bool appendresult, papuga_ErrorCode& errcode)
{
	if (appendresult)
	{
		if (!papuga_ValueVariant_defined( &var.value))
		{
			papuga_Serialization* ser = papuga_Allocator_alloc_Serialization( &var.allocator);
			if (!ser) throw std::bad_alloc();
			papuga_init_ValueVariant_serialization( &var.value, ser);
			var.isArray = true;
		}
		if (var.value.valuetype == papuga_TypeSerialization && var.isArray == true)
		{
			if (!papuga_Serialization_pushValue( var.value.value.serialization, &resultvalue)) throw std::bad_alloc();
		}
		else
		{
			errcode = papuga_MixedConstruction;
			return false;
		}
	}
	else if (var.isArray == false)
	{
		papuga_init_ValueVariant_value( &var.value, &resultvalue);
	}
	else
	{
		errcode = papuga_MixedConstruction;
		return false;
	}
#ifdef PAPUGA_LOWLEVEL_DEBUG
	if (!papuga_ValueVariant_isvalid( &var.value))
	{
		errcode = papuga_LogicError;
		return false;
	}
#endif
	return true;
}

extern "C" bool papuga_RequestContext_execute_request( papuga_RequestContext* context, const papuga_Request* request, papuga_RequestLogger* logger, papuga_ErrorBuffer* errorbuf, int* errorpos)
{
	papuga_RequestIterator* itr = 0;
	papuga_Allocator exec_allocator;
	int membuf_allocator[ 1024];
	papuga_init_Allocator( &exec_allocator, &membuf_allocator, sizeof(membuf_allocator));

	try
	{
		char membuf_err[ 1024];
		papuga_ErrorBuffer errorbuf_call;
		papuga_ErrorCode errcode = papuga_Ok;

		const papuga_ClassDef* classdefs = papuga_Request_classdefs( request);
		itr = papuga_create_RequestIterator( &exec_allocator, request);
		if (!itr)
		{
			papuga_ErrorBuffer_reportError( errorbuf, _TXT("error handling request: %s"), papuga_ErrorCode_tostring( papuga_NoMemError));
			papuga_destroy_Allocator( &exec_allocator);
			return false;
		}
		papuga_init_ErrorBuffer( &errorbuf_call, membuf_err, sizeof(membuf_err));
		const papuga_RequestMethodCall* call;

		while (!!(call = papuga_RequestIterator_next_call( itr, context)))
		{
			RequestVariable* var;
			if (call->resultvarname)
			{
				var = context->varmap.getOrCreate( call->resultvarname);
			}
			else
			{
				var = NULL;
			}
			if (call->methodid.functionid == 0)
			{
				// [1] Call the constructor
				const papuga_ClassConstructor func = classdefs[ call->methodid.classid-1].constructor;
				void* self;
	
				if (!(self=(*func)( &errorbuf_call, call->args.argc, call->args.argv)))
				{
					if (logger->logMethodCall)
					{
						(*logger->logMethodCall)( logger->self, 3,
							papuga_LogItemClassName, classdefs[ call->methodid.classid-1].name,
							papuga_LogItemArgc, call->args.argc,
							papuga_LogItemArgv, &call->args.argv[0]);
					}
					reportMethodCallError( errorbuf, request, call, papuga_ErrorBuffer_lastError( &errorbuf_call));
					*errorpos = 0;
					papuga_destroy_RequestIterator( itr);
					papuga_destroy_Allocator( &exec_allocator);
					return false;
				}
				// [2] Assign the result value to a variant type variable and log the call:
				if (var)
				{
					papuga_Deleter destroy_hobj = classdefs[ call->methodid.classid-1].destructor;
					papuga_HostObject* hobj = papuga_Allocator_alloc_HostObject( &var->allocator, call->methodid.classid, self, destroy_hobj);
					if (!hobj)
					{
						destroy_hobj( self);
						throw std::bad_alloc();
					}
					papuga_ValueVariant resultvalue;
					papuga_init_ValueVariant_hostobj( &resultvalue, hobj);

					if (!RequestVariable_add_result( *var, resultvalue, call->appendresult, errcode))
					{
						break;
					}
					if (logger->logMethodCall)
					{
						(*logger->logMethodCall)( logger->self, 4,
							papuga_LogItemClassName, classdefs[ call->methodid.classid-1].name,
							papuga_LogItemArgc, call->args.argc,
							papuga_LogItemArgv, &call->args.argv[0],
							papuga_LogItemResult, &resultvalue);
					}
				}
				else
				{
					if (logger->logMethodCall)
					{
						(*logger->logMethodCall)( logger->self, 3,
							papuga_LogItemClassName, classdefs[ call->methodid.classid-1].name,
							papuga_LogItemArgc, call->args.argc,
							papuga_LogItemArgv, &call->args.argv[0]);
					}
				}
			}
			else
			{
				// [1] Get the method and the object of the method to call:
				const papuga_ClassMethod func = classdefs[ call->methodid.classid-1].methodtable[ call->methodid.functionid-1];
				const papuga_ValueVariant* selfvalue = context->varmap.findVariable( call->selfvarname);
				if (!selfvalue)
				{
					errcode = papuga_MissingSelf;
					break;
				}
				if (selfvalue->valuetype != papuga_TypeHostObject || selfvalue->value.hostObject->classid != call->methodid.classid)
				{
					errcode = papuga_TypeError;
					break;
				}
				// [2] Call the method and report an error on failure:
				void* self = selfvalue->value.hostObject->data;
				papuga_CallResult retval;
				papuga_init_CallResult( &retval, var ? &var->allocator : &exec_allocator, false/*ownership*/, membuf_err, sizeof(membuf_err));
				if (!(*func)( self, &retval, call->args.argc, call->args.argv))
				{
					if (logger->logMethodCall)
					{
						(*logger->logMethodCall)( logger->self, 4,
								papuga_LogItemClassName, classdefs[ call->methodid.classid-1].name,
								papuga_LogItemMethodName, classdefs[ call->methodid.classid-1].methodnames[ call->methodid.functionid-1],
								papuga_LogItemArgc, call->args.argc,
								papuga_LogItemArgv, &call->args.argv[0]);
					}
					reportMethodCallError( errorbuf, request, call, papuga_ErrorBuffer_lastError( &retval.errorbuf));
					*errorpos = 0;
					papuga_destroy_RequestIterator( itr);
					papuga_destroy_Allocator( &exec_allocator);
					return false;
				}
				// [3] Fetch the result(s) if required (stored as variable):
				if (var)
				{
					papuga_ValueVariant resultvalue;
					if (retval.nofvalues == 0)
					{
						papuga_init_ValueVariant( &resultvalue);
					}
					else if (retval.nofvalues == 1)
					{
						papuga_init_ValueVariant_value( &resultvalue, &retval.valuear[0]);
					}
					else
					{
						// ... handle multiple return values as a serialization:
						int vi = 0, ve = retval.nofvalues;
						papuga_Serialization* ser = papuga_Allocator_alloc_Serialization( &var->allocator);
						bool sc = true;
						if (ser)
						{
							sc &= papuga_Serialization_pushOpen( ser);
							for (; vi < ve; ++vi)
							{
								sc &= papuga_Serialization_pushValue( ser, retval.valuear + vi);
							}
							sc &= papuga_Serialization_pushClose( ser);
						}
						if (!ser || !sc)
						{
							errcode = papuga_NoMemError;
							break;
						}
						papuga_init_ValueVariant_serialization( &resultvalue, ser);
					}
					// [4] Add the result to the result variable:
					if (!RequestVariable_add_result( *var, resultvalue, call->appendresult, errcode))
					{
						break;
					}
					// [5] Log the call
					if (logger->logMethodCall)
					{
						if (papuga_ValueVariant_defined( &resultvalue))
						{
							(*logger->logMethodCall)( logger->self, 5, 
										papuga_LogItemClassName, classdefs[ call->methodid.classid-1].name,
										papuga_LogItemMethodName, classdefs[ call->methodid.classid-1].methodnames[ call->methodid.functionid-1],
										papuga_LogItemArgc, call->args.argc,
										papuga_LogItemArgv, &call->args.argv[0],
										papuga_LogItemResult, &resultvalue);
						}
						else
						{
							(*logger->logMethodCall)( logger->self, 4, 
									papuga_LogItemClassName, classdefs[ call->methodid.classid-1].name,
									papuga_LogItemMethodName, classdefs[ call->methodid.classid-1].methodnames[ call->methodid.functionid-1],
									papuga_LogItemArgc, call->args.argc,
									papuga_LogItemArgv, &call->args.argv[0]);
						}
					}
				}
				else if (logger->logMethodCall)
				{
					(*logger->logMethodCall)( logger->self, 4, 
							papuga_LogItemClassName, classdefs[ call->methodid.classid-1].name,
							papuga_LogItemMethodName, classdefs[ call->methodid.classid-1].methodnames[ call->methodid.functionid-1],
							papuga_LogItemArgc, call->args.argc,
							papuga_LogItemArgv, &call->args.argv[0]);
				}
			}
		}
		// Report error if we could not resolve all parts of a method call:
		if (errcode != papuga_Ok)
		{
			reportMethodCallError( errorbuf, request, call, papuga_ErrorCode_tostring( errcode));
			*errorpos = 0;
			papuga_destroy_RequestIterator( itr);
			papuga_destroy_Allocator( &exec_allocator);
			return false;
		}
		else 
		{
			const papuga_RequestMethodError* errstruct = papuga_RequestIterator_get_last_error( itr);
			if (errstruct)
			{
				reportMethodResolveError( errorbuf, request, errstruct, papuga_ErrorCode_tostring( errcode));
				*errorpos = errstruct->scopestart;
				papuga_destroy_RequestIterator( itr);
				papuga_destroy_Allocator( &exec_allocator);
				return false;
			}
		}
		papuga_destroy_RequestIterator( itr);
		papuga_destroy_Allocator( &exec_allocator);
		return true;
	}
	catch (const std::exception& err)
	{
		if (itr) papuga_destroy_RequestIterator( itr);
		papuga_destroy_Allocator( &exec_allocator);
		papuga_ErrorBuffer_reportError( errorbuf, _TXT("error handling request: %s"), err.what());
		return false;
	}
}

static bool serializeResultValue( papuga_Serialization* self, const papuga_ValueVariant& value, papuga_ErrorCode* errcode)
{
	bool rt = true;

	if (value.valuetype == papuga_TypeSerialization
		&& !value.value.serialization->structid
		&& papuga_Serialization_first_tag( value.value.serialization) != papuga_TagName)
	{
		//... variable content is array
		rt &= papuga_Serialization_pushOpen( self);

		papuga_Serialization* elemser = papuga_Allocator_alloc_Serialization( self->allocator);
		papuga_ValueVariant elemserval;
		papuga_init_ValueVariant_serialization( &elemserval, elemser);

		papuga_SerializationIter elemiter;
		papuga_init_SerializationIter( &elemiter, value.value.serialization);

		int taglevel = 0;
		int elementcnt = 0;
		int resultcnt = 0;

		while (!papuga_SerializationIter_eof( &elemiter))
		{
			papuga_Tag tag = papuga_SerializationIter_tag( &elemiter);
			const papuga_ValueVariant* elemvalue = papuga_SerializationIter_value( &elemiter);
			papuga_SerializationIter_skip( &elemiter);
			++elementcnt;

			papuga_Serialization_push( elemser, tag, elemvalue);

			switch (tag)
			{
				case papuga_TagValue:
					if (taglevel == 0 && elementcnt == 1)
					{
						rt &= papuga_Serialization_pushValue( self, elemvalue);

						papuga_init_Serialization( elemser, self->allocator);
						elementcnt = 0;
					}
					break;
				case papuga_TagOpen:
					++taglevel;
					break;
				case papuga_TagClose:
					--taglevel;
					if (taglevel == 0)
					{
						++resultcnt;
						rt &= papuga_Serialization_pushValue( self, &elemserval);

						elemser = papuga_Allocator_alloc_Serialization( self->allocator);
						papuga_init_ValueVariant_serialization( &elemserval, elemser);
						elementcnt = 0;
					}
					break;
				case papuga_TagName:
					break;
				default:
					*errcode = papuga_LogicError;
					return false;
			}
		}
		rt &= papuga_Serialization_pushClose( self);
	}
	else
	{
		rt &= papuga_Serialization_pushValue( self, &value);
	}
	return rt;
}

struct MergeResult
{
	enum {MaxDepth=16,MaxKeyLen=64};
	struct {
		char name[ MaxKeyLen];
		std::size_t namelen;
	} keyar[ MaxDepth];
	int keyarsize;
	const RequestVariable* var;
	papuga_SerializationIter iter;
	bool used;

	void init()
	{
		keyarsize = 0;
		var = 0;
		papuga_init_SerializationIter_empty( &iter);
		used = false;
	}
};

enum {MergeResultBufSize=64};

struct MergeResultIter
{
	MergeResult* result;
	int keyitr;
	bool active;

	bool more() const					{return keyitr+1 < result->keyarsize;}
	void next()						{++keyitr;}
	const char* name() const				{return result->keyar[ keyitr].name;}
	std::size_t namelen() const				{return result->keyar[ keyitr].namelen;}
	void init( MergeResult* res)				{result = res; keyitr=0;}
	bool valid() const					{return !!result;}
	bool hasResult() const					{return !!result && active;}
	void clear()						{init(0);}
	void inactivate()					{active=false;}
	void activate()						{active=true;}

	explicit MergeResultIter( MergeResult* res=0)		:result(res),keyitr(0),active(!!res){}
	MergeResultIter( const MergeResultIter& o)		:result(o.result),keyitr(o.keyitr),active(o.active){}
	MergeResultIter& operator=( const MergeResultIter& o)	{result=o.result; keyitr=o.keyitr; active=o.active; return *this;}
};

static bool mergeResultAddKeyPart( MergeResult& mr, const char* key, std::size_t keylen, papuga_ErrorCode* errcode)
{
	if (mr.keyarsize >= MergeResult::MaxDepth || keylen >= MergeResult::MaxKeyLen)
	{
		*errcode = papuga_BufferOverflowError;
		return false;
	}
	std::memcpy( mr.keyar[ mr.keyarsize].name, key, mr.keyar[ mr.keyarsize].namelen = keylen);
	mr.keyar[ mr.keyarsize].name[ keylen] = 0;
	++mr.keyarsize;
	return true;
}

static bool initMergeResult( MergeResult& mr, const char* key, std::size_t keylen, const RequestVariable* var, papuga_ErrorCode* errcode)
{
	char const* ki = key;
	const char* ke = key + keylen;
	char const* kn = (const char*)std::memchr( ki, '/', ke-ki);
	for (; kn; ki=kn+1,kn=(const char*)std::memchr( ki, '/', ke-ki))
	{
		if (!mergeResultAddKeyPart( mr, ki, kn-ki, errcode)) return false;
	}
	if (!mergeResultAddKeyPart( mr, ki, ke-ki, errcode)) return false;

	mr.var = var;
	if (var->isArray)
	{
		if (var->value.valuetype != papuga_TypeSerialization) return false;
		papuga_init_SerializationIter( &mr.iter, var->value.value.serialization);
	}
	return true;
}

static bool initMergeResultArray( MergeResult* mr, std::size_t mrbufsize, std::size_t* mrsize, const RequestVariableMap& varmap, papuga_ErrorCode* errcode)
{
	mr[ *mrsize = 0].init();

	RequestVariableMap::const_iterator vi = varmap.begin(), ve = varmap.end();
	for (; vi != ve; ++vi)
	{
		if (isLocalVariable( vi->ptr->name) || vi->inheritcnt > 0 || vi->ptr->value.valuetype == papuga_TypeHostObject) continue;
		if (*mrsize >= mrbufsize)
		{
			*errcode = papuga_BufferOverflowError;
			return false;
		}
		else if (!initMergeResult( mr[ *mrsize], vi->ptr->name, std::strlen(vi->ptr->name), vi->ptr.get(), errcode))
		{
			return false;
		}
		mr[ ++*mrsize].init();
	}
	return true;
}

static MergeResult* findMergeResult( MergeResult* mr, std::size_t mrsize, const char* key, std::size_t keylen)
{
	std::size_t mi = 0;
	for (; mi < mrsize; ++mi)
	{
		if (keylen == mr[mi].keyar[0].namelen && 0==std::memcmp( mr[mi].keyar[0].name, key, keylen)) return &mr[ mi];
	}
	return NULL;
}

static bool serializeMergeResultValue( papuga_Serialization* self, MergeResultIter& resitr, bool all, papuga_ErrorCode* errcode)
{
	bool rt = true;
	MergeResult& res = *resitr.result;
	if (res.var->isArray)
	{
		if (all)
		{
			rt &= papuga_Serialization_pushOpen( self);
			while (!papuga_SerializationIter_eof( &res.iter))
			{
				papuga_ValueVariant* value = papuga_SerializationIter_value( &res.iter);
#ifdef PAPUGA_LOWLEVEL_DEBUG
				if (!papuga_ValueVariant_isvalid( value))
				{
					*errcode = papuga_LogicError;
					return false;
				}
#endif
				papuga_SerializationIter_skip( &res.iter);
				rt &= serializeResultValue( self, *value, errcode);
			}
			rt &= papuga_Serialization_pushClose( self);
		}
		else if (!papuga_SerializationIter_eof( &res.iter))
		{
			papuga_ValueVariant* value = papuga_SerializationIter_value( &res.iter);
			papuga_SerializationIter_skip( &res.iter);
			rt &= serializeResultValue( self, *value, errcode);
		}
		else
		{
			*errcode = papuga_ValueUndefined;
			rt = false;
		}
	}
	else if (!res.used)
	{
		res.used = true;
		rt &= serializeResultValue( self, res.var->value, errcode);
	}
	else
	{
		*errcode = papuga_ValueUndefined;
		rt = false;
	}
	return rt;
}

static bool serializeMergeResult( papuga_Serialization* self, MergeResultIter& resitr, bool all, papuga_ErrorCode* errcode)
{
	bool rt = true;
	int tagcnt = 0;
	while (resitr.more())
	{
		rt &= papuga_Serialization_pushName_string( self, resitr.name(), resitr.namelen());
		rt &= papuga_Serialization_pushOpen( self);
		resitr.next();
		++tagcnt;
	}
	rt &= papuga_Serialization_pushName_string( self, resitr.name(), resitr.namelen());
	rt &= serializeMergeResultValue( self, resitr, all, errcode);
	for (; tagcnt; --tagcnt)
	{
		rt &= papuga_Serialization_pushClose( self);
	}
	if (!rt && *errcode == papuga_Ok)
	{
		*errcode = papuga_NoMemError;
	}
#ifdef PAPUGA_LOWLEVEL_DEBUG
	papuga_ValueVariant serval;
	papuga_init_ValueVariant_serialization( &serval, self);
	if (!papuga_ValueVariant_isvalid( &serval))
	{
		*errcode = papuga_LogicError;
		return false;
	}
#endif
	return rt;
}

static bool testMergeResultUsed( const MergeResult* mr, std::size_t mrsize)
{
	std::size_t mi = 0;
	for (; mi < mrsize; ++mi)
	{
		if (mr[mi].var->isArray)
		{
			if (!papuga_SerializationIter_eof( &mr[mi].iter)) return false;
		}
		else
		{
			if (!mr[mi].used) return false;
		}
	}
	return true;
}

static bool serializeMergeResultArray( papuga_Serialization* self, MergeResultIter* ar, std::size_t asize, papuga_ErrorCode* errcode)
{
	bool rt = true;
	std::size_t ai = 0, ae = asize;
	for (ai=0; rt && ai != ae; ++ai)
	{
		if (ar[ai].valid())
		{
			MergeResultIter mergeResultIterFollow[ MergeResultBufSize];
			const char* name = ar[ ai].name();
			std::size_t namelen = ar[ ai].namelen();
	
			std::size_t ni = 0, fi = ai;
			mergeResultIterFollow[ ni++] = ar[ai];
			ar[ai].clear();
			for (++fi; fi != ae; ++fi)
			{
				if (ar[fi].valid() && ar[fi].namelen() == namelen && 0==std::memcmp( ar[fi].name(), name, namelen))
				{
					mergeResultIterFollow[ ni++] = ar[fi];
					ar[fi].clear();
				}
			}
			if (ni == 1)
			{
				rt &= serializeMergeResult( self, mergeResultIterFollow[0], true/*all*/, errcode);
			}
			else
			{
				rt &= papuga_Serialization_pushName_string( self, name, namelen);
				rt &= papuga_Serialization_pushOpen( self);

				std::size_t ci = 0, ce = ni;
				for (; ci != ce; ++ci)
				{
					if (!mergeResultIterFollow[ ci].more())
					{
						*errcode = papuga_MixedConstruction;
						rt = false;
					}
					else
					{
						mergeResultIterFollow[ ci].next();
					}
				}
				if (rt)
				{
					rt &= serializeMergeResultArray( self, mergeResultIterFollow, ni, errcode);
				}
				rt &= papuga_Serialization_pushClose( self);
			}
		}
	}
	return rt;
}


extern "C" bool papuga_Serialization_serialize_request_result( papuga_Serialization* self, const papuga_RequestContext* context, const papuga_Request* request, papuga_ErrorCode* errcode)
{
	MergeResult mergeResult[ MergeResultBufSize];
	MergeResultIter mergeResultIter[ MergeResultBufSize];
	std::size_t mergeResultSize = 0;
	bool rt = initMergeResultArray( mergeResult, MergeResultBufSize, &mergeResultSize, context->varmap, errcode);

	if (rt)
	{
		std::size_t mi = 0, me = mergeResultSize;
		for (; mi != me; ++mi)
		{
			mergeResultIter[ mi].init( mergeResult+mi);
		}
		rt &= serializeMergeResultArray( self, mergeResultIter, mergeResultSize, errcode);
	}
	if (!rt && *errcode != papuga_Ok)
	{
		*errcode = papuga_NoMemError;
	}
	return rt;
}

struct MergeStackElement
{
	MergeResultIter iter;
	bool inArray;

	MergeStackElement( const MergeResultIter& iter_, bool inArray_)
		:iter(iter_),inArray(inArray_){}
	MergeStackElement( const MergeStackElement& o)
		:iter(o.iter),inArray(o.inArray){}
};

struct MergeStack
{
public:
	MergeStack( papuga_ErrorCode* errcode_, MergeResult* mergeResult_, std::size_t mergeResultSize_)
		:errcode(errcode_),mergeResult(mergeResult_),mergeResultSize(mergeResultSize_),ar(),mergeResultMatch(0){}

	bool push()
	{
		// Open without name is an array element. An array element inherits the properties of the parent for the merge
		try
		{
			MergeResultIter follow( ar.back().iter);
			bool inArray = ar.back().inArray;
			ar.back().iter.inactivate();
			follow.activate();
			ar.push_back( MergeStackElement( follow, inArray));
			return true;
		}
		catch (const std::bad_alloc&)
		{
			*errcode = papuga_NoMemError;
			return false;
		}
	}

	bool push( const papuga_ValueVariant& name, bool inArray)
	{
		try
		{
			if (!ar.empty())
			{
				inArray |= ar.back().inArray;
			}
			mergeResultMatch.clear();
			if (name.valuetype == papuga_TypeString && name.encoding == papuga_UTF8)
			{
				if (!ar.empty() && ar.back().iter.valid() && ar.back().iter.namelen() == name.length && 0==std::memcmp( name.value.string, ar.back().iter.name(), name.length))
				{
					if (ar.back().iter.more())
					{
						MergeResultIter follow( ar.back().iter);
						follow.activate();
						follow.next();
						ar.back().iter.clear();
						ar.push_back( MergeStackElement( follow, inArray));
					}
					else
					{
						mergeResultMatch = ar.back().iter;
						ar.back().iter.clear();
						ar.push_back( MergeStackElement( MergeResultIter(), inArray));
					}
				}
				else
				{
					MergeResult* res = findMergeResult( mergeResult, mergeResultSize, name.value.string, name.length);
					if (res)
					{
						MergeResultIter follow( res);
						if (follow.more())
						{
							follow.activate();
							follow.next();
							ar.push_back( MergeStackElement( follow, inArray));
						}
						else
						{
							mergeResultMatch = follow;
							ar.push_back( MergeStackElement( MergeResultIter(), inArray));
						}
					}
					else
					{
						ar.push_back( MergeStackElement( MergeResultIter(), inArray));
					}
				}
			}
			else
			{
				ar.push_back( MergeStackElement( MergeResultIter(), inArray));
			}
			return true;
		}
		catch (const std::bad_alloc&)
		{
			*errcode = papuga_NoMemError;
			return false;
		}
	}

	MergeResultIter& match()
	{
		return mergeResultMatch;
	}

	bool isInArray() const
	{
		return !ar.empty() && ar.back().inArray;
	}

	bool isRoot() const
	{
		return ar.size() <= 1;
	}

	bool pop()
	{
		if (ar.empty())
		{
			mergeResultMatch.clear(); 
			*errcode = papuga_SyntaxError;
			return false;
		}
		else
		{
			mergeResultMatch = ar.back().iter;
			ar.pop_back();
			return true;
		}
	}

	bool empty() const
	{
		return ar.empty();
	}

private:
	papuga_ErrorCode* errcode;
	MergeResult* mergeResult;
	std::size_t mergeResultSize;
	std::vector<MergeStackElement> ar;
	MergeResultIter mergeResultMatch;
};

extern "C" bool papuga_Serialization_merge_request_result( papuga_Serialization* self, const papuga_RequestContext* context, const papuga_Request* request, papuga_StringEncoding input_encoding, papuga_ContentType input_doctype, const char* input_content, size_t input_contentlen, papuga_ErrorCode* errcode)
{
	MergeResult mergeResult[ MergeResultBufSize];
	std::size_t mergeResultSize = 0;
	if (!initMergeResultArray( mergeResult, MergeResultBufSize, &mergeResultSize, context->varmap, errcode))
	{
		return false;
	}
	papuga_Serialization inputser;
	papuga_init_Serialization( &inputser, self->allocator);
	papuga_SerializationIter inputitr;
	papuga_ValueVariant* nameval = NULL;
	
	switch (input_doctype)
	{
		case papuga_ContentType_Unknown:
			*errcode = papuga_NotImplemented;
			return false;
		case papuga_ContentType_XML:
			if (!papuga_Serialization_append_xml( &inputser, input_content, input_contentlen, input_encoding, false/*without root*/, true/*ignoreEmptyContent*/, errcode)) return false;
			break;
		case papuga_ContentType_JSON:
			if (!papuga_Serialization_append_json( &inputser, input_content, input_contentlen, input_encoding, false/*without root*/, errcode)) return false;
			break;
	}
	bool rt = true;
	papuga_init_SerializationIter( &inputitr, &inputser);
	MergeStack stk( errcode, mergeResult, mergeResultSize);

	while (rt && !papuga_SerializationIter_eof( &inputitr))
	{
		papuga_Tag tg = papuga_SerializationIter_tag( &inputitr);
#ifdef PAPUGA_LOWLEVEL_DEBUG
		const papuga_ValueVariant* tgval = papuga_SerializationIter_value( &inputitr);
		std::cerr << "TAG " << papuga_Tag_name( tg) << " " << papuga::ValueVariant_tostring( *tgval, *errcode) << std::endl;
#endif
		switch (tg)
		{
			case papuga_TagValue:
				if (nameval)
				{
					bool inArray = stk.isInArray();

					rt &= stk.push( *nameval, false);
					if (stk.match().valid())
					{
						rt &= serializeMergeResult( self, stk.match(), !inArray/*all*/, errcode);
						rt &= stk.pop();
					}
					else
					{
						rt &= stk.pop();
						if (stk.match().valid())
						{
							rt &= papuga_Serialization_pushName( self, nameval);
							rt &= serializeMergeResult( self, stk.match(), !inArray/*all*/, errcode);
						}
						else
						{
							rt &= papuga_Serialization_pushName( self, nameval);
							rt &= papuga_Serialization_pushValue( self, papuga_SerializationIter_value( &inputitr));
						}
					}
				}
				else
				{
					rt &= papuga_Serialization_pushValue( self, papuga_SerializationIter_value( &inputitr));
				}
				papuga_SerializationIter_skip( &inputitr);
				nameval = NULL;
				break;
			case papuga_TagOpen:
				if (nameval)
				{
					bool openArray = papuga_SerializationIter_follow_tag( &inputitr) == papuga_TagOpen;
					bool inArray = stk.isInArray();

					rt &= stk.push( *nameval, openArray);
					if (stk.match().valid())
					{
						rt &= serializeMergeResult( self, stk.match(), !inArray/*all*/, errcode);
						papuga_SerializationIter_skip_structure( &inputitr);
						stk.pop();
					}
					else
					{
						rt &= papuga_Serialization_pushName( self, nameval);
						rt &= papuga_Serialization_push( self, tg, papuga_SerializationIter_value( &inputitr));
						papuga_SerializationIter_skip( &inputitr);
					}
				}
				else
				{
					rt &= stk.push();
					rt &= papuga_Serialization_push( self, tg, papuga_SerializationIter_value( &inputitr));
					papuga_SerializationIter_skip( &inputitr);
				}
				nameval = NULL;
				break;
			case papuga_TagClose:
			{
				rt &= stk.pop();
				bool inArray = stk.isInArray();

				if (stk.match().hasResult())
				{
					rt &= serializeMergeResult( self, stk.match(), !inArray/*all*/, errcode);
				}
				rt &= papuga_Serialization_pushClose( self);
				papuga_SerializationIter_skip( &inputitr);
				nameval = NULL;
				break;
			}
			case papuga_TagName:
				nameval = papuga_SerializationIter_value( &inputitr);
				papuga_SerializationIter_skip( &inputitr);
				break;
		}
	}
	if (rt && !testMergeResultUsed( mergeResult, mergeResultSize))
	{
		*errcode = papuga_ValueUndefined;
		rt = false;
	}
	if (!rt && *errcode == papuga_Ok)
	{
		*errcode = papuga_NoMemError;
	}
#ifdef PAPUGA_LOWLEVEL_DEBUG
	if (rt)
	{
		std::cerr << "RESULT " << papuga::Serialization_tostring( *self, true/*linemode*/, 30/*maxdepth*/, *errcode) << std::endl;
	}
#endif
	return rt;
}

