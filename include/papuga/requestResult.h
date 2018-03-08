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
#include "papuga/interfaceDescription.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * @brief Defines one element of the result of a request
 */
typedef struct papuga_RequestResultNode
{
	struct papuga_RequestResultNode* next;
	const char* name;
	bool name_optional;
	papuga_ValueVariant value;
} papuga_RequestResultNode;

/*
 * @brief Defines the result of a request
 */
typedef struct papuga_RequestResult
{
	papuga_Allocator* allocator;					/*< reference to allocator to use */
	const char* name;						/*< name of the result (unique top level root element name for XML) */
	const papuga_StructInterfaceDescription* structdefs;		/*< structs structure descriptions addressed in serialization in values */
	papuga_RequestResultNode* nodes;				/*< list of result elements */
} papuga_RequestResult;

/*
 * @brief Initializes a result of a request with a single value
 * @param[in] allocator pointer to allocator to use
 * @param[in] self pointer to structure initialized
 * @param[in] rootname name of the result root element
 * @param[in] elemname name of a result element
 * @param[in] value pointer to content of the result
 * @return true on success, false on memory allocation error
 */
bool papuga_init_RequestResult_single(
		papuga_RequestResult* self,
		papuga_Allocator* allocator,
		const char* rootname,
		const char* elemname,
		const papuga_StructInterfaceDescription* structdefs,
		const papuga_ValueVariant* value);

/*
* @brief Map a request result to XML in a defined encoding
* @param[in] self pointer to structure
* @param[in] enc encoding of the output XML
* @param[out] len length of the output in character units, depending on the encoding specified
* @param[out] err error code in case of error (untouched if call succeeds)
* @return the dumped XML (allocated in result allocator) on success, NULL on failure
*/
void* papuga_RequestResult_toxml( const papuga_RequestResult* self, papuga_StringEncoding enc, size_t* len, papuga_ErrorCode* err);

/*
* @brief Map a request result to JSON in a defined encoding
* @param[in] self pointer to structure
* @param[in] enc encoding of the output JSON
* @param[out] len length of the output in character units, depending on the encoding specified
* @param[out] err error code in case of error (untouched if call succeeds)
* @return the dumped JSON (allocated in result allocator) on success, NULL on failure
*/
void* papuga_RequestResult_tojson( const papuga_RequestResult* self, papuga_StringEncoding enc, size_t* len, papuga_ErrorCode* err);

/*
* @brief Map a request result to HTML5 div style in a defined encoding with some injected meta data
* @param[in] self pointer to structure
* @param[in] enc encoding of the output HTML
* @param[out] len length of the output in character units, depending on the encoding specified
* @param[out] err error code in case of error (untouched if call succeeds)
* @return the dumped HTML (allocated in result allocator) on success, NULL on failure
*/
void* papuga_RequestResult_tohtml5( const papuga_RequestResult* self, papuga_StringEncoding enc, const char* head, size_t* len, papuga_ErrorCode* err);


/*
* @brief Dump the request result in readable form
* @param[in] self pointer to structure
* @param[in] maxdepth maximum recursion depth for printing structures
* @param[out] len length of the output (UTF-8) in bytes
* @return the dumped request allocated with malloc on success, NULL on memory allocation error
*/
char* papuga_RequestResult_tostring( const papuga_RequestResult* self, int maxdepth, size_t* len);

/*
* @brief Dump the request result as text/plain
* @param[in] self pointer to structure
* @param[in] enc encoding of the output text
* @param[out] len length of the output in character units, depending on the encoding specified
* @param[out] err error code in case of error (untouched if call succeeds)
* @return the dumped text allocated with malloc on success, NULL on memory allocation error
*/
void* papuga_RequestResult_totext( const papuga_RequestResult* self, papuga_StringEncoding enc, size_t* len, papuga_ErrorCode* err);

#ifdef __cplusplus
}
#endif

#endif
