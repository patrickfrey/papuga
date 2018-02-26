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
#include "papuga/requestResult.h"
#include "papuga/requestLogger.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * @brief Request handler
 */
typedef struct papuga_RequestHandler papuga_RequestHandler;

typedef void (*papuga_LoggerProcedure)( int nofItems, ...);

/*
 * @brief Defines the context of a request
 */
typedef struct papuga_RequestContext
{
	papuga_ErrorCode errcode;			/*< last error in the request context */
	papuga_Allocator* allocator;			/*< allocator for this context */
	papuga_RequestVariable* variables;		/*< variables defined in the context */
	papuga_RequestLogger* logger;			/*< logger to use */
	const char* type;				/*< type identifier of this context used for schema selection papuga_RequestHandler_get_schema */
} papuga_RequestContext;

/*
 * @brief Creates a new context for handling a request
 * @param[in] self this pointer to the object to initialize
 * @param[in] allocator allocator to use
 * @param[in] logger logger interface to use
 */
void papuga_init_RequestContext( papuga_RequestContext* self, papuga_Allocator* allocator, papuga_RequestLogger* logger);

/*
 * @brief Get the last error in the request
 * @param[in] self this pointer to the object to get the error from
 * @return error code of last error
 */
papuga_ErrorCode papuga_RequestContext_last_error( const papuga_RequestContext* self);

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
 * @brief Creates a request handler
 * @param[in] logger logger interface to use
 * @return pointer to request handler
 */
papuga_RequestHandler* papuga_create_RequestHandler( papuga_RequestLogger* logger);

/*
 * @brief Destroys a request handler
 * @param[in] self this pointer to the request handler to destroy
 */
void papuga_destroy_RequestHandler( papuga_RequestHandler* self);

/*
 * @brief Add the context (deep copy) to the request handler with the ownership of all host object references
 * @param[in] self this pointer to the request handler
 * @param[in] type type name given to the context used to address it and its schemas
 * @param[in] name name given to the context used with the type to address it
 * @param[in,out] ctx context copied to handler with the ownership of all host object references moved
 * @param[out] errcode error code in case of error, untouched in case of success
 * @remark Not thread safe, synchronization has to be done by the caller
 * @return true on success, false on failure
 */
bool papuga_RequestHandler_add_context( papuga_RequestHandler* self, const char* type, const char* name, papuga_RequestContext* ctx, papuga_ErrorCode* errcode);

/*
* @brief List the names of contexts of a given type
* @param[in] self this pointer to the request handler
* @param[in] type type name of the contexts to list
* @param[in] buf buffer to use for result
* @param[in] bufsize size of buffer to use for result
* @return NULL terminated array of context names or NULL if the buffer buf is too small for the result
*/
const char** papuga_RequestHandler_list_contexts( const papuga_RequestHandler* self, const char* type, char const** buf, size_t bufsize);

/*
* @brief List the names of context types
* @param[in] self this pointer to the request handler
* @param[in] buf buffer to use for result
* @param[in] bufsize size of buffer to use for result
* @return NULL terminated array of context names or NULL if the buffer buf is too small for the result
*/
const char** papuga_RequestHandler_list_context_types( const papuga_RequestHandler* self, char const** buf, size_t bufsize);

/*
 * @brief Defines a new context for requests inherited from another context addressed by type and name in the request handler
 * @param[out] self this pointer to the request context initialized
 * @param[in] allocator allocator to use
 * @param[in] handler request handler to get the parent context from
 * @param[in] type type name of the context to select and inherit from
 * @param[in] name name of the context to select and inherit from
 * @param[out] errcode error code in case of error, untouched in case of success
 * @remark Thread safe, if writers (papuga_RequestHandler_add_.. and papuga_RequestHandler_allow_..) are synchronized
 * @return true on success, false on failure
 */
bool papuga_init_RequestContext_child( papuga_RequestContext* self, papuga_Allocator* allocator, const papuga_RequestHandler* handler, const char* type, const char* name, papuga_ErrorCode* errcode);

/*
 * @brief Find a stored context
 * @param[in] handler request handler to get the context from
 * @param[in] type type name of the context to find
 * @param[in] name name of the context to find
 * @remark Thread safe, if writers (papuga_RequestHandler_add_.. and papuga_RequestHandler_allow_..) are synchronized
 * @return pointer to the found context on success, NULL if not found
 */
const papuga_RequestContext* papuga_RequestHandler_find_context( const papuga_RequestHandler* handler, const char* type, const char* name);

/*
 * @brief Defines a new context for requests inherited from another context addressed by name in the request handler
 * @param[in] self this pointer to the request handler
 * @param[in] type type name name of the context the added schema is valid for
 * @param[in] name name given to the schema
 * @param[in] automaton pointer to automaton of the schema
 * @remark Not thread safe, synchronization has to be done by the caller
 * @return true on success, false on memory allocation error
 */ 
bool papuga_RequestHandler_add_schema( papuga_RequestHandler* self, const char* type, const char* name, const papuga_RequestAutomaton* automaton);

/*
 * @brief Test if a schema definition with a given name exists
 * @param[in] self this pointer to the request handler
 * @param[in] type type name of the context the schema is valid for
 * @param[in] schema name of the schema queried
 * @return true, if the schema exists, false else
 */
bool papuga_RequestHandler_has_schema( papuga_RequestHandler* self, const char* type, const char* schema);

/*
 * @brief Retrieve a schema for execution of a request
 * @param[in] self this pointer to the request handler
 * @param[in] type type name of the object that is base of this schema
 * @param[in] name name of the schema (the tuple [type,name] is identifying the schema)
 * @param[out] errcode error code in case of error, untouched in case of success
 * @remark Thread safe, if writers (papuga_RequestHandler_add_.. and papuga_RequestHandler_allow_..) are synchronized
 * @return pointer to automaton on success, NULL on failure
 */
const papuga_RequestAutomaton* papuga_RequestHandler_get_schema( const papuga_RequestHandler* self, const char* type, const char* name, papuga_ErrorCode* errcode);

/*
 * @brief Execute a request
 * @param[in,out] context context of the request
 * @param[in] request content of the request
 * @param[out] errorbuf buffer for the error message in case of error
 * @param[out] errorpos position in case of an error counted in request parser events (to reproduce the error location you have to rescan the source)
 * @return true on success, false on failure
 */
bool papuga_RequestContext_execute_request( papuga_RequestContext* context, const papuga_Request* request, papuga_ErrorBuffer* errorbuf, int* errorpos);

/*
 * @brief Initialize the result of a request
 * @param[out] self this pointer to the request result initialized
 * @param[in] context context of the request execution
 * @param[in] request content of the request
 * @return true on success, false on out of memory
 */
bool papuga_set_RequestResult( papuga_RequestResult* self, papuga_RequestContext* context, const papuga_Request* request);

#ifdef __cplusplus
}
#endif

#endif

