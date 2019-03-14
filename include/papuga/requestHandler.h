/*
 * Copyright (c) 2017 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/*
* \brief Structures for the context (state) of the execution of an XML/JSON request
* \file requestHandler.h
*/
#ifndef _PAPUGA_REQUEST_HANDLER_H_INCLUDED
#define _PAPUGA_REQUEST_HANDLER_H_INCLUDED
#include "papuga/typedefs.h"
#include "papuga/request.h"
#include "papuga/schemaDescription.h"
#include "papuga/requestLogger.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * @brief Request handler
 */
typedef struct papuga_RequestHandler papuga_RequestHandler;

/*
 * @brief Describes a method and its parameters
 */
typedef struct papuga_RequestMethodDescription
{
	papuga_RequestMethodId id;			/*< method identifier */
	int* paramtypes;				/*< 0 terminated list of parameter type identifiers */
	bool has_content;				/*< true, if the method accepts or rejects content or parameters passed with the request */
	int httpstatus_success;				/*< HTTP status code in case of success */
	const char* resulttype;				/*< NULL in case of content, HTTP header variable name else */
	const char* result_rootelem;			/*< root element name for mapping result */
	const char* result_listelem;			/*< list element name for mapping result in case of an array */
} papuga_RequestMethodDescription;

/*
 * @brief Creates a new context for handling a request
 * @return the request context created or NULL in case of a memory allocation error
 */
papuga_RequestContext* papuga_create_RequestContext();

/*
 * @brief Destroys a request context
 * @param[in] self this pointer to the request context to destroy
 */
void papuga_destroy_RequestContext( papuga_RequestContext* self);

/*
 * @brief Get the last error in the request
 * @param[in] self this pointer to the object to get the error from
 * @param[in] clear true if the error should be cleared
 * @return error code of last error
 */
papuga_ErrorCode papuga_RequestContext_last_error( papuga_RequestContext* self, bool clear);

/*
 * @brief Add a variable (deep copy) to the context, moving ownership of host object references to context
 * @param[in] self this pointer to the object to add the variable to
 * @param[in] name name of variable to add
 * @param[in] value value of the variable to add, ownership of host object references are moved to created variable
 * @return true on success, false on failure
 */
bool papuga_RequestContext_add_variable( papuga_RequestContext* self, const char* name, papuga_ValueVariant* value);

/*
 * @brief Get a variable reference in the context
 * @param[in] self this pointer to the object to get the variable reference from
 * @param[in] name name of variable to get
 * @return the variable value on success, NULL if it does not exist
 */
const papuga_ValueVariant* papuga_RequestContext_get_variable( const papuga_RequestContext* self, const char* name);

/*
* @brief List the names of variables defined in a context
* @param[in] self this pointer to the context
* @param[in] max_inheritcnt maximum number of inheritance steps a selected variable has gone through or -1, if inheritance level is not a criterion (0: own variable, 1: parent context variable, 2: gran parent ...)
* @param[in] buf buffer to use for result
* @param[in] bufsize size of buffer to use for result
* @return NULL terminated array of variable names or NULL if the buffer buf is too small for the result
*/
const char** papuga_RequestContext_list_variables( const papuga_RequestContext* self, int max_inheritcnt, char const** buf, size_t bufsize);

/*
 * @brief Inherit all non local variables from another context
 * @param[in,out] self this pointer
 * @param[in] context to inherit from 
 * @remark Not thread safe, synchronization has to be done by the caller
 * @return true on success, false on failure
 */
bool papuga_RequestContext_inherit( papuga_RequestContext* self, const papuga_RequestHandler* handler, const char* type, const char* name);

/*
 * @brief Creates a request handler
 * @param[in] classdefs interface description of structures and classes
 * @return pointer to request handler
 */
papuga_RequestHandler* papuga_create_RequestHandler( const papuga_ClassDef* classdefs);

/*
 * @brief Destroys a request handler
 * @param[in] self this pointer to the request handler to destroy
 */
void papuga_destroy_RequestHandler( papuga_RequestHandler* self);

/*
 * @brief Transfer the context (with ownership) to the request handler
 * @param[in] self this pointer to the request handler
 * @param[in] type type name given to the context used to address it and its schemas
 * @param[in] name name given to the context used with the type to address it
 * @param[in,out] context context moved with ownership to handler
 * @param[out] errcode error code in case of error, untouched in case of success
 * @remark Not thread safe, synchronization has to be done by the caller
 * @return true on success, false on failure
 */
bool papuga_RequestHandler_transfer_context( papuga_RequestHandler* self, const char* type, const char* name, papuga_RequestContext* context, papuga_ErrorCode* errcode);

/*
 * @brief Destroy a context defined if it exists
 * @param[in] self this pointer to the request handler
 * @param[in] type type name given to the context used to address it and its schemas
 * @param[in] name name given to the context used with the type to address it
 * @remark Not thread safe, synchronization has to be done by the caller
 * @return true on success, false if the addressed context does not exist or in case of an error
 */
bool papuga_RequestHandler_remove_context( papuga_RequestHandler* self, const char* type, const char* name, papuga_ErrorCode* errcode);

/*
 * @brief Defines a new context for requests inherited from another context addressed by name in the request handler
 * @param[in] self this pointer to the request handler
 * @param[in] type type name name of the context the added schema is valid for
 * @param[in] name name given to the schema
 * @param[in] automaton pointer to automaton of the schema
 * @remark Not thread safe, synchronization has to be done by the caller
 * @return true on success, false on memory allocation error
 */ 
bool papuga_RequestHandler_add_schema( papuga_RequestHandler* self, const char* type, const char* name, const papuga_RequestAutomaton* automaton, const papuga_SchemaDescription* description);

/*
 * @brief List the schema names defined for a given context type
 * @param[in] self this pointer to the request handler
 * @param[in] type type name of the context the schema is valid for or NULL, if all schema identifiers should be returned
 * @param[in] buf buffer to use for result
 * @param[in] bufsize size of buffer to use for result
 * @return NULL terminated array of context names or NULL if the buffer buf is too small for the result
 */
const char** papuga_RequestHandler_list_schema_names( const papuga_RequestHandler* self, const char* type, char const** buf, size_t bufsize);

/*
 * @brief Retrieve the automaton for execution of a request
 * @param[in] self this pointer to the request handler
 * @param[in] type type name of the object that is base of this schema
 * @param[in] name name of the schema (the tuple [type,name] is identifying the schema)
 * @return pointer to automaton on success, NULL if not found
 */
const papuga_RequestAutomaton* papuga_RequestHandler_get_automaton( const papuga_RequestHandler* self, const char* type, const char* name);

/*
 * @brief Retrieve the description of the schema associated with a request type
 * @param[in] self this pointer to the request handler
 * @param[in] type type name of the object that is base of this schema
 * @param[in] name name of the schema (the tuple [type,name] is identifying the schema)
 * @return pointer to automaton on success, NULL if not found
 */
const papuga_SchemaDescription* papuga_RequestHandler_get_description( const papuga_RequestHandler* self, const char* type, const char* name);

/*
 * @brief Attach a method addressed by name with parameter description to an object class
 * @param[in] self this pointer to the request handler
 * @param[in] name name of the method (HTTP request method in a request)
 * @param[in] descr pointer to description of the method
 * @remark Not thread safe, synchronization has to be done by the caller
 * @return true on success, false on memory allocation error
 */
bool papuga_RequestHandler_add_method( papuga_RequestHandler* self, const char* name, const papuga_RequestMethodDescription* descr);

/*
 * @brief Get the identifier and the parameter description of a method addressed by name to an object class
 * @param[in] self this pointer to the request handler
 * @param[in] classid identifier of the object class (owner of the method)
 * @param[in] methodname name of the method (HTTP request method in a request)
 * @return pointer to method identifier, NULL if not found
 */
const papuga_RequestMethodDescription* papuga_RequestHandler_get_method( const papuga_RequestHandler* self, int classid, const char* methodname, bool with_content);

/*
 * @brief List the methods defined for a given class
 * @param[in] self this pointer to the request handler
 * @param[in] buf buffer to use for result
 * @param[in] bufsize size of buffer to use for result
 * @return NULL terminated array of context names or NULL if the buffer buf is too small for the result
 */
const char** papuga_RequestHandler_list_methods( const papuga_RequestHandler* self, int classid, char const** buf, size_t bufsize);

/*
 * @brief Execute a request
 * @param[in,out] context context of the request
 * @param[in] request content of the request
 * @param[out] errorbuf buffer for the error message in case of error
 * @param[out] errorpos position in case of an error counted in request parser events (to reproduce the error location you have to rescan the source)
 * @return true on success, false on failure
 */
bool papuga_RequestContext_execute_request( papuga_RequestContext* context, const papuga_Request* request, papuga_RequestLogger* logger, papuga_ErrorBuffer* errorbuf, int* errorpos);

/*
 * @brief Initialize the result of a request as value variant
 * @param[in,out] self serialization where to serialize the request result in a context to
 * @param[in] context context of the request execution
 * @param[in] request content of the request
 * @return true on success, false on out of memory
 */
bool papuga_Serialization_serialize_request_result( papuga_Serialization* self, papuga_RequestContext* context, const papuga_Request* request);

#ifdef __cplusplus
}
#endif

#endif

