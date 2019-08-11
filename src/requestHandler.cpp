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

typedef papuga::shared_ptr<RequestVariableMap> RequestVariableMapRef;

/*
 * @brief Defines the context of a request
 */
struct papuga_RequestContext
{
	std::string classname;
	papuga_ErrorCode errcode;		//< last error in the request context
	RequestVariableMap varmap;		//< map of variables defined in this context

	explicit papuga_RequestContext( const char* classname_)
		:classname(classname_?classname_:""),errcode(papuga_Ok),varmap()
	{
	}
	~papuga_RequestContext()
	{}

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

static void assignErrMethod( papuga_RequestError& errstruct, const papuga_RequestMethodId& mid, const papuga_ClassDef* classdefs)
{
	if (mid.classid)
	{
		errstruct.classname = classdefs[ mid.classid-1].name;
		if (mid.functionid)
		{
			errstruct.methodname = classdefs[ mid.classid-1].methodnames[ mid.functionid-1];
		}
		else
		{
			errstruct.methodname = 0;
		}
	}
	else
	{
		errstruct.classname = 0;
		errstruct.methodname = 0;
	}
}

static void assignErrMessage( papuga_RequestError& errstruct, const char* msg)
{
	std::size_t msgsize = std::strlen(msg);
	if (msgsize >= sizeof( errstruct.errormsg)) msgsize = sizeof( errstruct.errormsg)-1;
	std::memcpy( errstruct.errormsg, msg, msgsize);
}

extern "C" bool papuga_RequestContext_execute_request( papuga_RequestContext* context, const papuga_Request* request, papuga_Allocator* allocator, papuga_RequestLogger* logger, papuga_RequestResult** results, int* nofResults, papuga_RequestError* errstruct)
{
	papuga_RequestIterator* itr = 0;
	*results = 0;
	*nofResults = 0;
	std::memset( errstruct, 0, sizeof(*errstruct));

	try
	{
		papuga_ErrorBuffer errorbuf_call;

		const papuga_ClassDef* classdefs = papuga_Request_classdefs( request);
		itr = papuga_create_RequestIterator( allocator, request, &errstruct->errcode);
		if (!itr) return false;

		papuga_init_ErrorBuffer( &errorbuf_call, errstruct->errormsg, sizeof(errstruct->errormsg));

		const papuga_RequestMethodCall* call;
		while (!!(call = papuga_RequestIterator_next_call( itr, context)))
		{
			// Execute all method calls and variable assignments:
			if (call->methodid.classid == 0)
			{
				// [A] Variable assignment:
				// [A.0] Create the result variable
				if (!call->resultvarname || call->args.argc != 1)
				{
					errstruct->errcode = papuga_ValueUndefined;
					papuga_destroy_RequestIterator( itr);
					return false;
				}
				RequestVariable* var = NULL;
				if (!papuga_Request_is_result_variable( request, call->resultvarname))
				{
					// ... HACK const_cast: We know that the source is only modified by the following deepcopy_value in the case 
					//	of a host object moved. But we know that a variable assignment is constructed from content and cannot
					//	contain host object references.
					papuga_ValueVariant* source = const_cast<papuga_ValueVariant*>( &call->args.argv[0]);

					var = context->varmap.createVariable( call->resultvarname);
					if (!papuga_Allocator_deepcopy_value( &var->allocator, &var->value, source, false/*movehostobj*/, &errstruct->errcode))
					{
						errstruct->variable = call->resultvarname;
						papuga_destroy_RequestIterator( itr);
						return false;
					}
				}
				else
				{
					// Add the result to be substituted in the result content template:
					(void)papuga_RequestIterator_push_call_result( itr, &call->args.argv[0]);
				}
				// [A.1] Log the assignment if logging enabled
				if (logger->logMethodCall)
				{
					(*logger->logMethodCall)( logger->self, 2,
						papuga_LogItemResultVariable, call->resultvarname,
						papuga_LogItemResult, &call->args.argv[0]);
				}
			}
			else if (call->methodid.functionid == 0)
			{
				// [B] Constructor call:
				// [B.0] Create the result variable
				RequestVariable* var;
				if (call->resultvarname)
				{
					if (papuga_Request_is_result_variable( request, call->resultvarname))
					{
						errstruct->variable = call->resultvarname;
						errstruct->errcode = papuga_MixedConstruction;
						papuga_destroy_RequestIterator( itr);
						return false;
					}
					var = context->varmap.createVariable( call->resultvarname);
				}
				else
				{
					var = NULL;
				}
				// [B.1] Call the constructor
				const papuga_ClassConstructor func = classdefs[ call->methodid.classid-1].constructor;
				void* self;
	
				if (!(self=(*func)( &errorbuf_call, call->args.argc, call->args.argv)))
				{
					errstruct->errcode = papuga_HostObjectError;
					assignErrMethod( *errstruct, call->methodid, classdefs);
					papuga_destroy_RequestIterator( itr);
					return false;
				}
				// [B.2] Assign the result value to a variant type variable and log the call
				if (var)
				{
					papuga_Deleter destroy_hobj = classdefs[ call->methodid.classid-1].destructor;
					papuga_HostObject* hobj = papuga_Allocator_alloc_HostObject( &var->allocator, call->methodid.classid, self, destroy_hobj);
					if (!hobj)
					{
						destroy_hobj( self);
						papuga_destroy_RequestIterator( itr);
						errstruct->errcode = papuga_NoMemError;
						return false;
					}
					papuga_init_ValueVariant_hostobj( &var->value, hobj);
				}
				// [B.3] Log the call if logging enabled
				if (logger->logMethodCall)
				{
					if (var)
					{
						(*logger->logMethodCall)( logger->self, 5,
							papuga_LogItemClassName, classdefs[ call->methodid.classid-1].name,
							papuga_LogItemArgc, call->args.argc,
							papuga_LogItemArgv, &call->args.argv[0],
							papuga_LogItemResultVariable, call->resultvarname,
							papuga_LogItemResult, &var->value);
					}
					else
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
				// [C] Method call:
				RequestVariable* var;
				if (call->resultvarname && !papuga_Request_is_result_variable( request, call->resultvarname))
				{
					var = context->varmap.createVariable( call->resultvarname);
				}
				else
				{
					var = NULL;
				}
				// [C.1] Get the method and the object of the method to call
				const papuga_ClassMethod func = classdefs[ call->methodid.classid-1].methodtable[ call->methodid.functionid-1];
				const papuga_ValueVariant* selfvalue = context->varmap.findVariable( call->selfvarname);
				if (!selfvalue)
				{
					assignErrMethod( *errstruct, call->methodid, classdefs);
					errstruct->variable = call->selfvarname;
					errstruct->errcode = papuga_MissingSelf;
					break;
				}
				if (selfvalue->valuetype != papuga_TypeHostObject || selfvalue->value.hostObject->classid != call->methodid.classid)
				{
					assignErrMethod( *errstruct, call->methodid, classdefs);
					errstruct->variable = call->selfvarname;
					errstruct->errcode = papuga_TypeError;
					break;
				}
				// [C.2] Call the method and report an error on failure
				void* self = selfvalue->value.hostObject->data;
				papuga_CallResult retval;
				papuga_init_CallResult( &retval, var ? &var->allocator : allocator, false/*ownership*/, errstruct->errormsg, sizeof(errstruct->errormsg));
				if (!(*func)( self, &retval, call->args.argc, call->args.argv))
				{
					errstruct->errcode = papuga_HostObjectError;
					assignErrMethod( *errstruct, call->methodid, classdefs);
					errstruct->variable = call->selfvarname;
					break;
				}
				// [C.3] Build the result value
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
					papuga_Serialization* ser = papuga_Allocator_alloc_Serialization( var ? &var->allocator : allocator);
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
						errstruct->errcode = papuga_NoMemError;
						break;
					}
					papuga_init_ValueVariant_serialization( &resultvalue, ser);
				}
				// [C.4] Assign the result
				if (var)
				{
					// [C.4.1] Assign the result to the result variable
					papuga_init_ValueVariant_value( &var->value, &resultvalue);
				}
				else if (papuga_ValueVariant_defined( &resultvalue))
				{
					// [C.4.2] Add the result to be substituted in the result content template
					(void)papuga_RequestIterator_push_call_result( itr, &resultvalue);
				}
				// [C.5] Log the call if logging enabled
				if (logger->logMethodCall)
				{
					if (papuga_ValueVariant_defined( &resultvalue))
					{
						(*logger->logMethodCall)( logger->self, 6,
									papuga_LogItemClassName, classdefs[ call->methodid.classid-1].name,
									papuga_LogItemMethodName, classdefs[ call->methodid.classid-1].methodnames[ call->methodid.functionid-1],
									papuga_LogItemArgc, call->args.argc,
									papuga_LogItemArgv, &call->args.argv[0],
									papuga_LogItemResultVariable, call->resultvarname,
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
		const papuga_RequestError* itr_errstruct = papuga_RequestIterator_get_last_error( itr);
		if (errstruct->errcode != papuga_Ok)
		{
			papuga_destroy_RequestIterator( itr);
			return false;
		}
		else if (itr_errstruct)
		{
			std::memcpy( errstruct, itr_errstruct, sizeof(papuga_RequestError));
			papuga_destroy_RequestIterator( itr);
			return false;
		}
		*results = papuga_get_RequestResult_array( itr, allocator, nofResults);
		if (!*results)
		{
			itr_errstruct = papuga_RequestIterator_get_last_error( itr);
			if (itr_errstruct)
			{
				std::memcpy( errstruct, itr_errstruct, sizeof(papuga_RequestError));
				papuga_destroy_RequestIterator( itr);
				return false;
			}
		}
		papuga_destroy_RequestIterator( itr);
		return true;
	}
	catch (const std::bad_alloc& err)
	{
		if (itr) papuga_destroy_RequestIterator( itr);
		errstruct->errcode = papuga_NoMemError;
		return false;
	}
	catch (const std::runtime_error& err)
	{
		if (itr) papuga_destroy_RequestIterator( itr);
		errstruct->errcode = papuga_UncaughtException;
		assignErrMessage( *errstruct, err.what());
		return false;
	}
}



