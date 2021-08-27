/*
 * Copyright (c) 2017 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/*
* \brief Request context data and the collection of it
* \file requestContext.h
*/
#ifndef _PAPUGA_REQUEST_CONTEXT_H_INCLUDED
#define _PAPUGA_REQUEST_CONTEXT_H_INCLUDED
#include "papuga/typedefs.h"
#include "papuga/interfaceDescription.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * @brief Request handler
 */
typedef struct papuga_RequestContext papuga_RequestContext;
typedef struct papuga_RequestContextPool papuga_RequestContextPool;

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
bool papuga_RequestContext_define_variable( papuga_RequestContext* self, const char* name, papuga_ValueVariant* value);

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
bool papuga_RequestContext_inherit( papuga_RequestContext* self, const papuga_RequestContextPool* handler, const char* type, const char* name);

/*
 * @brief Creates a request handler
 * @return pointer to request handler
 */
papuga_RequestContextPool* papuga_create_RequestContextPool();

/*
 * @brief Destroys a request handler
 * @param[in] self this pointer to the request handler to destroy
 */
void papuga_destroy_RequestContextPool( papuga_RequestContextPool* self);

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
bool papuga_RequestContextPool_transfer_context( papuga_RequestContextPool* self, const char* type, const char* name, papuga_RequestContext* context, papuga_ErrorCode* errcode);

/*
 * @brief Destroy a context defined if it exists
 * @param[in] self this pointer to the request handler
 * @param[in] type type name given to the context used to address it and its schemas
 * @param[in] name name given to the context used with the type to address it
 * @remark Not thread safe, synchronization has to be done by the caller
 * @return true on success, false if the addressed context does not exist or in case of an error
 */
bool papuga_RequestContextPool_remove_context( papuga_RequestContextPool* self, const char* type, const char* name, papuga_ErrorCode* errcode);

/*
 * @brief Get the dump of the context as string for debugging purposes
 * @param[in] self context to dump
 * @param[in] allocator allocator to use for the result string copy or NULL if std::malloc should be used
 * @return the contents of the context as readable, null terminated string
 */
const char* papuga_RequestContext_debug_tostring( const papuga_RequestContext* self, papuga_Allocator* allocator, papuga_StructInterfaceDescription* structdefs);

/*
 * @brief Get the dump of the context map of the request handler as string for debugging purposes
 * @param[in] self request handler to dump
 * @param[in] allocator allocator to use for the result string copy or NULL if std::malloc should be used
 * @param[in] structdefs structure descriptions for dump
 * @return the contents of the context map as readable, null terminated string
 */
const char* papuga_RequestContextPool_debug_contextmap_tostring( const papuga_RequestContextPool* self, papuga_Allocator* allocator, papuga_StructInterfaceDescription* structdefs);

#ifdef __cplusplus
}
#endif

#endif

