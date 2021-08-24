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

/*
* @brief Get the result not type as string
* @param[in] tp the type
* @result the name as string
*/
const char* papuga_RequestResultNodeTypeName( papuga_RequestResultNodeType tp);

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

#define papuga_RequestResultDescription_MaxNofContentVars 7

/*
* @brief Request result description structure
* @member name name of the result, root element (constant, string not copied), NULL if not content defined (delegate request without content)
* @member schema name of the schema that handles the request if the result forms a request to other servers
* @member requestmethod request method if the result forms a request to other servers
* @member addressvar name of the variable with the urls if the result forms a request to other servers
* @member path additional path added to urls referenced in address variables if the result forms a request to other servers
* @member nodear nodes of the result description that trigger the creation of result elements
* @member nodearallocsize allocation size of nodear
* @member nodearsize fill size of nodear
* @member contentvar list of variable names addressing content to attach to the result (NULL terminated)
* @member contentvarsize number of elements in contentvar
*/
typedef struct papuga_RequestResultDescription
{
	const char* name;
	const char* schema;
	const char* requestmethod;
	const char* addressvar;
	const char* path;
	papuga_RequestResultNodeDescription* nodear;
	int nodearallocsize;
	int nodearsize;
	const char* contentvar[ papuga_RequestResultDescription_MaxNofContentVars+1];
	int contentvarsize;
} papuga_RequestResultDescription;

/*
* @brief RequestResultDescription constructor function
* @param[in] name_ name of the result, root element (constant, string not copied), NULL if not content defined (delegate request without content)
* @param[in] schema_ name of the schema that handles the request if the result forms a request to other servers
* @param[in] requestmethod_ request method if the result forms a request to other servers
* @param[in] addressvar_ name of the variable with the urls if the result forms a request to other servers
* @param[in] path_ additional path added to urls referenced in address variables if the result forms a request to other servers
* @return structure to free with papuga_destroy_RequestResultDescription
*/
papuga_RequestResultDescription* papuga_create_RequestResultDescription( const char* name_, const char* schema_, const char* requestmethod_, const char* addressvar_, const char* path_);

/*
 * @brief Destructor function
 * @param[in] self pointer to structure to free content
 */
void papuga_destroy_RequestResultDescription( papuga_RequestResultDescription* self);

/*
 * @brief Add a constant node to this output description
 * @param[in,out] descr result description to modify
 * @param[in] inputselect tag select expression that triggers the output of this result node
 * @param[in] tagname name of the output tag of this node or NULL if no tag is printed for this result node
 * @param[in] constant value of this result node printed
 * @return true in case of success, false in case of a memory allocation error
 */
bool papuga_RequestResultDescription_push_constant( papuga_RequestResultDescription* descr, const char* inputselect, const char* tagname, const char* constant);

/*
 * @brief Add a structure node to this output description
 * @param[in,out] descr result description to modify
 * @param[in] inputselect tag select expression that triggers the output of this result node
 * @param[in] tagname name of the output tag of this node or NULL for an open array element
 * @param[in] array true if the structure is an array
 * @return true in case of success, false in case of a memory allocation error
 */
bool papuga_RequestResultDescription_push_structure( papuga_RequestResultDescription* descr, const char* inputselect, const char* tagname, bool array);

/*
 * @brief Add a node referring to an item of the input to this output description
 * @param[in,out] descr result description to modify
 * @param[in] inputselect tag select expression that triggers the output of this result node
 * @param[in] tagname name of the output tag of this node or NULL for an open array element
 * @param[in] itemid identifier of the node taken from input
 * @param[in] resolvetype the resolve type 
 * @return true in case of success, false in case of a memory allocation error
 */
bool papuga_RequestResultDescription_push_input( papuga_RequestResultDescription* descr, const char* inputselect, const char* tagname, int itemid, papuga_ResolveType resolvetype);

/*
 * @brief Add a node referring to a result of a call to this output description
 * @param[in,out] descr result description to modify
 * @param[in] inputselect tag select expression that triggers the output of this result node
 * @param[in] tagname name of the output tag of this node or NULL for an open array element
 * @param[in] variable name of the variable the result is assigned to
 * @param[in] resolvetype the resolve type 
 * @return true in case of success, false in case of a memory allocation error
 */
bool papuga_RequestResultDescription_push_callresult( papuga_RequestResultDescription* descr, const char* inputselect, const char* tagname, const char* variable, papuga_ResolveType resolvetype);

/*
 * @brief Add a variable to add the content of to the result
 * @param[in,out] descr result description to modify
 * @param[in] variable name of the variable the result is extended with
 * @return true in case of success, false in case of a memory allocation error or if too many variables defined
 */
bool papuga_RequestResultDescription_push_content_variable( papuga_RequestResultDescription* descr, const char* variable);

#ifdef __cplusplus
}
#endif
#endif
