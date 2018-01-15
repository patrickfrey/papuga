/*
 * Copyright (c) 2017 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/* \brief Structure to build and map the Result of an XML/JSON request
 * @file requestResult.h
 */
#ifndef _PAPUGA_REQUEST_RESULT_H_INCLUDED
#define _PAPUGA_REQUEST_RESULT_H_INCLUDED
#include "papuga/typedefs.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * @brief Defines one element of the result of a request
 */
typedef struct papuga_RequestResultNode
{
	papuga_RequestResultNode* next;
	const char* name;
	papuga_ValueVariant value;
} papuga_RequestResultNode;

/* Forward declaration interfaceDescription.h */
typedef struct papuga_StructInterfaceDescription papuga_StructInterfaceDescription;

/*
 * @brief Defines the result of a request
 */
typedef struct papuga_RequestResult
{
	const char* name;						//< name of the result (unique top level root element name for XML)
	const papuga_StructInterfaceDescription* structdefs;		//< structs structure descriptions addressed in serialization in values
	papuga_RequestResultNode* nodes;				//< list of result elements
} papuga_RequestResult;

/*
* @brief Map a request result to XML in a defined encoding
* @param[in] self pointer to structure
* @param[in] enc encoding of the output XML
* @param[out] len length of the output in character units, depending on the encoding specified
* @param[out] err error code in case of error (untouched if call succeeds)
* @return the dumped XML allocated with malloc on success, NULL on failure
* @remark return value allocated with malloc, to be freed with free
*/
void* papuga_RequestResult_toxml( const papuga_RequestResult* self, papuga_StringEncoding enc, size_t* len, papuga_ErrorCode* err);

/*
* @brief Map a request result to JSON in a defined encoding
* @param[in] self pointer to structure
* @param[in] enc encoding of the output JSON
* @param[out] len length of the output in character units, depending on the encoding specified
* @param[out] err error code in case of error (untouched if call succeeds)
* @return the dumped XML allocated with malloc on success, NULL on failure
* @remark return value allocated with malloc, to be freed with free
*/
void* papuga_RequestResult_tojson( const papuga_RequestResult* self, papuga_StringEncoding enc, size_t* len, papuga_ErrorCode* err);


/*
* @brief Dump the request result in readable form
* @param[in] self pointer to structure
* @param[out] len length of the output (UTF-8) in bytes
* @return the request allocated with malloc on success, NULL on memory allocation error
*/
char* papuga_RequestResult_tostring( const papuga_RequestResult* self, size_t* len);


#ifdef __cplusplus
}
#endif

#endif
