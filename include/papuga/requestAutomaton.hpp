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
#include <string>
#include <stdexcept>
#include <vector>

namespace papuga {

/// \brief Request method call (function) definition
struct RequestAutomaton_FunctionDef
{
	/// \brief Request function argument
	struct Arg
	{
		const char* varname;		///< name of the variable referencing the argument in case of a variable
		int itemid;			///< item identifier unique in its scope, in case of an item reference (a value or a structure)
		bool inherited;			///< tells wheter the item is in a enclosing scope (true) or in an enclosed scope (false), in case of an item reference (a value or a structure)
		const char* defaultvalue;	///< default value in case not defined

		Arg( const char* varname_)
			:varname(varname_),itemid(-1),inherited(false){}
		Arg( int itemid_, bool inherited_=false, const char* defaultvalue_=0)
			:varname(0),itemid(itemid_),inherited(inherited_),defaultvalue(defaultvalue_){}
		Arg( const Arg& o)
			:varname(o.varname),itemid(o.itemid),inherited(o.inherited),defaultvalue(o.defaultvalue){}
	};

	const char* expression;			///< selecting expression addressing the scope of the request
	const char* resultvar;			///< variable where the result of the call is stored to, empty if the result is void or dropped
	const char* selfvar;			///< variable addressing the object of the method call
	papuga_RequestMethodId methodid;	///< identifier of the method to call
	std::vector<Arg> args;			///< list of references addressing the arguments of the method call

	/// \brief Copy constructor
	RequestAutomaton_FunctionDef( const RequestAutomaton_FunctionDef& o)
		:expression(o.expression),resultvar(o.resultvar),selfvar(o.selfvar),args(o.args)
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
	RequestAutomaton_FunctionDef( const char* expression_, const char* resultvar_, const char* selfvar_, const papuga_RequestMethodId& methodid_, const std::vector<Arg>& args_)
		:expression(expression_),resultvar(resultvar_),selfvar(selfvar_),args(args_)
	{
		methodid.classid = methodid_.classid;
		methodid.functionid = methodid_.functionid;
	}
	/// \brief Add this method call definition to an automaton
	/// \param[in] atm automaton to add this definition to
	void addToAutomaton( papuga_RequestAutomaton* atm) const;
};

/// \brief Request structure definition 
struct RequestAutomaton_StructDef
{
	/// \brief Structure element definition
	struct Element
	{
		const char* name;	///< name of the element or NULL in case of an array element
		int itemid;		///< identifier of the item addressing the element value
		bool inherited;		///< true if the element is defined in the enclosing scope, false if in the enclosed scope

		///\brief Constructor (named dictionary element)
		Element( const char* name_, int itemid_, bool inherited_)
			:name(name_),itemid(itemid_),inherited(inherited_){}
		///\brief Constructor (array elements)
		Element( int itemid_)
			:name(0),itemid(itemid_),inherited(false){}
		///\brief Copy constructor
		Element( const Element& o)
			:name(o.name),itemid(o.itemid),inherited(o.inherited){}
	};

	const char* expression;		///< selecting expression addressing the scope of this structure definition
	int itemid;			///< item identifier unique in its scope (referencing a value or a structure)
	std::vector<Element> elems;	///< list of references to the elements of this structure

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
	/// \param[in] atm automaton to add this definition to
	void addToAutomaton( papuga_RequestAutomaton* atm) const;
};

/// \brief Request atomic value definition 
struct RequestAutomaton_ValueDef
{
	const char* scope_expression;	///< selecting expression addressing the scope of this value definition
	const char* select_expression;	///< selecting expression addressing the value itself
	int itemid;			///< identifier given to the item to make it uniquely addressable in the context of its scope

	/// \brief Copy constructor
	RequestAutomaton_ValueDef( const RequestAutomaton_ValueDef& o)
		:scope_expression(o.scope_expression),select_expression(o.select_expression),itemid(o.itemid){}
	/// \brief Constructor
	/// \param[in] scope_expression_ selecting expression addressing the scope of this value definition
	/// \param[in] select_expression_ selecting expression addressing the value itself
	/// \param[in] itemid_ identifier given to the item to make it uniquely addressable in the context of its scope
	RequestAutomaton_ValueDef( const char* scope_expression_, const char* select_expression_, int itemid_)
		:scope_expression(scope_expression_),select_expression(select_expression_),itemid(itemid_){}

	/// \brief Add this value definition to an automaton
	/// \param[in] atm automaton to add this definition to
	void addToAutomaton( papuga_RequestAutomaton* atm) const;
};

/// \brief Request grouping of functions definitions
struct RequestAutomaton_GroupDef
{
	std::vector<RequestAutomaton_FunctionDef> nodes;	///< list of function definitions belonging to this group

	/// \brief Copy constructor
	RequestAutomaton_GroupDef( const RequestAutomaton_GroupDef& o)
		:nodes(o.nodes){}
	/// \brief Constructor
	/// \param[in] nodes_ list of nodes of this group
	RequestAutomaton_GroupDef( const std::vector<RequestAutomaton_FunctionDef>& nodes_)
		:nodes(nodes_){}

	/// \brief Add this group definition to an automaton
	/// \param[in] atm automaton to add this definition to
	void addToAutomaton( papuga_RequestAutomaton* atm) const;
};

#if __cplusplus >= 201103L
/// \class RequestAutomaton_Node
/// \brief Union of all RequestAutomaton_XXDef types for using in C++ initializer lists
struct RequestAutomaton_Node
{
	struct Group_ {};
	enum Type {
		Empty,
		Group,
		Function,
		Struct,
		Value
	};
	Type type;
	union
	{
		RequestAutomaton_GroupDef* groupdef;
		RequestAutomaton_FunctionDef* functiondef;
		RequestAutomaton_StructDef* structdef;
		RequestAutomaton_ValueDef* valuedef;
	} value;

	///\brief Default contructor
	RequestAutomaton_Node();
	///\brief Contructor as RequestAutomaton_GroupDef
	RequestAutomaton_Node( const Group_&, const std::initializer_list<RequestAutomaton_FunctionDef>& nodes_);
	///\brief Contructor as RequestAutomaton_FunctionDef
	RequestAutomaton_Node( const char* expression_, const char* resultvar_, const char* selfvar_, const papuga_RequestMethodId& methodid_, const std::initializer_list<RequestAutomaton_FunctionDef::Arg>& args_);
	///\brief Contructor as RequestAutomaton_StructDef
	RequestAutomaton_Node( const char* expression_, int itemid_, const std::initializer_list<RequestAutomaton_StructDef::Element>& elems_);
	///\brief Contructor as RequestAutomaton_ValueDef
	RequestAutomaton_Node( const char* scope_expression_, const char* select_expression_, int itemid_);

	///\brief Copy contructor
	RequestAutomaton_Node( const RequestAutomaton_Node& o);
	/// \brief Add this node definition to an automaton
	/// \param[in] atm automaton to add this definition to
	void addToAutomaton( papuga_RequestAutomaton* atm) const;
};
#endif

/// \brief Structure defining the mapping of requests of a certain type to a list of method calls
class RequestAutomaton
{
private:
	RequestAutomaton( const RequestAutomaton&)
		:m_atm(0){}
	void operator=( const RequestAutomaton&) {}
	//... non copyable

public:
	/// \brief Constructor defining an empty automaton to be filled with further method calls
	RequestAutomaton( const papuga_ClassDef* classdefs, const papuga_StructInterfaceDescription* structdefs, const char* answername);

#if __cplusplus >= 201103L
	/// \brief Constructor defining the whole automaton from an initializer list
	RequestAutomaton( const papuga_ClassDef* classdefs, const papuga_StructInterfaceDescription* structdefs, const char* answername, const std::initializer_list<RequestAutomaton_Node>& nodes);
#endif
	/// \brief Destructor
	~RequestAutomaton();

	/// \brief Add a method call
	/// \param[in] expression select expression addressing the scope of this method call definition
	/// \param[in] resultvar variable where the result of the call is stored to, empty if the result is void or dropped
	/// \param[in] selfvar variable where the result of the call is stored to, empty if the result is void or dropped
	/// \param[in] methodid identifier of the method to call
	/// \param[in] args list of references addressing the arguments of the method call
	/// \remark Only available if this automaton has been constructed as empty
	/// \note We suggest to define the automaton with one constructor call with the whole automaton defined as structure if C++>=11 is available
	void addFunction( const char* expression, const char* resultvar, const char* selfvar, const papuga_RequestMethodId& methodid, const RequestAutomaton_FunctionDef::Arg* args);

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
	/// \param[in] itemid identifier given to the item to make it uniquely addressable in the context of its scope
	/// \note We suggest to define the automaton with one constructor call with the whole automaton defined as structure if C++>=11 is available
	void addValue( const char* scope_expression, const char* select_expression, int itemid);

	/// \brief Open a method call group definition
	/// \remark Only available if this automaton has been constructed as empty
	/// \note We suggest to define the automaton with one constructor call with the whole automaton defined as structure if C++>=11 is available
	void openGroup();

	/// \brief Close a method call group definition
	/// \remark Only available if this automaton has been constructed as empty
	/// \note We suggest to define the automaton with one constructor call with the whole automaton defined as structure if C++>=11 is available
	void closeGroup();

	/// \brief Finish the automaton definition
	/// \remark Only available if this automaton has been constructed as empty
	/// \note We suggest to define the automaton with one constructor call with the whole automaton defined as structure if C++>=11 is available
	void done();

	/// \brief Get the pointer to the defined automaton for handling requests
	/// \return the automaton definition
	const papuga_RequestAutomaton* impl() const	{return m_atm;}

private:
	papuga_RequestAutomaton* m_atm;			///< automaton definition
};


}//namespace
#endif

