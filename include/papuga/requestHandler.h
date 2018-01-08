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
#ifndef _PAPUGA_REQUEST_HANDLER_H_INCLUDED
#define _PAPUGA_REQUEST_HANDLER_H_INCLUDED
#include "papuga/typedefs.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct papuga_RequestHandler papuga_RequestHandler;

/*
 * \brief Defines a variable of the request in the list of variables
 */
typedef struct papuga_RequestVariable
{
	struct papuga_RequestVariable* next;		/*< next variable */
	const char* name;				/*< name of variable associated with this value */
	papuga_ValueVariant value;			/*< variable value associated with this name */
} papuga_RequestVariable;

/*
 * \brief Access control list
 */
typedef struct papuga_RequestAcl
{
	struct papuga_RequestAcl* next;			/*< next role allowed */
	const char* allowed_role;			/*< role allowed for accessing this item */
} papuga_RequestAcl;

/*
 * \brief Defines the context of a request
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
 * \brief Creates a new context for handling a request
 * \param[in] self this pointer to object to initialize
 */
void papuga_init_RequestContext( papuga_RequestContext* self);

/*
 * \brief Deletes a request context and its content
 * \param[in] self this pointer to object to delete
 */
void papuga_destroy_RequestContext( papuga_RequestContext* self);

/*
 * \brief Get the last error in the request
 * \param[in] self this pointer to object to get the error from
 * \return error code of last error
 */
papuga_ErrorCode papuga_RequestContext_last_error( const papuga_RequestContext* self);

/*
 * \brief Add a variable (deep copy) to the context, moving ownership of host object references to context
 * \param[in] self this pointer to object to add the variable to
 * \param[in] name name of variable to add
 * \param[in] value value of the variable to add, ownership of host object references are moved to created variable
 * \return true on success, false on failure
 */
bool papuga_RequestContext_add_variable( papuga_RequestContext* self, const char* name, papuga_ValueVariant* value);

/*
 * \brief Allow access to this context by a role
 * \param[in] self this pointer to object to add the role for access to
 * \param[in] role name of role to grant access to this
 * \return true on success, false on failure
 */
bool papuga_RequestContext_allow_access( papuga_RequestContext* self, const char* role);

/*
 * \brief Creates a request handler
 * \return pointer to request handler
 */
papuga_RequestHandler* papuga_create_RequestHandler();

/*
 * \brief Destroys a request handler
 * \param[in] self this pointer to request handler to destroy
 */
void papuga_destroy_RequestHandler( papuga_RequestHandler* self);

/*
 * \brief Add the context (deep copy) to the request handler with the ownership of all host object references
 * \param[in] self this pointer to request handler
 * \param[in] name name given to the context used to address the parent context when initializing a child context
 * \param[in,out] ctx context copied to handler with the ownership of all host object references moved
 * \param[out] errcode error code in case of error, untouched in case of success
 * \return true on success, false on failure
 */
bool papuga_RequestHandler_add_context( papuga_RequestHandler* self, const char* name, papuga_RequestContext* ctx, papuga_ErrorCode* errcode);

/*
 * \brief Defines a new context for requests inherited from another context addressed by name in the request handler, checking credentials
 * \param[out] self this pointer to request context initialized
 * \param[in] handler request handler to get the parent context from
 * \param[in] parent name of the parent context
 * \param[in] role name of the instance asking for granting access to the parent context
 * \param[out] errcode error code in case of error, untouched in case of success
 * \return true on success, false on failure
 */
bool papuga_init_RequestContext_child( papuga_RequestContext* self, const papuga_RequestHandler* handler, const char* parent, const char* role, papuga_ErrorCode* errcode);

#ifdef __cplusplus
}
#endif

#endif

