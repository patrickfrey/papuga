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

#ifdef __cplusplus
extern "C" {
#endif

/*
 * @brief Request handler
 */
typedef struct papuga_RequestHandler papuga_RequestHandler;

/*
 * @brief Access control list
 */
typedef struct papuga_RequestAcl papuga_RequestAcl;

/* External declarations in request.h */
#ifndef _PAPUGA_REQUEST_H_INCLUDED
typedef struct papuga_RequestVariable papuga_RequestVariable;
typedef struct papuga_RequestAutomaton papuga_RequestAutomaton;
typedef struct papuga_Request papuga_Request;
#endif
/* External declarations in requestResult.h */
#ifndef _PAPUGA_REQUEST_RESULT_H_INCLUDED
typedef struct papuga_RequestResult papuga_RequestResult;
#endif

/*
 * @brief Defines the context of a request
 */
typedef struct papuga_RequestContext
{
	papuga_ErrorCode errcode;			/*< last error in the request context */
	papuga_Allocator allocator;			/*< allocator for this context */
	papuga_RequestVariable* variables;		/*< variables defined in the context */
	papuga_RequestAcl* acl;				/*< access control list */
	char allocator_membuf[ 1<<14];			/*< initial memory buffer for the allocator */
} papuga_RequestContext;

/*
 * @brief Creates a new context for handling a request
 * @param[in] self this pointer to the object to initialize
 */
void papuga_init_RequestContext( papuga_RequestContext* self);

/*
 * @brief Deletes a request context and its content
 * @param[in] self this pointer to the object to delete
 */
void papuga_destroy_RequestContext( papuga_RequestContext* self);

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
const papuga_ValueVariant* papuga_RequestContext_get_variable( papuga_RequestContext* self, const char* name);

/*
 * @brief Allow access to this context by a role
 * @param[in] self this pointer to the object to add the role for access to
 * @param[in] role name of role to grant access to this
 * @return true on success, false on failure
 */
bool papuga_RequestContext_allow_access( papuga_RequestContext* self, const char* role);

/*
 * @brief Creates a request handler
 * @param[in] classdefs class definitions referred to in host object references
 * @return pointer to request handler
 */
papuga_RequestHandler* papuga_create_RequestHandler();

/*
 * @brief Destroys a request handler
 * @param[in] self this pointer to the request handler to destroy
 */
void papuga_destroy_RequestHandler( papuga_RequestHandler* self);

/*
 * @brief Add the context (deep copy) to the request handler with the ownership of all host object references
 * @param[in] self this pointer to the request handler
 * @param[in] name name given to the context used to address the parent context when initializing a child context
 * @param[in,out] ctx context copied to handler with the ownership of all host object references moved
 * @param[out] errcode error code in case of error, untouched in case of success
 * @remark Not thread safe, synchronization has to be done by the caller
 * @return true on success, false on failure
 */
bool papuga_RequestHandler_add_context( papuga_RequestHandler* self, const char* name, papuga_RequestContext* ctx, papuga_ErrorCode* errcode);

/*
 * @brief Defines a new context for requests inherited from another context addressed by name in the request handler, checking credentials
 * @param[out] self this pointer to the request context initialized
 * @param[in] handler request handler to get the parent context from
 * @param[in] parent name of the parent context
 * @param[in] role name of the instance asking for granting access to the parent context
 * @param[out] errcode error code in case of error, untouched in case of success
 * @remark Thread safe, if writers (papuga_RequestHandler_add_.. and papuga_RequestHandler_allow_..) are synchronized
 * @return true on success, false on failure
 */
bool papuga_init_RequestContext_child( papuga_RequestContext* self, const papuga_RequestHandler* handler, const char* parent, const char* role, papuga_ErrorCode* errcode);

/*
 * @brief Defines a new context for requests inherited from another context addressed by name in the request handler, checking credentials
 * @param[in] self this pointer to the request handler
 * @param[in] name name given to the schema
 * @param[in] automaton automaton of the schema (ownership passed)
 * @remark Not thread safe, synchronization has to be done by the caller, read access is thread safe if writers are synchronized
 * @return true on success, false on memory allocation error
 */ 
bool papuga_RequestHandler_add_schema( papuga_RequestHandler* self, const char* name, papuga_RequestAutomaton* automaton);

/*
 * @brief Defines a new context for requests inherited from another context addressed by name in the request handler, checking credentials
 * @param[in] self this pointer to the request handler
 * @param[in] name name of the schema
 * @param[in] role name of role to grant access to this schema
 * @param[out] errcode error code in case of error, untouched in case of success
 * @remark Not thread safe, synchronization has to be done by the caller
 * @return true on success, false on memory allocation error
 */
bool papuga_RequestHandler_allow_schema_access( papuga_RequestHandler* self, const char* name, const char* role, papuga_ErrorCode* errcode);

/*
 * @brief Retrieve a schema for execution with validation of access rights
 * @param[in] self this pointer to the request handler
 * @param[in] name name of the schema
 * @param[in] role name of role to grant access to this schema
 * @param[out] errcode error code in case of error, untouched in case of success
 * @remark Thread safe, if writers (papuga_RequestHandler_add_.. and papuga_RequestHandler_allow_..) are synchronized
 * @return pointer to automaton on success, NULL on failure
 */
const papuga_RequestAutomaton* papuga_RequestHandler_get_schema( const papuga_RequestHandler* self, const char* name, const char* role, papuga_ErrorCode* errcode);

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

