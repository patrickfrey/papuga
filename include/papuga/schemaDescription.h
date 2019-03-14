/*
 * Copyright (c) 2019 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/*
* \brief Automaton to describe and build papuga XML and JSON requests
* \file schemaDescription.h
*/
#ifndef _PAPUGA_REQUEST_SCHEMA_DESCRIPTION_H_INCLUDED
#define _PAPUGA_REQUEST_SCHEMA_DESCRIPTION_H_INCLUDED

#include "papuga/typedefs.h"
#include "papuga/request.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct papuga_SchemaDescription papuga_SchemaDescription;

/*
 * @brief Create an empty description of a schema to be initialized
 * @return The schema description structure
 */
papuga_SchemaDescription* papuga_create_SchemaDescription();

/*
 * @brief Destroy an schema description
 * @param[in] self the schema description structure to free
 */
void papuga_destroy_SchemaDescription( papuga_SchemaDescription* self);

/*
 * @brief Get the last error when building the schema description
 * @param[in] self schema description to get the last error from
 * @return the error code
 */
papuga_ErrorCode papuga_SchemaDescription_last_error( const papuga_SchemaDescription* self);

/*
 * @brief Add an element (structure or atom) to the schema description
 * @remark not threadsafe
 * @param[in] self schema description to add element to
 * @param[in] id identifier of the element or -1 if undefined
 * @param[in] expression select expression of the element
 * @param[in] valueType in case of an atomic value or papuga_TypeVoid else
 * @param[in] examples semicolon ';' separated list of example values to use for this element in the examples in the case of an atomic type, NULL else
 * @return true if operation succeeded, false if it failed (call papuga_SchemaDescription_last_error for the reason of the operation failure)
 */
bool papuga_SchemaDescription_add_element( papuga_SchemaDescription* self, int id, const char* expression, papuga_Type valueType, const char* examples);

/*
 * @brief Declare dependency graph arc
 * @remark not threadsafe
 * @param[in] self schema description to add relation to
 * @param[in] sink_id identifier of the sink element
 * @param[in] source_id identifier of the source element
 * @param[in] resolveType type of relation
 * @return true if operation succeeded, false if it failed (call papuga_SchemaDescription_last_error for the reason of the operation failure)
 */
bool papuga_SchemaDescription_add_relation( papuga_SchemaDescription* self, int sink_id, int source_id, papuga_ResolveType resolveType);

/*
 * @brief Declare a description of a schema to be finished
 * @remark not threadsafe
 * @param[in] self schema description to close for further input
 * @note After this operation no more elements can be added
 * @return true if operation succeeded, false if it failed (call papuga_SchemaDescription_last_error for the reason of the operation failure)
 */
bool papuga_SchemaDescription_done( papuga_SchemaDescription* self);

/*
 * @brief Get the description of the schema as text
 * @remark not threadsafe
 * @param[in] self schema description to get the description of
 * @param[in] allocator to use for result
 * @param[in] doctype of the result (also defines the schema definition language used)
 * @param[in] enc character set encoding of the result
 * @return the description of the schema as text or NULL in case of error (call papuga_SchemaDescription_last_error for the reason of the operation failure)
 */
const char* papuga_SchemaDescription_get_text( const papuga_SchemaDescription* self, papuga_Allocator* allocator, papuga_ContentType doctype, papuga_StringEncoding enc);

/*
 * @brief Get the example of the schema as text
 * @param[in] self schema description to get the description of
 * @param[in] allocator to use for result
 * @param[in] doctype of the result (also defines the schema definition language used)
 * @param[in] enc character set encoding of the result
 * @return the description of the example as text or NULL in case of error (call papuga_SchemaDescription_last_error for the reason of the operation failure)
 */
const char* papuga_SchemaDescription_get_example( const papuga_SchemaDescription* self, papuga_Allocator* allocator, papuga_ContentType doctype, papuga_StringEncoding enc);

#ifdef __cplusplus
}
#endif
#endif
