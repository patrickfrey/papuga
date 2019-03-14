/*
 * Copyright (c) 2019 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/*
* \brief Automaton to describe and build papuga XML and JSON requests
* \file schemeDescription.h
*/
#ifndef _PAPUGA_REQUEST_DESCRIPTION_H_INCLUDED
#define _PAPUGA_REQUEST_DESCRIPTION_H_INCLUDED

#include "papuga/typedefs.h"
#include "papuga/request.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct papuga_SchemeDescription papuga_SchemeDescription;

/*
 * @brief Create an empty description of a scheme to be initialized
 * @return The scheme description structure
 */
papuga_SchemeDescription* papuga_create_SchemeDescription();

/*
 * @brief Destroy an scheme description
 * @param[in] self the scheme description structure to free
 */
void papuga_destroy_SchemeDescription( papuga_SchemeDescription* self);

/*
 * @brief Get the last error when building the scheme description
 * @param[in] self scheme description to get the last error from
 * @return the error code
 */
papuga_ErrorCode papuga_SchemeDescription_last_error( const papuga_SchemeDescription* self);

/*
 * @brief Add an element (structure or atom) to the scheme description
 * @remark not threadsafe
 * @param[in] self scheme description to add element to
 * @param[in] id identifier of the element or -1 if undefined
 * @param[in] expression select expression of the element
 * @param[in] valueType in case of an atomic value or papuga_TypeVoid else
 * @param[in] examples semicolon ';' separated list of example values to use for this element in the examples in the case of an atomic type, NULL else
 * @return true if operation succeeded, false if it failed (call papuga_SchemeDescription_last_error for the reason of the operation failure)
 */
bool papuga_SchemeDescription_add_element( papuga_SchemeDescription* self, int id, const char* expression, papuga_Type valueType, const char* examples);

/*
 * @brief Declare dependency graph arc
 * @remark not threadsafe
 * @param[in] self scheme description to add relation to
 * @param[in] sink_id identifier of the sink element
 * @param[in] source_id identifier of the source element
 * @param[in] resolveType type of relation
 * @return true if operation succeeded, false if it failed (call papuga_SchemeDescription_last_error for the reason of the operation failure)
 */
bool papuga_SchemeDescription_add_relation( papuga_SchemeDescription* self, int sink_id, int source_id, papuga_ResolveType resolveType);

/*
 * @brief Declare a description of a scheme to be finished
 * @remark not threadsafe
 * @param[in] self scheme description to close for further input
 * @note After this operation no more elements can be added
 * @return true if operation succeeded, false if it failed (call papuga_SchemeDescription_last_error for the reason of the operation failure)
 */
bool papuga_SchemeDescription_done( papuga_SchemeDescription* self);

/*
 * @brief Get the description of the scheme as text
 * @remark not threadsafe
 * @param[in] self scheme description to get the description of
 * @return the description of the scheme as text or NULL in case of error (call papuga_SchemeDescription_last_error for the reason of the operation failure)
 */
const char* papuga_SchemeDescription_get_text( const papuga_SchemeDescription* self);

/*
 * @brief Get the example of the scheme as text
 * @param[in] self scheme description to get the description of
 * @return the description of the example as text or NULL in case of error (call papuga_SchemeDescription_last_error for the reason of the operation failure)
 */
const char* papuga_SchemeDescription_get_example( const papuga_SchemeDescription* self);

#ifdef __cplusplus
}
#endif
#endif
