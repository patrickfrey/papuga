/*
 * Copyright (c) 2017 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/// \brief Automaton to describe and build papuga XML and JSON requests
/// \file request.h
#ifndef _PAPUGA_REQUEST_H_INCLUDED
#define _PAPUGA_REQUEST_H_INCLUDED
#include "papuga/typedefs.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct papuga_RequestAutomaton papuga_RequestAutomaton;
typedef struct papuga_Request papuga_Request;

/*
 * \brief Create an automaton to configure
 * \return The automaton structure
 */
papuga_RequestAutomaton* papuga_create_RequestAutomaton();

/*
 * \brief Destroy an automaton
 * \param[in] self the automaton structure to free
 */
void papuga_destroy_RequestAutomaton( papuga_RequestAutomaton* self);

/*
 * \brief Get the last error when building the automaton
 * \param[in] self automaton to get the last error from
 */
papuga_ErrorCode papuga_RequestAutomaton_last_error( const papuga_RequestAutomaton* self);

/*
 * \brief Add a method call
 * \param[in,out] self automaton manipulated
 * \param[in] expression xpath expression (abbreviated syntax of xpath) bound to the method to call
 * \param[in] classname class name of method to call
 * \param[in] methodname name of method to call, NULL if constructor to be called
 * \param[in] selfvarname identifier of the owner object for the method to call
 * \param[in] resultvarname identifier to use for the result
 * \param[in] nofargs number of arguments of the call
 */
bool papuga_RequestAutomaton_add_call(
		papuga_RequestAutomaton* self,
		const char* expression,
		const char* classname,
		const char* methodname,
		const char* selfvarname,
		const char* resultvarname,
		int nofargs);

/*
 * \brief Define the start of a call group. Calls inside a group are executed in sequential order for their context. 
 * \note Groups define the first order criterion for call execution. The three criterions are (groupid,position,elementid), single elements are implicitely assigned to a group with one element. Calls without grouping are executed in order of their definition.
 * \remark Only one level of grouping allowed
 * \return true on success, false if already a group defined
 */
bool papuga_RequestAutomaton_open_group( papuga_RequestAutomaton* self);

/*
 * \brief Define the end of a call group. Calls inside a group are executed in sequential order for their context
 * \remark Only one level of grouping allowed
 * \return true on success, false if no group defined yet
 */
bool papuga_RequestAutomaton_close_group( papuga_RequestAutomaton* self);

/*
 * \brief Define a variable reference as argument to the last method call added
 * \param[in,out] self automaton manipulated
 * \param[in] idx index of the argument to set, starting with 0
 * \param[in] varname identifier of the variable to use as argument
 * \return true on success, false on failure (index out of range or memory allocation error)
 */
bool papuga_RequestAutomaton_set_call_arg_var(
		papuga_RequestAutomaton* self,
		int idx,
		const char* varname);

/*
 * \brief Define an argument to the last method call as item in the document processed
 * \param[in,out] self automaton manipulated
 * \param[in] idx index of the argument to set, starting with 0
 * \param[in] itemid identifier of the item
 * \param[in] inherited true, if the scope of the addressed element is covering the scope of the method call addressing it, false if the method call scope is covering the scope of the addressed element
 * \return true on success, false on failure (index out of range or memory allocation error)
 */
bool papuga_RequestAutomaton_set_call_arg_item(
		papuga_RequestAutomaton* self,
		int idx,
		int itemid,
		bool inherited);

/*
 * \brief Add a structure built from elements or structures in the document processed
 * \param[in,out] self automaton manipulated
 * \param[in] expression xpath expression (abbreviated syntax of xpath) bound to the structure
 * \param[in] itemid identifier for the item referenced by method call arguments or structures
 * \param[in] nofmembers number of elements of the structure
 */
bool papuga_RequestAutomaton_add_structure(
		papuga_RequestAutomaton* self,
		const char* expression,
		int itemid,
		int nofmembers);

/*
 * \brief Define an element of the last structure added
 * \param[in,out] self automaton manipulated
 * \param[in] idx index of the element to set, starting with 0
 * \param[in] name identifier naming the structure element added or NULL if the element does not get a name (for arrays)
 * \param[in] itemid identifier of the structure or value associated with the element added
 * \param[in] inherited true, if the scope of the addressed element is covering the scope of the structure addressing it, false if the structure scope is covering the scope of the addressed element
 * \return true on success, false on failure (index out of range or memory allocation error)
 */
bool papuga_RequestAutomaton_set_structure_element(
		papuga_RequestAutomaton* self,
		int idx,
		const char* name,
		int itemid,
		bool inherited);

/*
 * \brief Define an atomic value in the document processed
 * \param[in,out] self automaton manipulated
 * \param[in] scope_expression xpath expression (abbreviated syntax of xpath) defining the scope of the value
 * \param[in] select_expression xpath expression relative to the select expression (abbreviated syntax of xpath) selecting the value
 * \param[in] itemid identifier for the item referenced by method call arguments or structures
 */
bool papuga_RequestAutomaton_add_value(
		papuga_RequestAutomaton* self,
		const char* scope_expression,
		const char* select_expression,
		int itemid);

/*
 * \brief Declare building of the automaton terminated
 * \param[in,out] self automaton manipulated
 * \return true on success, false on failure
 * \remark It is not allowed to manipulate the automaton anymore after this call
 */
bool papuga_RequestAutomaton_done( papuga_RequestAutomaton* self);


/*
 * \brief Create a request structure to feed with content to get a translated request
 * \param[in] atm the automaton structure (done must be called before)
 * \return the request structure
 */
papuga_Request* papuga_create_Request( const papuga_RequestAutomaton* atm);

/*
 * \brief Destroy a request
 * \param[in] self the request structure to free
 */
void papuga_destroy_Request( papuga_Request* self);


/*
 * \brief Set a variable value of the request structure
 * \param[in,out] self the request structure to set the variable of
 * \param[in] varname name of the variable
 * \param[in] value value of the variable
 * \remark do not pass temporary for value, there is no deep copy made
 * \return true on success, false on failure
 * \note the error code in case of failure can be fetched with papuga_Request_last_error
 */
bool papuga_Request_set_variable_value( papuga_Request* self, const char* varname, const papuga_ValueVariant* value);

/*
 * \brief Feed an open tag event to the request structure
 * \param[in,out] self the request structure to feed
 * \param[in] tagname name of the tag
 * \param[in] tagsize size of tagname in bytes
 * \return true on success, false on failure
 * \note the error code in case of failure can be fetched with papuga_Request_last_error
 */
bool papuga_Request_feed_open_tag( papuga_Request* self, const char* tagname, size_t tagnamesize);

/*
 * \brief Feed a close tag event to the request structure
 * \param[in,out] self the request structure to feed
 * \return true on success, false on failure
 * \note the error code in case of failure can be fetched with papuga_Request_last_error
 */
bool papuga_Request_feed_close_tag( papuga_Request* self);

/*
 * \brief Feed an attribute name event to the request structure
 * \param[in,out] self the request structure to feed
 * \param[in] attrname name of the attribute
 * \param[in] attrsize size of attrname in bytes
 * \return true on success, false on failure
 * \note the error code in case of failure can be fetched with papuga_Request_last_error
 */
bool papuga_Request_feed_attribute_name( papuga_Request* self, const char* attrname, size_t attrnamesize);

/*
 * \brief Feed an attribute value event to the request structure
 * \param[in,out] self the request structure to feed
 * \param[in] valueptr content of the value
 * \param[in] valuesize size of valueptr in bytes
 * \return true on success, false on failure
 * \note the error code in case of failure can be fetched with papuga_Request_last_error
 */
bool papuga_Request_feed_attribute_value( papuga_Request* self, const char* valueptr, size_t valuesize);

/*
 * \brief Feed an content value event to the request structure
 * \param[in,out] self the request structure to feed
 * \param[in] valueptr content of the value
 * \param[in] valuesize size of valueptr in bytes
 * \return true on success, false on failure
 * \note the error code in case of failure can be fetched with papuga_Request_last_error
 */
bool papuga_Request_feed_content_value( papuga_Request* self, const char* valueptr, size_t valuesize);

/*
 * \brief Get the last processing error of the request
 * \param[in] self request to get the last error from
 * \return the error code
 */
papuga_ErrorCode papuga_Request_last_error( const papuga_Request* self);

/*
 * \brief Describes one method call provided by the request
 */
typedef struct papuga_RequestMethodCall
{
	const char* classname;				/*< method class name */
	const char* methodname;				/*< method name */
	papuga_CallArgs args;				/*< arguments of the call */
	char membuf[ 4096];				/*< local memory buffer for allocator */
} papuga_RequestMethodCall;

/*
 * \brief Get the next method call of the request
 * \param[in] self request to get the next method call from
 * \return pointer to the method call description (temporary, only valid until the next one is fetched)
 */
papuga_RequestMethodCall* papuga_Request_next_call( papuga_Request* self);

#ifdef __cplusplus
}
#endif
#endif

