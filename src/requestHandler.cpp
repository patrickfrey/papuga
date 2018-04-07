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
#include <unordered_map>
#include <stdexcept>
#include <cstdio>
#include <iostream>
#include <sstream>

/* @brief Hook for GETTEXT */
#define _TXT(x) x

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

struct RequestSchemeList
{
	struct RequestSchemeList* next;			/*< pointer next element in the single linked list */
	const char* type;				/*< type name of the context this scheme is valid for */
	const char* name;				/*< name of the scheme */
	const papuga_RequestAutomaton* automaton;	/*< automaton of the scheme */
};

struct RequestMethodList
{
	struct RequestMethodList* next;			/*< pointer next element in the single linked list */
	const char* name;				/*< name of the scheme */
	papuga_RequestMethodDescription method;		/*< method description */
};

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
	int inheritcnt;					/*< counter how many times variable value has been inherited */
	int allocatormem[ 128];				/*< memory buffer used for allocator */

private:
	void init()
	{
		papuga_init_Allocator( &allocator, allocatormem, sizeof(allocatormem));
		name = NULL;
		papuga_init_ValueVariant( &value);
		inheritcnt = 0;
	}
};

typedef papuga::shared_ptr<RequestVariable> RequestVariableRef;

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

	void add( const RequestVariableRef& var)
	{
		m_impl.push_back( var);
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
			if (findVariable( vi->get()->name)) return false;
			add( *vi);
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
		std::vector<RequestVariableRef>::iterator vi = m_impl.begin(), ve = m_impl.end();
		while (vi != ve)
		{
			if (isLocalVariable( vi->get()->name))
			{
				m_impl.erase( vi);
			}
			else
			{
				++vi;
			}
		}
	}
	const papuga_ValueVariant* findVariable( const char* name) const
	{
		std::vector<RequestVariableRef>::const_iterator vi = m_impl.begin(), ve = m_impl.end();
		for (; vi != ve; ++vi) if (0==std::strcmp( vi->get()->name, name)) break;
		return (vi == ve) ? NULL : &vi->get()->value;
	}
	RequestVariableRef getOrCreate( const char* name)
	{
		std::vector<RequestVariableRef>::const_iterator vi = m_impl.begin(), ve = m_impl.end();
		for (; vi != ve; ++vi) if (0==std::strcmp( vi->get()->name, name)) break;
		if (vi == ve)
		{
			RequestVariableRef rt( new RequestVariable( name));
			add( rt);
			return rt;
		}
		else
		{
			return *vi;
		}
	}
	const char** listVariables( int max_inheritcnt, char const** buf, size_t bufsize) const
	{
		size_t bufpos = 0;
		std::vector<RequestVariableRef>::const_iterator vi = m_impl.begin(), ve = m_impl.end();
		for (; vi != ve; ++vi)
		{
			if (bufpos >= bufsize) return NULL;
			if (max_inheritcnt >= 0 && vi->get()->inheritcnt > max_inheritcnt) continue;
			buf[ bufpos++] = vi->get()->name;
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
	papuga_ErrorCode errcode;		/*< last error in the request context */
	RequestVariableMap varmap;		/*< map of variables defined in this context */

	papuga_RequestContext()
		:errcode(papuga_Ok),varmap(){}
	papuga_RequestContext( const papuga_RequestContext& o)
		:errcode(o.errcode),varmap(o.varmap){}
	~papuga_RequestContext(){}

	std::string tostring() const
	{
		std::ostringstream out;
		RequestVariableMap::const_iterator vi = varmap.begin(), ve = varmap.end();
		for (; vi != ve; ++vi)
		{
			papuga_ErrorCode errcode_local = papuga_Ok;
			out << (*vi)->name << "=" << papuga::ValueVariant_tostring( (*vi)->value, errcode_local) << std::endl;
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
	RequestSchemeList* schemes;
	RequestMethodList** classmethodmap;
	int classmethodmapsize;
	const papuga_ClassDef* classdefs;
	papuga_Allocator allocator;
	int allocator_membuf[ 1024];

	explicit papuga_RequestHandler( const papuga_ClassDef* classdefs_)
		:contextmap(new RequestContextMap()),schemes(NULL),classmethodmap(NULL),classmethodmapsize(nofClassDefs(classdefs_)),classdefs(classdefs_)
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

extern "C" bool papuga_RequestContext_add_variable( papuga_RequestContext* self, const char* name, papuga_ValueVariant* value)
{
	try
	{
		RequestVariableRef varref = self->varmap.getOrCreate( name);
		return papuga_Allocator_deepcopy_value( &varref->allocator, &varref->value, value, true/*movehostobj*/, &self->errcode);
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

extern "C" bool papuga_RequestHandler_destroy_context( papuga_RequestHandler* self, const char* type, const char* name, papuga_ErrorCode* errcode)
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

extern "C" bool papuga_RequestHandler_add_scheme( papuga_RequestHandler* self, const char* type, const char* name, const papuga_RequestAutomaton* automaton)
{
	RequestSchemeList* listitem = alloc_type<RequestSchemeList>( &self->allocator);
	if (!listitem) return false;
	listitem->type = papuga_Allocator_copy_charp( &self->allocator, type);
	listitem->name = papuga_Allocator_copy_charp( &self->allocator, name);
	if (!listitem->type || !listitem->name) return false;
	listitem->automaton = automaton;
	self->schemes = add_list( self->schemes, listitem);
	return true;
}

static RequestSchemeList* find_scheme( RequestSchemeList* ll, const char* type, const char* name)
{
	for (; ll && (0!=std::strcmp( ll->name, name) || 0!=std::strcmp( ll->type, type)); ll = ll->next){}
	return ll;
}

static const char** RequestHandler_list_all_schemes( const papuga_RequestHandler* self, char const** buf, size_t bufsize)
{
	size_t bufpos = 0;
	RequestSchemeList const* sl = self->schemes;
	for (; sl; sl = sl->next)
	{
		size_t bi = 0;
		for (; bi < bufpos && 0!=std::strcmp(buf[bi],sl->type); ++bi){}
		if (bi < bufpos) continue;
		if (bufpos >= bufsize) return NULL;
		buf[ bufpos++] = sl->name;
	}
	if (bufpos >= bufsize) return NULL;
	buf[ bufpos] = NULL;
	return buf;
}

extern "C" const char** papuga_RequestHandler_list_schemes( const papuga_RequestHandler* self, const char* type, char const** buf, size_t bufsize)
{
	size_t bufpos = 0;
	RequestSchemeList const* sl = self->schemes;

	if (!type)
	{
		return RequestHandler_list_all_schemes( self, buf, bufsize);
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

extern "C" const papuga_RequestAutomaton* papuga_RequestHandler_get_scheme( const papuga_RequestHandler* self, const char* type, const char* name)
{
	const RequestSchemeList* sl = find_scheme( self->schemes, type?type:"", name);
	return sl ? sl->automaton : NULL;
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
	for (; ml && ml->method.has_content == with_content && 0!=std::strcmp( ml->name, name); ml = ml->next){}
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
		if (call->argcnt >= 0)
		{
			papuga_ErrorBuffer_reportError( errorbuf, _TXT( "error resolving argument %d of the method %s->%s::%s: %s"), call->argcnt+1, call->selfvarname, classname, methodname, msg);
		}
		else
		{
			papuga_ErrorBuffer_reportError( errorbuf, _TXT( "error for object '%s': %s"), call->selfvarname, msg);
		}
	}
	else
	{
		if (call->argcnt >= 0)
		{
			papuga_ErrorBuffer_reportError( errorbuf, _TXT( "error resolving argument %d of the constructor of %s: %s"), call->argcnt+1, classname, msg);
		}
		else
		{
			papuga_ErrorBuffer_reportError( errorbuf, _TXT( "error creating '%s': %s"), call->resultvarname, msg);
		}
	}
}

static bool RequestVariable_add_result( RequestVariableRef& var, papuga_ValueVariant& resultvalue, bool appendresult, papuga_ErrorCode& errcode)
{
	if (appendresult)
	{
		if (!papuga_ValueVariant_defined( &var->value))
		{
			papuga_Serialization* ser = papuga_Allocator_alloc_Serialization( &var->allocator);
			if (!ser) throw std::bad_alloc();
			papuga_init_ValueVariant_serialization( &var->value, ser);
		}
		if (var->value.valuetype == papuga_TypeSerialization)
		{
			if (!papuga_Serialization_pushValue( var->value.value.serialization, &resultvalue)) throw std::bad_alloc();
		}
		else
		{
			errcode = papuga_MixedConstruction;
			return false;
		}
	}
	else
	{
		papuga_init_ValueVariant_value( &var->value, &resultvalue);
	}
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
			return false;
		}
		papuga_init_ErrorBuffer( &errorbuf_call, membuf_err, sizeof(membuf_err));
		const papuga_RequestMethodCall* call;

		while (!!(call = papuga_RequestIterator_next_call( itr, context)))
		{
			RequestVariableRef var;
			if (call->resultvarname)
			{
				var = context->varmap.getOrCreate( call->resultvarname);
			}
			else
			{
				var.reset( new RequestVariable());
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
					*errorpos = call->eventcnt;
					papuga_destroy_RequestIterator( itr);
					return false;
				}
				// [2] Assign the result value to a variant type variable:
				papuga_Deleter destroy_hobj = classdefs[ call->methodid.classid-1].destructor;
				papuga_HostObject* hobj = papuga_Allocator_alloc_HostObject( &var->allocator, call->methodid.classid, self, destroy_hobj);
				if (!hobj)
				{
					destroy_hobj( self);
					throw std::bad_alloc();
				}
				papuga_ValueVariant resultvalue;
				papuga_init_ValueVariant_hostobj( &resultvalue, hobj);

				// [3] Add the result to the result variable:
				if (!RequestVariable_add_result( var, resultvalue, call->appendresult, errcode))
				{
					break;
				}

				// [4] Log the call:
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
				papuga_init_CallResult( &retval, &var->allocator, false/*ownership*/, membuf_err, sizeof(membuf_err));
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
					*errorpos = call->eventcnt;
					papuga_destroy_RequestIterator( itr);
					return false;
				}
				// [3] Fetch the result(s) if required (stored as variable):
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
				if (!RequestVariable_add_result( var, resultvalue, call->appendresult, errcode))
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
		}
		// Report error if we could not resolve all parts of a method call:
		if (errcode == papuga_Ok)
		{
			errcode = papuga_RequestIterator_get_last_error( itr, &call);
		}
		if (errcode != papuga_Ok)
		{
			reportMethodCallError( errorbuf, request, call, papuga_ErrorCode_tostring( errcode));
			*errorpos = call->eventcnt;
			papuga_destroy_RequestIterator( itr);
			return false;
		}
		papuga_destroy_RequestIterator( itr);
		return true;
	}
	catch (const std::exception& err)
	{
		if (itr) papuga_destroy_RequestIterator( itr);
		papuga_ErrorBuffer_reportError( errorbuf, _TXT("error handling request: %s"), err.what());
		return false;
	}
}

extern "C" bool papuga_Serialization_serialize_request_result( papuga_Serialization* self, papuga_RequestContext* context, const papuga_Request* request)
{
	bool rt = true;
	RequestVariableMap::const_iterator vi = context->varmap.begin(), ve = context->varmap.end();
	for (; vi != ve; ++vi)
	{
		if (isLocalVariable( (*vi)->name) || (*vi)->inheritcnt > 0 || (*vi)->value.valuetype == papuga_TypeHostObject) continue;
		rt &= papuga_Serialization_pushName_charp( self, (*vi)->name);
		rt &= papuga_Serialization_pushValue( self, &(*vi)->value);
	}
	return rt;
}


