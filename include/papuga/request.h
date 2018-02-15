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
#ifndef _PAPUGA_REQUEST_H_INCLUDED
#define _PAPUGA_REQUEST_H_INCLUDED
#include "papuga/typedefs.h"
#include "papuga/interfaceDescription.h"
#include "papuga/classdef.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct papuga_RequestAutomaton papuga_RequestAutomaton;
typedef struct papuga_Request papuga_Request;

/*
 * @brief Defines an identifier for a method
 */
typedef struct papuga_RequestMethodId
{
	int classid;					/*< index of object class starting with 1 */
	int functionid ;				/*< index of method function in class starting with 1 */
} papuga_RequestMethodId;

/*
 * @brief Defines a list of variables of a request
 * @remark A request does not need too many variables, maybe 2 or 3, so a list is fine for search
 */
typedef struct papuga_RequestVariable
{
	struct papuga_RequestVariable* next;		/*< next variable */
	const char* name;				/*< name of variable associated with this value */
	papuga_ValueVariant value;			/*< variable value associated with this name */
	bool inherited;					/*< variable value has been inherited and is not printed as part of the result */
} papuga_RequestVariable;

/*
 * @brief Create an automaton to configure
* \param[in] classdefs class definitions referred to in host object references
 * @return The automaton structure
 */
papuga_RequestAutomaton* papuga_create_RequestAutomaton( const papuga_ClassDef* classdefs, const papuga_StructInterfaceDescription* structdefs, const char* resultname);

/*
 * @brief Destroy an automaton
 * @param[in] self the automaton structure to free
 */
void papuga_destroy_RequestAutomaton( papuga_RequestAutomaton* self);

/*
 * @brief Get the last error when building the automaton
 * @param[in] self automaton to get the last error from
 */
papuga_ErrorCode papuga_RequestAutomaton_last_error( const papuga_RequestAutomaton* self);

/*
 * @brief Add a method call
 * @param[in,out] self automaton manipulated
 * @param[in] expression xpath expression (abbreviated syntax of xpath) bound to the method to call
 * @param[in] method identifier of the method to call
 * @param[in] selfvarname identifier of the owner object for the method to call
 * @param[in] resultvarname identifier to use for the result
 * @param[in] nofargs number of arguments of the call
 */
bool papuga_RequestAutomaton_add_call(
		papuga_RequestAutomaton* self,
		const char* expression,
		const papuga_RequestMethodId* method,
		const char* selfvarname,
		const char* resultvarname,
		int nofargs);

/*
 * @brief Define a variable reference as argument to the last method call added
 * @param[in,out] self automaton manipulated
 * @param[in] idx index of the argument to set, starting with 0
 * @param[in] varname identifier of the variable to use as argument
 * @return true on success, false on failure (index out of range or memory allocation error)
 */
bool papuga_RequestAutomaton_set_call_arg_var(
		papuga_RequestAutomaton* self,
		int idx,
		const char* varname);

/*
 * @brief Enumeration type describing the way an argument of structure member is resolved
 */
typedef enum {
	papuga_ResolveTypeRequired,		/*< the item must be found in the included scope and it is unique */
	papuga_ResolveTypeOptional,		/*< the item is found in the included scope and it is unique, if it exists */
	papuga_ResolveTypeInherited,		/*< the item must be found in an including scope, uniqueness is not checked, the innermost candidate wins */
	papuga_ResolveTypeArray,		/*< the item is found in the included scope and it can exist more that once */
} papuga_ResolveType;

/*
 * @brief Get the resolve type name as string
 * @param[in] resolvetype the resolve type
 * @return the resolve type name as string
 */
const char* papuga_ResolveTypeName( papuga_ResolveType resolvetype);

/*
 * @brief Define an argument to the last method call as item in the document processed
 * @param[in,out] self automaton manipulated
 * @param[in] idx index of the argument to set, starting with 0
 * @param[in] itemid identifier of the item
 * @param[in] resolvetype defines the way an addressed item is resolved and constructed
 * @param[in] max_tag_diff maximum reach of search in number of tag hierarchy levels or -1 if not limited (always >= 0 also for inherited values)
 * @return true on success, false on failure (index out of range or memory allocation error)
 */
bool papuga_RequestAutomaton_set_call_arg_item(
		papuga_RequestAutomaton* self,
		int idx,
		int itemid,
		papuga_ResolveType resolvetype,
		int max_tag_diff);

/*
 * @brief Define the start of a call group. Calls inside a group are executed in sequential order for their context. 
 * @note Groups define the first order criterion for call execution. The three criterions are (groupid,position,elementid), single elements are implicitely assigned to a group with one element. Calls without grouping are executed in order of their definition.
 * @remark Only one level of grouping allowed
 * @return true on success, false if already a group defined
 */
bool papuga_RequestAutomaton_open_group( papuga_RequestAutomaton* self);

/*
 * @brief Define the end of a call group. Calls inside a group are executed in sequential order for their context
 * @remark Only one level of grouping allowed
 * @return true on success, false if no group defined yet
 */
bool papuga_RequestAutomaton_close_group( papuga_RequestAutomaton* self);

/*
 * @brief Add a structure built from elements or structures in the document processed
 * @param[in,out] self automaton manipulated
 * @param[in] expression xpath expression (abbreviated syntax of xpath) bound to the structure
 * @param[in] itemid identifier for the item referenced by method call arguments or structures
 * @param[in] nofmembers number of elements of the structure
 */
bool papuga_RequestAutomaton_add_structure(
		papuga_RequestAutomaton* self,
		const char* expression,
		int itemid,
		int nofmembers);

/*
 * @brief Define an element of the last structure added
 * @param[in,out] self automaton manipulated
 * @param[in] idx index of the element to set, starting with 0
 * @param[in] name identifier naming the structure element added or NULL if the element does not get a name (for arrays)
 * @param[in] itemid identifier of the structure or value associated with the element added
 * @param[in] resolvetype defines the way an addressed item is resolved and constructed
 * @param[in] max_tag_diff maximum reach of search in number of tag hierarchy levels or -1 if not limited (always >= 0 also for inherited values)
 * @return true on success, false on failure (index out of range or memory allocation error)
 */
bool papuga_RequestAutomaton_set_structure_element(
		papuga_RequestAutomaton* self,
		int idx,
		const char* name,
		int itemid,
		papuga_ResolveType resolvetype,
		int max_tag_diff);

/*
 * @brief Define an atomic value in the document processed
 * @param[in,out] self automaton manipulated
 * @param[in] scope_expression xpath expression (abbreviated syntax of xpath) defining the scope of the value
 * @param[in] select_expression xpath expression relative to the select expression (abbreviated syntax of xpath) selecting the value
 * @param[in] itemid identifier for the item referenced by method call arguments or structures
 */
bool papuga_RequestAutomaton_add_value(
		papuga_RequestAutomaton* self,
		const char* scope_expression,
		const char* select_expression,
		int itemid);

/*
 * @brief Declare building of the automaton terminated
 * @param[in,out] self automaton manipulated
 * @return true on success, false on failure
 * @remark It is not allowed to manipulate the automaton anymore after this call
 */
bool papuga_RequestAutomaton_done( papuga_RequestAutomaton* self);


/*
 * @brief Create a request structure to feed with content to get a translated request
 * @param[in] atm the automaton structure (done must be called before)
 * @return the request structure
 */
papuga_Request* papuga_create_Request( const papuga_RequestAutomaton* atm);

/*
 * @brief Destroy a request
 * @param[in] self the request structure to free
 */
void papuga_destroy_Request( papuga_Request* self);

/*
 * @brief Feed an open tag event to the request structure
 * @param[in,out] self the request structure to feed
 * @param[in] tagname name of the tag
 * @return true on success, false on failure
 * @note the error code in case of failure can be fetched with papuga_Request_last_error
 */
bool papuga_Request_feed_open_tag( papuga_Request* self, const papuga_ValueVariant* tagname);

/*
 * @brief Feed a close tag event to the request structure
 * @param[in,out] self the request structure to feed
 * @return true on success, false on failure
 * @note the error code in case of failure can be fetched with papuga_Request_last_error
 */
bool papuga_Request_feed_close_tag( papuga_Request* self);

/*
 * @brief Feed an attribute name event to the request structure
 * @param[in,out] self the request structure to feed
 * @param[in] attrname name of the attribute
 * @return true on success, false on failure
 * @note the error code in case of failure can be fetched with papuga_Request_last_error
 */
bool papuga_Request_feed_attribute_name( papuga_Request* self, const papuga_ValueVariant* attrname);

/*
 * @brief Feed an attribute value event to the request structure
 * @param[in,out] self the request structure to feed
 * @param[in] value content of the value
 * @return true on success, false on failure
 * @note the error code in case of failure can be fetched with papuga_Request_last_error
 */
bool papuga_Request_feed_attribute_value( papuga_Request* self, const papuga_ValueVariant* value);

/*
 * @brief Feed an content value event to the request structure
 * @param[in,out] self the request structure to feed
 * @param[in] value content of the value
 * @return true on success, false on failure
 * @note the error code in case of failure can be fetched with papuga_Request_last_error
 */
bool papuga_Request_feed_content_value( papuga_Request* self, const papuga_ValueVariant* value);

/*
 * @brief Terminate feeding to the request
 * @param[in,out] self the request structure to close
 * @return true on success, false on failure
 * @note the error code in case of failure can be fetched with papuga_Request_last_error
 */
bool papuga_Request_done( papuga_Request* self);

/*
 * @brief Get the last processing error of the request
 * @param[in] self request to get the last error from
 * @return the error code
 */
papuga_ErrorCode papuga_Request_last_error( const papuga_Request* self);

/*
 * @brief Get the class definitions of a request for refering to the method calls
 * @param[in] self request to get the last error from
 * @return the {NULL,..} terminated array of class definitions
 */
const papuga_ClassDef* papuga_Request_classdefs( const papuga_Request* self);

/*
 * @brief Describes one method call provided by the request
 */
typedef struct papuga_RequestMethodCall
{
	const char* selfvarname;			/*< variable referencing the object for the method call */
	const char* resultvarname;			/*< variable where to write the result to */
	papuga_RequestMethodId methodid;		/*< method identifier */
	int eventcnt;					/*< event (scope) counter for reproducing error area */
	int argcnt;					/*< argument index of erroneous parameter or -1*/
	papuga_CallArgs args;				/*< arguments of the call */
	char membuf[ 4096];				/*< local memory buffer for allocator */
} papuga_RequestMethodCall;

typedef struct papuga_RequestIterator papuga_RequestIterator;

/*
 * @brief Create an iterator on the method calls of a closed request
 * @param[in] allocator for memory allocation for the iterator
 * @param[in] request request object to get the iterator on the request method calls
 * @return the iterator in case of success, or NULL in case of a memory allocation error
 */
papuga_RequestIterator* papuga_create_RequestIterator( papuga_Allocator* allocator, const papuga_Request* request);

/*
 * @brief Destructor of an iterator on the method calls of a closed request
 * @param[in] self request iterator to destroy
 */
void papuga_destroy_RequestIterator( papuga_RequestIterator* self);

/*
 * @brief Get the next method call of a request
 * @param[in] self request iterator to get the next method call from
 * @param[in] varlist pointer to current list of variables
 * @return pointer to the method call description (temporary, only valid until the next one is fetched)
 */
const papuga_RequestMethodCall* papuga_RequestIterator_next_call( papuga_RequestIterator* self, const papuga_RequestVariable* varlist);

/*
 * @brief Get the last error of the iterator with a pointer to the method call that failed, if available
 * @param[in] self request iterator to get the last error from
 * @param[out] call pointer to call that caused the error
 * @return the error code of papuga_Ok if there was no error
 * @return pointer to the method call description (temporary, only valid until the next one is fetched)
 */
papuga_ErrorCode papuga_RequestIterator_get_last_error( papuga_RequestIterator* self, const papuga_RequestMethodCall** call);


/*
 * @brief Map a request to a readable string of method calls without variables resolved for inspection
 * @param[in] self request iterator to get the next method call from
 * @param[in] allocator allocator to use for the result
 * @param[in] enc charset of the result string
 * @param[out] length length of the string in units (1 for UTF-8, 2 for UTF16, etc.)
 * @param[out] errcode error code in case of an error
 * @return pointer to the string built
 */
const char* papuga_Request_tostring( const papuga_Request* self, papuga_Allocator* allocator, papuga_StringEncoding enc, size_t* length, papuga_ErrorCode* errcode);

/*
 * @brief Get the name of the result produced by this request (e.g. used as toplevel tag for XML of result)
 */
const char* papuga_Request_resultname( const papuga_Request* self);

/*
 * @brief Get the structure descriptions of the request for mapping the output
 */
const papuga_StructInterfaceDescription* papuga_Request_struct_descriptions( const papuga_Request* self);

#ifdef __cplusplus
}
#endif
#endif

