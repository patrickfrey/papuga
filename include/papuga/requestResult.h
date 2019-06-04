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
#include "papuga/serialization.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * @brief Describes a result node
 */
typedef enum papuga_RequestResultNodeType
{
	papuga_ResultNodeConstant,
	papuga_ResultNodeOpenStructure,
	papuga_ResultNodeCloseStructure,
	papuga_ResultNodeInputReference,
	papuga_ResultNodeResultReference
} papuga_RequestResultNodeType;

typedef struct papuga_RequestResultNodeDescription
{
	const char* inputselect;
	papuga_RequestResultNodeType type;
	const char* tagname;
	union
	{
		int nodeid;
		const char* variable;
		const char* constant;
	} value;
} papuga_RequestResultNodeDescription;

typedef struct papuga_RequestResultDescription
{
	papuga_Allocator* allocator;
	papuga_RequestResultNodeDescription* nodear;
	int nodearallocsize;
	int nodearsize;
} papuga_RequestResultDescription;

/*
* @brief RequestResultDescription constructor
* @param[out] self pointer to structure 
* @param[in] allocator_ pointer to allocator to use
*/
#define papuga_init_RequestResultDescription(self_,allocator_)		{papuga_RequestResultDescription* s = (self_); s->allocator=(allocator_); s->nodear=0; s->nodearallocsize=0; s->nodearsize=0;}

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
 * @return true in case of success, false in case of a memory allocation error
 */
bool papuga_RequestResultDescription_push_structure( papuga_RequestResultDescription* descr, const char* inputselect, const char* tagname);

/*
 * @brief Add a node referring to an item of the input to this output description
 * @param[in] inputselect tag select expression that triggers the output of this result node
 * @param[in] tagname name of the output tag of this node or NULL for an open array element
 * @param[in] nodeid identifier of the node taken from input
 * @return true in case of success, false in case of a memory allocation error
 */
bool papuga_RequestResultDescription_push_input( papuga_RequestResultDescription* descr, const char* inputselect, const char* tagname, int nodeid);

/*
 * @brief Add a node referring to a result of a call to this output description
 * @param[in] inputselect tag select expression that triggers the output of this result node
 * @param[in] tagname name of the output tag of this node or NULL for an open array element
 * @param[in] variable name of the variable the result is assigned to
 * @return true in case of success, false in case of a memory allocation error
 */
bool papuga_RequestResultDescription_push_callresult( papuga_RequestResultDescription* descr, const char* inputselect, const char* tagname, const char* variable);

