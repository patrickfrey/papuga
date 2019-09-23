/*
 * Copyright (c) 2017 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
#ifndef _PAPUGA_REQUEST_AUTOMATON_HPP_INCLUDED
#define _PAPUGA_REQUEST_AUTOMATON_HPP_INCLUDED
/// \brief Structure to define an automaton mapping a request to function calls in C++ in a convenient way (using initializer lists in C++11)
/// \file requestAutomaton.hpp
#include "papuga/typedefs.h"
#include "papuga/classdef.h"
#include "papuga/request.h"
#include "papuga/schemaDescription.h"
#include <string>
#include <vector>
#include <set>
#include <utility>
#include <stdexcept>

namespace papuga {

static inline papuga_ResolveType getResolveType( char resolvechr)
{
	switch (resolvechr)
	{
		case '*': return papuga_ResolveTypeArray;
		case '+': return papuga_ResolveTypeArrayNonEmpty;
		case '?': return papuga_ResolveTypeOptional;
		case '!': return papuga_ResolveTypeRequired;
		case '$': return papuga_ResolveTypeInherited;
		default: throw std::runtime_error( "unknown resolve type identifier");
	}
}

/// \brief Request method call (function) definition or assignment of a variable if no method defined
struct RequestAutomaton_FunctionDef
{
	/// \brief Request function argument
	struct Arg
	{
		const char* varname;		///< name of the variable referencing the argument in case of a variable
		int itemid;			///< item identifier unique in its scope, in case of an item reference (a value or a structure)
		papuga_ResolveType resolvetype;	///< tells wheter the item is in a enclosing scope (true) or in an enclosed scope (false), in case of an item reference (a value or a structure)
		int max_tag_diff;		///< maximum reach of search in number of tag hierarchy levels or 0 if not limited (always >= 0 also for inherited values)

		Arg( const char* varname_)
			:varname(varname_),itemid(-1),resolvetype(papuga_ResolveTypeRequired),max_tag_diff(0){}
		Arg( int itemid_, char resolvechr, int max_tag_diff_=1)
			:varname(0),itemid(itemid_),resolvetype(getResolveType(resolvechr)),max_tag_diff(max_tag_diff_){}
		Arg( int itemid_)
			:varname(0),itemid(itemid_),resolvetype(papuga_ResolveTypeRequired),max_tag_diff(1){}
		Arg( const Arg& o)
			:varname(o.varname),itemid(o.itemid),resolvetype(o.resolvetype),max_tag_diff(o.max_tag_diff){}
	};
	const char* scope_expression;		///< selecting expression addressing the scope of this function definition, used to prioritize variable definitions over function definitions with the same target variable of the same scope or covering scope
	const char* select_expression;		///< select expression addressing the scope of this function definition
	const char* resultvar;			///< variable where the result of the call is stored to, empty if the result is void or dropped
	const char* selfvar;			///< variable addressing the object of the method call
	papuga_RequestMethodId methodid;	///< identifier of the method to call
	std::vector<Arg> args;			///< list of references addressing the arguments of the method call

	/// \brief Copy constructor
	RequestAutomaton_FunctionDef( const RequestAutomaton_FunctionDef& o)
		:scope_expression(o.scope_expression),select_expression(o.select_expression),resultvar(o.resultvar),selfvar(o.selfvar),args(o.args)
	{
		methodid.classid = o.methodid.classid;
		methodid.functionid = o.methodid.functionid;
	}
	/// \brief Constructor
	/// \param[in] expression_ select expression addressing the scope of this method call definition
	/// \param[in] resultvar_ variable where the result of the call is stored to, empty if the result is void or dropped
	/// \param[in] selfvar_ variable where the result of the call is stored to, empty if the result is void or dropped
	/// \param[in] methodid_ identifier of the method to call
	/// \param[in] args_ list of references addressing the arguments of the method call
	RequestAutomaton_FunctionDef( const char* scope_expression_, const char* select_expression_, const char* resultvar_, const char* selfvar_, const papuga_RequestMethodId& methodid_, const std::vector<Arg>& args_)
		:scope_expression(scope_expression_)
		,select_expression(select_expression_)
		,resultvar(resultvar_)
		,selfvar(selfvar_)
		,args(args_)
	{
		methodid.classid = methodid_.classid;
		methodid.functionid = methodid_.functionid;
	}
	/// \brief Add this method call definition to an automaton
	/// \param[in] rootexpr path prefix for selection expressions
	/// \param[in] atm automaton to add this definition to
	/// \param[in] descr schema description to add this definition to
	void addToAutomaton( const std::string& rootexpr, papuga_RequestAutomaton* atm, papuga_SchemaDescription* descr) const;

	/// \brief True if this function is prioritized among others in the given scope expression
	/// \note Prioritized means that other function calls in the given scope with the same target variable are suppressed
	/// \return true if yes
	bool prioritize() const
	{
		return methodid.classid ? false : true;
	}
};

/// \brief Request structure definition 
struct RequestAutomaton_StructDef
{
	/// \brief Structure element definition
	struct Element
	{
		const char* name;		///< name of the element or NULL in case of an array element
		int itemid;			///< identifier of the item addressing the element value
		papuga_ResolveType resolvetype;	///< describes the occurrence of the element
		int max_tag_diff;		///< maximum reach of search in number of tag hierarchy levels or 0 if not limited (always >= 0 also for inherited values)

		///\brief Constructor (named dictionary element)
		Element( const char* name_, int itemid_, char resolvechr, int max_tag_diff_=1)
			:name(name_),itemid(itemid_),resolvetype(getResolveType(resolvechr)),max_tag_diff(max_tag_diff_){}
		Element( const char* name_, int itemid_)
			:name(name_),itemid(itemid_),resolvetype(papuga_ResolveTypeRequired),max_tag_diff(1){}
		Element( int itemid_, char resolvechr, int max_tag_diff_=1)
			:name(NULL),itemid(itemid_),resolvetype(getResolveType(resolvechr)),max_tag_diff(max_tag_diff_){}
		Element( int itemid_)
			:name(NULL),itemid(itemid_),resolvetype(papuga_ResolveTypeRequired),max_tag_diff(1){}
		///\brief Copy constructor
		Element( const Element& o)
			:name(o.name),itemid(o.itemid),resolvetype(o.resolvetype),max_tag_diff(o.max_tag_diff){}
	};

	const char* expression;			///< selecting expression addressing the scope of this structure definition
	int itemid;				///< item identifier unique in its scope (referencing a value or a structure)
	std::vector<Element> elems;		///< list of references to the elements of this structure

	/// \brief Copy constructor
	RequestAutomaton_StructDef( const RequestAutomaton_StructDef& o)
		:expression(o.expression),itemid(o.itemid),elems(o.elems){}
	/// \brief Constructor
	/// \param[in] expression_ select expression addressing the scope of this structure definition
	/// \param[in] itemid_ item identifier unique in its scope (referencing a value or a structure)
	/// \param[in] elems_ list of references to the elements of this structure
	RequestAutomaton_StructDef( const char* expression_, int itemid_, const std::vector<Element>& elems_)
		:expression(expression_),itemid(itemid_),elems(elems_){}

	/// \brief Add this structure definition to an automaton
	/// \param[in] rootexpr path prefix for selection expressions
	/// \param[in] atm automaton to add this definition to
	/// \param[in] descr schema description to add this definition to
	void addToAutomaton( const std::string& rootexpr, papuga_RequestAutomaton* atm, papuga_SchemaDescription* descr) const;

	/// \brief Get a unique key of this definition for detecting duplicate definitions
	/// \param[in] rootexpr path prefix for selection expressions
	std::string key( const std::string& rootexpr) const;
};

/// \brief Request atomic value definition 
struct RequestAutomaton_ValueDef
{
	const char* scope_expression;	///< selecting expression addressing the scope of this value definition
	const char* select_expression;	///< selecting expression addressing the value itself
	int itemid;			///< identifier given to the item to make it addressable in the context of its scope
	papuga_Type valuetype;		///< expected value type of the item
	const char* examples;		///< semicolon ';' separated list of examples or NULL if no examples defined

	/// \brief Copy constructor
	RequestAutomaton_ValueDef( const RequestAutomaton_ValueDef& o)
		:scope_expression(o.scope_expression),select_expression(o.select_expression),itemid(o.itemid),valuetype(o.valuetype),examples(o.examples){}
	/// \brief Constructor
	/// \param[in] scope_expression_ selecting expression addressing the scope of this value definition
	/// \param[in] select_expression_ selecting expression addressing the value itself
	/// \param[in] itemid_ identifier given to the item to make it addressable in the context of its scope
	RequestAutomaton_ValueDef( const char* scope_expression_, const char* select_expression_, int itemid_, papuga_Type valuetype_, const char* examples_)
		:scope_expression(scope_expression_),select_expression(select_expression_),itemid(itemid_),valuetype(valuetype_),examples(examples_){}

	/// \brief Add this value definition to an automaton
	/// \param[in] rootexpr path prefix for selection expressions
	/// \param[in] atm automaton to add this definition to
	/// \param[in] descr schema description to add this definition to
	void addToAutomaton( const std::string& rootexpr, papuga_RequestAutomaton* atm, papuga_SchemaDescription* descr) const;

	/// \brief Get a unique key of this definition for detecting duplicate definitions
	/// \param[in] rootexpr path prefix for selection expressions
	std::string key( const std::string& rootexpr) const;
};

/// \brief Request grouping of functions definitions
struct RequestAutomaton_GroupDef
{
	struct Element
		:public RequestAutomaton_FunctionDef
	{
		Element( const char* expression_, const char* resultvar_, const char* selfvar_, papuga_RequestMethodId methodid_, std::vector<RequestAutomaton_FunctionDef::Arg> args_)
			:RequestAutomaton_FunctionDef( expression_, "", resultvar_, selfvar_, methodid_, args_){}
		Element( const Element& o)
			:RequestAutomaton_FunctionDef( o){}
	};

	std::vector<RequestAutomaton_FunctionDef> nodes;	///< list of function definitions belonging to this group

	/// \brief Copy constructor
	RequestAutomaton_GroupDef( const RequestAutomaton_GroupDef& o)
		:nodes(o.nodes){}
	/// \brief Constructor
	/// \param[in] nodes_ list of nodes of this group
	RequestAutomaton_GroupDef( const std::vector<Element>& nodes_)
		:nodes()
	{
		std::vector<Element>::const_iterator ni = nodes_.begin(), ne = nodes_.end();
		for (; ni != ne; ++ni) nodes.push_back( RequestAutomaton_FunctionDef( *ni));
	}

	/// \brief Add this group definition to an automaton
	/// \param[in] rootexpr path prefix for selection expressions
	/// \param[in] atm automaton to add this definition to
	/// \param[in] descr schema description to add this definition to
	void addToAutomaton( const std::string& rootexpr, papuga_RequestAutomaton* atm, papuga_SchemaDescription* descr) const;
};

/// \brief Definition of the resolve type for the schema description if not defined by other structures
struct RequestAutomaton_ResolveDef
{
	const char* expression;		///< selecting expression addressing the element
	papuga_ResolveType resolvetype;	///< describes the occurrence of the element
	
	/// \brief Constructor
	/// \param[in] expression_ selecting expression of the element
	/// \param[in] resolvechr_ describes the occurrence of the element
	RequestAutomaton_ResolveDef( const char* expression_, char resolvechr_)
		:expression(expression_),resolvetype(getResolveType(resolvechr_)){}

	/// \brief Add this definition to an automaton
	/// \param[in] rootexpr path prefix for selection expressions
	/// \param[in] atm automaton to add this definition to
	/// \param[in] descr schema description to add this definition to
	void addToAutomaton( const std::string& rootexpr, papuga_RequestAutomaton* atm, papuga_SchemaDescription* descr) const;
};

/// \class RequestAutomaton_ResultElementDef
/// \brief Result element declaration for using in C++ initializer lists
struct RequestAutomaton_ResultElementDef
{
	enum Type {
		Empty,
		Structure,
		Array,
		Constant,
		InputReference,
		ResultReference
	};
	Type type;
	papuga_ResolveType resolvetype;
	const char* inputselect;
	const char* tagname;
	int itemid;
	const char* str;

	RequestAutomaton_ResultElementDef( const RequestAutomaton_ResultElementDef& o)
		:type(o.type),resolvetype(o.resolvetype),inputselect(o.inputselect),tagname(o.tagname),itemid(o.itemid),str(o.str){}
	RequestAutomaton_ResultElementDef()
		:type(Empty),resolvetype(papuga_ResolveTypeRequired),inputselect(0),tagname(0),itemid(-1),str(0){}
	RequestAutomaton_ResultElementDef( const char* expression_, const char* tagname_, bool array_=false)
		:type(array_?Array:Structure),resolvetype(papuga_ResolveTypeRequired),inputselect(expression_),tagname(tagname_),itemid(-1),str(0){}
	RequestAutomaton_ResultElementDef( const char* expression_, const char* tagname_, const char* constant_)
		:type(Constant),resolvetype(papuga_ResolveTypeRequired),inputselect(expression_),tagname(tagname_),itemid(-1),str(constant_){}
	RequestAutomaton_ResultElementDef( const char* expression_, const char* tagname_, int itemid_, char resolvechr='!')
		:type(InputReference),resolvetype(getResolveType(resolvechr)),inputselect(expression_),tagname(tagname_),itemid(itemid_),str(0){}
	RequestAutomaton_ResultElementDef( const char* expression_, const char* tagname_, const char* varname_, char resolvechr='!')
		:type(ResultReference),resolvetype(getResolveType(resolvechr)),inputselect(expression_),tagname(tagname_),itemid(-1),str(varname_){}
};


#if __cplusplus >= 201103L
/// \brief Forward declaration
class RequestAutomaton_NodeList;

/// \class RequestAutomaton_Node
/// \brief Union of all RequestAutomaton_XXDef types for using in C++ initializer lists
struct RequestAutomaton_Node
{
	enum Type {
		Empty,
		Group,
		Function,
		Struct,
		Value,
		NodeList,
		ResolveDef
	};
	Type type;
	typedef union
	{
		RequestAutomaton_GroupDef* groupdef;
		RequestAutomaton_FunctionDef* functiondef;
		RequestAutomaton_StructDef* structdef;
		RequestAutomaton_ValueDef* valuedef;
		RequestAutomaton_NodeList* nodelist;
		RequestAutomaton_ResolveDef* resolvedef;
	} ValueUnion;
	ValueUnion value;
	std::string rootexpr;
	int thisid;

	///\brief Destructor
	~RequestAutomaton_Node();
	///\brief Default contructor
	RequestAutomaton_Node();
	///\brief Contructor as RequestAutomaton_GroupDef
	/// \param[in] nodelist_ list of nodes forming a group of functions
	RequestAutomaton_Node( const std::initializer_list<RequestAutomaton_GroupDef::Element>& nodes_);
	///\brief Contructor as RequestAutomaton_FunctionDef
	/// \param[in] expression select expression addressing the scope of this method call definition
	/// \param[in] resultvar variable where the result of the call is stored to, empty or NULL if the result is void or dropped
	/// \param[in] selfvar variable where the result of the call is stored to, empty if the result is void or dropped
	/// \param[in] methodid identifier of the method to call
	/// \param[in] args list of references addressing the arguments of the method call
	RequestAutomaton_Node( const char* expression, const char* resultvar, const char* selfvar, const papuga_RequestMethodId& methodid, const std::initializer_list<RequestAutomaton_FunctionDef::Arg>& args);
	/// \brief Contructor as RequestAutomaton_FunctionDef (assignment defined as function without call)
	/// \param[in] scope_expression selecting expression addressing the scope of this variable assignment definition, used to prioritize variable definitions over function definitions with the same target variable of the same scope or covering scope
	/// \param[in] select_expression select expression addressing the scope of this variable assignment definition
	/// \param[in] resultvar variable where the result of the call is stored to, empty or NULL if the result is void or dropped
	/// \param[in] itemid identifier given to the item to make it addressable in the context of its scope
	/// \param[in] resolvechr describes the occurrence of the element addressed
	/// \param[in] max_tag_diff maximum reach of search in number of tag hierarchy levels or 0 if not limited (always >= 0 also for inherited values)
	RequestAutomaton_Node( const char* scope_expression, const char* select_expression, const char* resultvar, int itemid, char resolvechr, int max_tag_diff=1);
	///\brief Contructor as RequestAutomaton_StructDef
	/// \param[in] expression select expression addressing the scope of this structure definition
	/// \param[in] itemid item identifier unique in its scope (referencing a value or a structure)
	/// \param[in] elems list of references to the elements of this structure
	RequestAutomaton_Node( const char* expression, int itemid, const std::initializer_list<RequestAutomaton_StructDef::Element>& elems);
	///\brief Contructor as RequestAutomaton_ValueDef
	/// \param[in] scope_expression selecting expression addressing the scope of this value definition
	/// \param[in] select_expression selecting expression addressing the value itself
	/// \param[in] itemid identifier given to the item to make it addressable in the context of its scope
	/// \param[in] valuetype type of the value
	/// \param[in] examples semicolon ';' separated list of examples or NULL if no examples defined
	RequestAutomaton_Node( const char* scope_expression, const char* select_expression, int itemid, papuga_Type valuetype, const char* examples);
	///\brief Contructor as RequestAutomaton_ResolveDef
	/// \param[in] expression select expression addressing the scope of this definition
	/// \param[in] resolvechr describes the occurrence of the element addressed
	RequestAutomaton_Node( const char* expression, char resolvechr);
	///\brief Contructor from list of predefined nodes (for sharing definitions)
	/// \param[in] nodelist list of nodes
	RequestAutomaton_Node( const RequestAutomaton_NodeList& nodelist);

	///\brief Move contructor
	RequestAutomaton_Node( RequestAutomaton_Node&& o);
	///\brief Move assignment
	RequestAutomaton_Node& operator=( RequestAutomaton_Node&& o);
	///\brief Copy contructor
	RequestAutomaton_Node( const RequestAutomaton_Node& o);
	///\brief Copy with path prefix
	RequestAutomaton_Node( const std::string& rootprefix, const RequestAutomaton_Node& o);
	///\brief Copy assignment
	RequestAutomaton_Node& operator=( const RequestAutomaton_Node& o);
	/// \brief Add this node definition to an automaton
	/// \param[in] path prefix for all selection expressions
	/// \param[in] atm automaton to add this definition to
	/// \param[in] descr schema description to add this definition to
	/// \param[in] keyset set of path expression for identifying duplicated definitions that would lead to errors hard to track if not catched.
	/// \param[in] accepted_root_tags set of accepted requests identified by their root tag name
	void addToAutomaton( const std::string& rootpath_, papuga_RequestAutomaton* atm, papuga_SchemaDescription* descr, std::set<std::string>& keyset, std::set<std::string>& accepted_root_tags) const;
};

class RequestAutomaton_NodeList
	:public std::vector<RequestAutomaton_Node>
{
public:
	RequestAutomaton_NodeList( const std::initializer_list<RequestAutomaton_Node>& nodes)
		:std::vector<RequestAutomaton_Node>( nodes.begin(), nodes.end()){}
	RequestAutomaton_NodeList( const char* rootexpr_, const std::initializer_list<RequestAutomaton_Node>& nodes)
	{
		std::initializer_list<RequestAutomaton_Node>::const_iterator ni = nodes.begin(), ne = nodes.end();
		for (; ni != ne; ++ni)
		{
			push_back( RequestAutomaton_Node( rootexpr_, *ni));
		}
	}
	RequestAutomaton_NodeList( const RequestAutomaton_NodeList& o)
		:std::vector<RequestAutomaton_Node>( o){}

	/// \brief Append an other node list (join lists)
	void append( const RequestAutomaton_NodeList& o)
	{
		insert( end(), o.begin(), o.end());
	}
};

#endif

class RequestAutomaton_ResultElementDefList
	:public std::vector<RequestAutomaton_ResultElementDef>
{
public:
#if __cplusplus >= 201103L
	RequestAutomaton_ResultElementDefList( const std::initializer_list<RequestAutomaton_ResultElementDef>& nodes)
		:std::vector<RequestAutomaton_ResultElementDef>( nodes.begin(), nodes.end()){}
#endif
	RequestAutomaton_ResultElementDefList()
		:std::vector<RequestAutomaton_ResultElementDef>(){}
	RequestAutomaton_ResultElementDefList( const std::vector<RequestAutomaton_ResultElementDef>& nodes)
		:std::vector<RequestAutomaton_ResultElementDef>( nodes.begin(), nodes.end()){}
	RequestAutomaton_ResultElementDefList( const RequestAutomaton_ResultElementDefList& o)
		:std::vector<RequestAutomaton_ResultElementDef>( o){}

	/// \brief Append an other node list (join lists)
	void append( const RequestAutomaton_ResultElementDefList& o)
	{
		insert( end(), o.begin(), o.end());
	}
};

class RequestAutomaton_ResultDef
{
public:
#if __cplusplus >= 201103L
	RequestAutomaton_ResultDef( const char* name_, const std::initializer_list<RequestAutomaton_ResultElementDef>& elements_)
		:m_name(name_),m_schema(0),m_requestmethod(0),m_addressvar(0),m_path(0),m_elements(elements_){}
	RequestAutomaton_ResultDef( const char* name_, const char* schema_, const char* requestmethod_, const char* addressvar_, const char* path_, const std::initializer_list<RequestAutomaton_ResultElementDef>& elements_)
		:m_name(name_),m_schema(schema_),m_requestmethod(requestmethod_),m_addressvar(addressvar_),m_path(path_),m_elements(elements_){}
#endif
	RequestAutomaton_ResultDef( const char* name_, const char* schema_, const char* requestmethod_, const char* addressvar_, const char* path_, const std::vector<RequestAutomaton_ResultElementDef>& elements_)
		:m_name(name_),m_schema(schema_),m_requestmethod(requestmethod_),m_addressvar(addressvar_),m_path(path_),m_elements(elements_){}
	RequestAutomaton_ResultDef( const char* name_)
		:m_name(name_),m_schema(0),m_requestmethod(0),m_addressvar(0),m_path(0),m_elements() {}
	RequestAutomaton_ResultDef( const RequestAutomaton_ResultDef& o)
		:m_name(o.m_name),m_schema(o.m_schema),m_requestmethod(o.m_requestmethod),m_addressvar(o.m_addressvar),m_path(o.m_path),m_elements(o.m_elements){}
	
	const char* name() const					{return m_name;}
	const char* schema() const					{return m_schema;}
	const char* requestmethod() const				{return m_requestmethod;}
	const char* addressvar() const					{return m_addressvar;}
	const char* path() const					{return m_path;}

	const RequestAutomaton_ResultElementDefList& elements() const	{return m_elements;}

	/// \brief Add this node definition to the automaton given as argument
	/// \param[in] atm automaton to add this definition to
	void addToAutomaton( papuga_RequestAutomaton* atm) const;

private:
	const char* m_name;
	const char* m_schema;
	const char* m_requestmethod;
	const char* m_addressvar;
	const char* m_path;
	RequestAutomaton_ResultElementDefList m_elements;
};


/// \brief Structure defining the mapping of requests of a certain type to a list of method calls
class RequestAutomaton
{
private:
	RequestAutomaton( const RequestAutomaton&)
		:m_atm(0),m_descr(0){}
	void operator=( const RequestAutomaton&) {}
	//... non copyable

public:
	/// \brief Constructor defining an empty automaton to be filled with further method calls
	/// \param[in] strict true, if strict checking is enabled, false, if the automaton accepts root tags that are not declared, used for parsing a structure embedded into a request (e.g. the main configuration)
	RequestAutomaton(
		const papuga_ClassDef* classdefs,
		const papuga_StructInterfaceDescription* structdefs,
		bool strict);

#if __cplusplus >= 201103L
	struct InheritedDef
	{
		std::string type;
		std::string name_expression;
		bool required;

		InheritedDef( const std::string& type_, const std::string& name_expression_, bool required_)
			:type(type_),name_expression(name_expression_),required(required_){}
		InheritedDef( const InheritedDef& o)
			:type(o.type),name_expression(o.name_expression),required(o.required){}
	};
	/// \brief Constructor defining the whole automaton from an initializer list
	/// \param[in] strict true, if strict checking is enabled, false, if the automaton accepts root tags that are not declared, used for parsing a structure embedded into a request
	RequestAutomaton( const papuga_ClassDef* classdefs, const papuga_StructInterfaceDescription* structdefs, bool strict,
				const std::initializer_list<RequestAutomaton_ResultDef>& resultdefs,
				const std::initializer_list<InheritedDef>& inherited,
				const std::initializer_list<RequestAutomaton_Node>& nodes);
#endif
	/// \brief Destructor
	~RequestAutomaton();

	/// \brief Define the declaration of a context this schema is dependent on
	/// \param[in] type the type name of the context inherited
	/// \param[in] expression select expression addressing the name of the context inherited
	/// \param[in] required true if the inherited context is mandatory
	/// \remark Only available if this automaton has been constructed as empty
	/// \note We suggest to define the automaton with one constructor call with the whole automaton defined as structure if C++>=11 is available
	void addInheritContext( const char* typenam, const char* expression, bool required);

	/// \brief Add a result template definition
	/// \param[in] resultdef the result definition
	void addResult( const RequestAutomaton_ResultDef& resultdef);

	/// \brief Add a method call
	/// \param[in] expression select expression addressing the scope of this method call definition
	/// \param[in] resultvar variable where the result of the call is stored to, empty or NULL if the result is void or dropped
	/// \param[in] selfvar variable where the result of the call is stored to, empty if the result is void or dropped
	/// \param[in] methodid identifier of the method to call
	/// \param[in] args list of references addressing the arguments of the method call
	/// \remark Only available if this automaton has been constructed as empty
	/// \note We suggest to define the automaton with one constructor call with the whole automaton defined as structure if C++>=11 is available
	void addFunction( const char* expression, const char* resultvar, const char* selfvar, const papuga_RequestMethodId& methodid, const RequestAutomaton_FunctionDef::Arg* args);

	/// \brief Add a variable assignment of a content element
	/// \note Implemented as function without method call specified
	/// \param[in] scope_expression selecting expression addressing the scope of this variable assignment definition, used to prioritize variable definitions over function definitions with the same target variable of the same scope or covering scope
	/// \param[in] select_expression select expression addressing the scope of this variable assignment definition
	/// \param[in] varname	name of the variable referencing the destination of the assignment
	/// \param[in] itemid	identifier given to the item to make it addressable in the context of its scope
	/// \param[in] resolvechr describes the occurrence of the element
	/// \param[in] max_tag_diff maximum reach of search in number of tag hierarchy levels or 0 if not limited (always >= 0 also for inherited values)
	void addAssignment( const char* scope_expression, const char* select_expression, const char* varname, int itemid, char resolvechr, int max_tag_diff=1);

	/// \brief Add a structure definition
	/// \remark Only available if this automaton has been constructed as empty
	/// \param[in] expression select expression addressing the scope of this structure definition
	/// \param[in] itemid item identifier unique in its scope (referencing a value or a structure)
	/// \param[in] elems list of references to the elements of this structure
	/// \note We suggest to define the automaton with one constructor call with the whole automaton defined as structure if C++>=11 is available
	void addStruct( const char* expression, int itemid, const RequestAutomaton_StructDef::Element* elems);

	/// \brief Add an atomic value definition
	/// \remark Only available if this automaton has been constructed as empty
	/// \param[in] scope_expression selecting expression addressing the scope of this value definition
	/// \param[in] select_expression selecting expression addressing the value itself
	/// \param[in] itemid identifier given to the item to make it addressable in the context of its scope
	/// \param[in] valuetype type of the value
	/// \param[in] examples semicolon ';' separated list of examples or NULL if no examples defined
	/// \note We suggest to define the automaton with one constructor call with the whole automaton defined as structure if C++>=11 is available
	void addValue( const char* scope_expression, const char* select_expression, int itemid, papuga_Type valuetype, const char* examples);

	/// \brief Define the resolve type for the schema description if not defined by other structures
	/// \param[in] expression selecting expression of the element
	/// \param[in] resolvechr describes the occurrence of the element
	void setResolve( const char* expression, char resolvechr);

	/// \brief Open a method call group definition
	/// \remark Only available if this automaton has been constructed as empty
	/// \note We suggest to define the automaton with one constructor call with the whole automaton defined as structure if C++>=11 is available
	void openGroup();

	/// \brief Close a method call group definition
	/// \remark Only available if this automaton has been constructed as empty
	/// \note We suggest to define the automaton with one constructor call with the whole automaton defined as structure if C++>=11 is available
	void closeGroup();

	/// \brief Open a new sub expression to for the root context
	void openRoot( const char* expr);
	/// \brief Close the current open root expression
	void closeRoot();

	/// \brief Finish the automaton definition
	/// \remark Only available if this automaton has been constructed as empty
	/// \note We suggest to define the automaton with one constructor call with the whole automaton defined as structure if C++>=11 is available
	void done();

	/// \brief Get the pointer to the defined automaton for handling requests
	/// \return the automaton definition
	const papuga_RequestAutomaton* impl() const		{return m_atm;}

	/// \brief Get the pointer to the XSD Schema declaration source generation from the schema description
	/// \return the schema description XSD source
	const papuga_SchemaDescription* description() const	{return m_descr;}

private:
	papuga_RequestAutomaton* m_atm;				///< automaton definition
	papuga_SchemaDescription* m_descr;			///< schema description
	std::string m_rootexpr;					///< current root expression
	std::vector<int> m_rootstk;				///< stack for open close root expressions
};


}//namespace
#endif

