/*
 * Copyright (c) 2019 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/* @brief Structures and functions for describing a request result
 * @file requestResult.h
 */
#ifndef _PAPUGA_REQUEST_RESULT_H_INCLUDED
#define _PAPUGA_REQUEST_RESULT_H_INCLUDED
#include "papuga/typedefs.h"
#include "papuga/request.h"
#include "papuga/serialization.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * @brief Type of a result node
 */
typedef enum papuga_RequestResultNodeType
{
	papuga_ResultNodeConstant,
	papuga_ResultNodeOpenStructure,
	papuga_ResultNodeCloseStructure,
	papuga_ResultNodeOpenArray,
	papuga_ResultNodeCloseArray,
	papuga_ResultNodeInputReference,
	papuga_ResultNodeResultReference
} papuga_RequestResultNodeType;

typedef struct papuga_RequestResultNodeDescription
{
	const char* inputselect;
	papuga_RequestResultNodeType type;
	papuga_ResolveType resolvetype;
	const char* tagname;
	union
	{
		int itemid;
		const char* str;
	} value;
} papuga_RequestResultNodeDescription;

typedef struct papuga_RequestResultDescription
{
	const char* name;
	const char* schema;
	const char* requestmethod;
	const char* addressvar;
	papuga_RequestResultNodeDescription* nodear;
	int nodearallocsize;
	int nodearsize;
} papuga_RequestResultDescription;

/*
* @brief RequestResultDescription constructor function
* @param[in] name_ name of the result, root element (constant, string not copied)
* @param[in] schema_ name of the schema that handles the request if the result forms a request to other servers
* @param[in] requestmethod_ request method if the result forms a request to other servers
* @param[in] addressvar_ name of the variable with the urls if the result forms a request to other servers
* @return structure to free with papuga_destroy_RequestResultDescription
*/
papuga_RequestResultDescription* papuga_create_RequestResultDescription( const char* name_, const char* schema_, const char* requestmethod_, const char* addressvar_);

/*
 * @brief Destructor function
 * @param[in] self pointer to structure to free content
 */
void papuga_destroy_RequestResultDescription( papuga_RequestResultDescription* self);

/*
 * @brief Add a constant node to this output description
 * @param[in] inputselect tag select expression that triggers the output of this result node
 * @param[in] tagname name of the output tag of this node or NULL if no tag is printed for this result node
 * @param[in] constant value of this result node printed
 * @return true in case of success, false in case of a memory allocation error
 */
bool papuga_RequestResultDescription_push_constant( papuga_RequestResultDescription* descr, const char* inputselect, const char* tagname, const char* constant);

/*
 * @brief Add a structure node to this output description
 * @param[in] inputselect tag select expression that triggers the output of this result node
 * @param[in] tagname name of the output tag of this node or NULL for an open array element
 * @param[in] array true if the structure is an array
 * @return true in case of success, false in case of a memory allocation error
 */
bool papuga_RequestResultDescription_push_structure( papuga_RequestResultDescription* descr, const char* inputselect, const char* tagname, bool array);

/*
 * @brief Add a node referring to an item of the input to this output description
 * @param[in] inputselect tag select expression that triggers the output of this result node
 * @param[in] tagname name of the output tag of this node or NULL for an open array element
 * @param[in] itemid identifier of the node taken from input
 * @param[in] resolvetype the resolve type 
 * @return true in case of success, false in case of a memory allocation error
 */
bool papuga_RequestResultDescription_push_input( papuga_RequestResultDescription* descr, const char* inputselect, const char* tagname, int itemid, papuga_ResolveType resolvetype);

/*
 * @brief Add a node referring to a result of a call to this output description
 * @param[in] inputselect tag select expression that triggers the output of this result node
 * @param[in] tagname name of the output tag of this node or NULL for an open array element
 * @param[in] variable name of the variable the result is assigned to
 * @param[in] resolvetype the resolve type 
 * @return true in case of success, false in case of a memory allocation error
 */
bool papuga_RequestResultDescription_push_callresult( papuga_RequestResultDescription* descr, const char* inputselect, const char* tagname, const char* variable, papuga_ResolveType resolvetype);

#ifdef __cplusplus
}
#endif
#endif
