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
#include "papuga/requestLogger.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct papuga_RequestAutomaton papuga_RequestAutomaton;
typedef struct papuga_Request papuga_Request;
typedef struct papuga_RequestContext papuga_RequestContext;
typedef struct papuga_RequestResultDescription papuga_RequestResultDescription;

/*
 * @brief Defines an identifier for a method
 */
typedef struct papuga_RequestMethodId
{
	int classid;					/*< index of object class starting with 1 */
	int functionid ;				/*< index of method function in class starting with 1 */
} papuga_RequestMethodId;

/*
 * @brief Structure holding the result of a request
* @member name name of the result, root element (constant, string not copied), NULL if not content defined (delegate request without content)
* @member schema name of the schema that handles the request if the result forms a request to other servers
* @member requestmethod request method if the result forms a request to other servers
* @member addressvar name of the variable with the urls if the result forms a request to other servers
* @member path additional path added to urls referenced in address variables if the result forms a request to other servers
* @member contentvar list of variable names addressing content to attach to the result (NULL terminated)
* @member serialization serialization of the result
*/
typedef struct papuga_RequestResult
{
	const char* name;
	const char* schema;
	const char* requestmethod;
	const char* addressvar;
	const char* path;
	const char* const* contentvar;
	papuga_Serialization serialization;
} papuga_RequestResult;

/*
 * @brief Create an automaton to configure
 * @param[in] classdefs class definitions referred to in host object references
 * @param[in] structdefs structure definitions
 * @param[in] strict true, if strict checking is enabled, false, if the automaton accepts root tags that are not declared, used for parsing a structure embedded into a request
 * @param[in] exclusiveAccess true, if a request needs exclusive access to its underlying data and resources, e.g. the execution of other requests have to be rejected (http status 503) while this request is running
 * @return The automaton structure
 */
papuga_RequestAutomaton* papuga_create_RequestAutomaton(
		const papuga_ClassDef* classdefs,
		const papuga_StructInterfaceDescription* structdefs,
		bool strict,
		bool exclusiveAccess);

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
 * @brief Declare a dependency to a context where all variables are inherited from
 * @param[in,out] self automaton changed
 * @param[in] type type name of the context to inherit from
 * @param[in] name_expression xpath expression (abbreviated syntax of xpath) bound to the name of the context to inherit from
 * @param[in] required true if the inheritance declaration in mandatory
 */
bool papuga_RequestAutomaton_inherit_from(
		papuga_RequestAutomaton* self,
		const char* type,
		const char* name_expression,
		bool required);

/*
 * @brief Add a method call
 * @param[in,out] self automaton changed
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
 * @param[in,out] self automaton changed
 * @param[in] idx index of the argument to set, starting with 0
 * @param[in] varname name of the variable to use as argument
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
	papuga_ResolveTypeInherited,		/*< the item must be found in an including scope, uniqueness is not checked, the innermost candidates win */
	papuga_ResolveTypeArray,		/*< the item is found in the included scope and it does not have to exist or it can exist more that once */
	papuga_ResolveTypeArrayNonEmpty		/*< the item is found in the included scope probably more that once */
} papuga_ResolveType;

/*
 * @brief Get the resolve type name as string
 * @param[in] resolvetype the resolve type
 * @return the resolve type name as string
 */
const char* papuga_ResolveTypeName( papuga_ResolveType resolvetype);

/*
 * @brief Define an argument to the last method call as item in the document processed
 * @param[in,out] self automaton changed
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
 * @brief Prioritize the last method call added in the scope passed as selection expression
 * @param[in,out] self automaton changed
 * @param[in] expression xpath expression (abbreviated syntax of xpath) of the scope the last method call is prioritized over others with the same destination variable
 * @return true on success, false on failure (no last call defined or memory allocation error)
 */
bool papuga_RequestAutomaton_prioritize_last_call(
		papuga_RequestAutomaton* self,
		const char* scope_expression);

/*
 * @brief Define the start of a call group. Calls inside a group are executed in sequential order for appearing in input.
 * @note Calls without grouping are executed in order of their definition.
 * @remark Only one level of grouping allowed (no nesting of group definitions allowed)
 * @param[in,out] self automaton changed
 * @param[in] groupid group identifier
 * @return true on success, false if already a group defined
 */
bool papuga_RequestAutomaton_open_group( papuga_RequestAutomaton* self, int groupid);

/*
 * @brief Define the end of a call group. Calls inside a group are executed in sequential order for their context
 * @remark Only one level of grouping allowed
 * @param[in,out] self automaton changed
 * @return true on success, false if not in the context of an opened group
 */
bool papuga_RequestAutomaton_close_group( papuga_RequestAutomaton* self);

/*
 * @brief Add a structure built from elements or structures in the document processed
 * @param[in,out] self automaton changed
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
 * @brief Define an item in the document processed to be an element of the last structure added
 * @param[in,out] self automaton changed
 * @param[in] idx index of the element to set, starting with 0
 * @param[in] name identifier naming the structure element added or NULL if the element does not get a name (array element)
 * @param[in] itemid identifier of the structure or value associated with the element added
 * @param[in] resolvetype defines the way an addressed item is resolved and constructed
 * @param[in] max_tag_diff maximum reach of search in number of tag hierarchy levels or -1 if not limited (always >= 0 also for inherited values)
 * @return true on success, false on failure (index out of range or memory allocation error)
 */
bool papuga_RequestAutomaton_set_structure_element_item(
		papuga_RequestAutomaton* self,
		int idx,
		const char* name,
		int itemid,
		papuga_ResolveType resolvetype,
		int max_tag_diff);

/*
 * @brief Define a value referenced by a variable to be an element of the last structure added
 * @param[in,out] self automaton changed
 * @param[in] idx index of the element to set, starting with 0
 * @param[in] name identifier naming the structure element added or NULL if the element does not get a name (array element)
 * @param[in] varname name of the variable referencing the value of the structure element
 * @return true on success, false on failure (index out of range or memory allocation error)
 */
bool papuga_RequestAutomaton_set_structure_element_var(
		papuga_RequestAutomaton* self,
		int idx,
		const char* name,
		const char* varname);

/*
 * @brief Define an atomic value in the document processed
 * @param[in,out] self automaton changed
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
 * @brief Add a description of a result
 * @param[in,out] self automaton changed
 * @param[in] descr description of how to build the result (passed with ownership)
 */
bool papuga_RequestAutomation_add_result(
		papuga_RequestAutomaton* self,
		papuga_RequestResultDescription* descr);

/*
 * @brief Structure holding an instruction for an assigment of a variable by a value depending on the environment
 * @note Environment assignment instructions are executed before any other operation of the request automaton
 * @member name name of the variable created with the result of the environment function as value
 * @member envid identifier of the function executed
 * @member argument constant argument for the function
*/
typedef struct papuga_RequestEnvAssignment
{
	const char* variable;
	int envid;
	const char* argument;
} papuga_RequestEnvAssignment;

/*
 * @brief Adding an assigment of a variable by a value depending on the environment to the request
 * @param[in] name name of the variable created with the result of the environment function as value
 * @param[in] envid identifier of the function executed
 * @param[in] argument constant argument for the function
*/
bool papuga_RequestAutomation_add_env_assignment(
		papuga_RequestAutomaton* self,
		const char* variable,
		int envid,
		const char* argument);

/*
 * @brief Return the environment assigments defined for the automaton
 * @param[in] self automaton
 * @return a {NULL,0,NULL) terminated list
 */
const papuga_RequestEnvAssignment* papuga_RequestAutomation_get_env_assignments( const papuga_RequestAutomaton* self);

/*
 * @brief Get the exclusiveAccess flag of the automaton
 * @param[in] self automaton
 * @return the flag
 */
bool papuga_RequestAutomaton_has_exclusive_access( const papuga_RequestAutomaton* self);

/*
 * @brief Declare building of the automaton terminated
 * @param[in,out] self automaton changed
 * @return true on success, false on failure
 * @remark It is not allowed to manipulate the automaton anymore after this call
 */
bool papuga_RequestAutomaton_done( papuga_RequestAutomaton* self);


/*
 * @brief Create a request structure to feed with content to get a translated request
 * @param[in] atm the automaton structure (done must be called before)
 * @param[in] logger logging facility to use for the request
 * @return the request structure
 */
papuga_Request* papuga_create_Request( const papuga_RequestAutomaton* atm, papuga_RequestLogger* logger);

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
 * @brief Get the item in the automaton definition that caused the last error
 * @param[in] self request to get the last error from
 * @return the item identifier
 */
int papuga_Request_last_error_itemid( const papuga_Request* self);

/*
 * @brief Check if a variable is declared as part of the result
 * @param[in] self request to evaluate the variable for
 * @param[in] varname name of the variable to test
 * @note The request handler uses this method to decide wheter to assign the result of a call in the request to a context variable or map it as part of the result, referenced by the result template.
 * @return true, if yes, false if no
 */
bool papuga_Request_is_result_variable( const papuga_Request* self, const char* varname);

/*
 * @brief Describes a context inherited by name
 */
typedef struct papuga_RequestInheritedContextDef
{
	const char* type;
	const char* name;
} papuga_RequestInheritedContextDef;

/*
 * @brief Get the list of inherited context definitions by type and name
 * @param[in] self request to get the list from
 * @param[out] errcode error code in case of error
 * @return the {NULL,NULL} terminated array of inherited context definitions or NULL if definition incomplete or another error occurred
 */
const papuga_RequestInheritedContextDef* papuga_Request_get_inherited_contextdefs( const papuga_Request* self, papuga_ErrorCode* errcode);

/*
 * @brief Get the class definitions of a request for refering to the method calls
 * @param[in] self request to get the last error from
 * @return the {NULL,..} terminated array of class definitions
 */
const papuga_ClassDef* papuga_Request_classdefs( const papuga_Request* self);

/*
 * @brief Describes details about the error occurred in resolving a method call
 */
typedef struct papuga_RequestError
{
	papuga_ErrorCode errcode;			/*< error code */
	int scopestart;					/*< scope start (equals event counter) for reproducing error area */
	int argcnt;					/*< argument index of erroneous parameter or -1*/
	const char* classname;				/*< class name */
	const char* methodname;				/*< method name */
	const char* variable;				/*< variable name causing the error or NULL if not defined */
	int itemid;					/*< item causing the error or 0 if not defined */
	char structpath[ 128];				/*< path of the structure accessed when the error occurred */
	char errormsg[ 2048];				/*< error message reported by the bindings method call */
} papuga_RequestError;

/*
* @brief Request error constructor
* @param[out] self pointer to structure 
*/
void papuga_init_RequestError( papuga_RequestError* self);

/*
 * @brief Describes one method call provided by the request
 */
typedef struct papuga_RequestMethodCall
{
	const char* selfvarname;			/*< variable referencing the object for the method call */
	const char* resultvarname;			/*< variable where to write the result to */
	papuga_RequestMethodId methodid;		/*< method identifier if defined */
	papuga_CallArgs args;				/*< arguments of the call */
	char membuf[ 4096];				/*< local memory buffer for allocator */
} papuga_RequestMethodCall;

/*
 * @brief Handle for an iterator on the request
 */
typedef struct papuga_RequestIterator papuga_RequestIterator;

/*
 * @brief Create an iterator on the method calls of a closed request
 * @param[in] allocator for memory allocation for the iterator
 * @param[in] request request object to get the iterator on the request method calls
 * @param[out] errcode error code in case NULL is returned
 * @return the iterator in case of success, or NULL in case of a memory allocation error
 */
papuga_RequestIterator* papuga_create_RequestIterator( papuga_Allocator* allocator, const papuga_Request* request, papuga_ErrorCode* errcode);

/*
 * @brief Destructor of an iterator on the method calls of a closed request
 * @param[in] self request iterator to destroy
 */
void papuga_destroy_RequestIterator( papuga_RequestIterator* self);

/*
 * @brief Get the next method call of a request
 * @param[in] self request iterator to get the next method call from
 * @param[in] context request context
 * @return pointer to the method call description (temporary, only valid until the next one is fetched)
 */
const papuga_RequestMethodCall* papuga_RequestIterator_next_call( papuga_RequestIterator* self, const papuga_RequestContext* context);

/*
 * @brief Declare the value of the last call fetched
 * @note This method counteracts the idea of an iterator. It indicates a flaw in the organization of this API. Every result of a call has to be notified so that the request results can be built in the request module. In a future redesign this has to be fixed.
 */
bool papuga_RequestIterator_push_call_result( papuga_RequestIterator* self, const papuga_ValueVariant* result);

/*
 * @brief Get the list of all non empty results of a request, without content of variables (contentvar) attached to the result serialization yet
 * @note Empty result means that it was not created at all, not an empty content returned with the result)
 * @param[in,out] self this context pointer
 * @param[in] context request context
 * @param[in] allocator allocator to use for the result array and its contents
 * @param[out] nofResults the number of results constructed, the size of the array of result pointers returned
 * @return a pointer to the array of results
 */
papuga_RequestResult* papuga_RequestIterator_get_result_array( papuga_RequestIterator* self, const papuga_RequestContext* context, papuga_Allocator* allocator, int* nofResults);

/*
 * @brief Get the last error of the iterator with a pointer to the method call that failed, if available
 * @param[in] self request iterator to get the last error from
 * @return the error structure or NULL if there was no error
 */
const papuga_RequestError* papuga_RequestIterator_get_last_error( papuga_RequestIterator* self);

/*
 * @brief Get the structure descriptions of the request for mapping the output
 * @return pointer to the description structure
 */
const papuga_StructInterfaceDescription* papuga_Request_struct_descriptions( const papuga_Request* self);

#ifdef __cplusplus
}
#endif
#endif

