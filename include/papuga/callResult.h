/*
 * Copyright (c) 2017 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
#ifndef _PAPUGA_CALL_RESULT_H_INCLUDED
#define _PAPUGA_CALL_RESULT_H_INCLUDED
/*
* @brief Representation of a result of a call to papuga language bindings
* @file callResult.h
*/
#include "papuga/typedefs.h"

#ifdef __cplusplus
extern "C" {
#endif
/*
* @brief Constructor of a CallResult
* @param[out] self pointer to structure initialized by constructor
* @param[in] allocbuf pointer to local memory buffer to use for first memory allocations
* @param[in] allocbufsize allocation size of allocbuf in bytes
* @param[in] errbuf pointer to local memory buffer to use for error messages
* @param[in] errbufsize allocation size of errbuf in bytes
* @return true on success, false if too many return values defined
*/
void papuga_init_CallResult( papuga_CallResult* self, void* allocbuf, size_t allocbufsize, char* errbuf, size_t errbufsize);

/*
* @brief Destructor of a CallResult
* @param[in] self pointer to structure to free
* @return true on success, false if too many return values defined
*/
void papuga_destroy_CallResult( papuga_CallResult* self);

/*
* @brief Add a return value value of CallResult as int
* @param[in,out] self pointer to structure
* @param[in] val value to set as return value
* @return true on success, false if too many return values defined
*/
bool papuga_add_CallResult_int( papuga_CallResult* self, papuga_Int val);

/*
* @brief Add a return value of CallResult as double precision floating point value
* @param[in,out] self pointer to structure
* @param[in] val value to set as return value
* @return true on success, false if too many return values defined
*/
bool papuga_add_CallResult_double( papuga_CallResult* self, double val);

/*
* @brief Add a return value of CallResult as boolean value
* @param[in,out] self pointer to structure
* @param[in] val value to set as return value
* @return true on success, false if too many return values defined
*/
bool papuga_add_CallResult_bool( papuga_CallResult* self, bool val);

/*
* @brief Add a return value of CallResult as string reference
* @param[in,out] self pointer to structure
* @param[in] val value to set as return value
* @param[in] valsize size of value in bytes
* @return true on success, false if too many return values defined
*/
bool papuga_add_CallResult_string( papuga_CallResult* self, const char* val, size_t valsize);

/*
* @brief Add a return value of CallResult as string copy
* @param[in,out] self pointer to structure
* @param[in] val value to set as return value
* @param[in] valsize size of value in bytes
* @return true on success, false on memory allocation error or too many return values defined
*/
bool papuga_add_CallResult_string_copy( papuga_CallResult* self, const char* val, size_t valsize);

/*
* @brief Add a return value of CallResult as binary blob reference
* @param[in,out] self pointer to structure
* @param[in] val value to set as return value
* @param[in] valsize size of value in bytes
* @return true on success, false if too many return values defined
*/
bool papuga_add_CallResult_blob( papuga_CallResult* self, const void* val, size_t valsize);

/*
* @brief Add a return value of CallResult as string copy
* @param[in,out] self pointer to structure
* @param[in] val value to set as return value
* @param[in] valsize size of value in bytes
* @return true on success, false on memory allocation error or too many return values defined
*/
bool papuga_add_CallResult_blob_copy( papuga_CallResult* self, const void* val, size_t valsize);

/*
* @brief Define value of CallResult as C-string reference
* @param[in,out] self pointer to structure
* @param[in] val value to set as return value
* @return true on success, false if too many return values defined
*/
bool papuga_add_CallResult_charp( papuga_CallResult* self, const char* val);

/*
* @brief Define value of CallResult as C-string copy
* @param[in,out] self pointer to structure
* @param[in] val value to set as return value
* @return true on success, false on memory allocation error
*/
bool papuga_add_CallResult_charp_copy( papuga_CallResult* self, const char* val);

/*
* @brief Define value of CallResult as string reference with character set encoding specified
* @param[in,out] self pointer to structure
* @param[in] enc character set encoding of the string
* @param[in] val value to set as return value
* @param[in] valsize size of value in character units (the number if bytes is depending on the encoding)
* @return true on success, false if too many return values defined
*/
bool papuga_add_CallResult_string_enc( papuga_CallResult* self, papuga_StringEncoding enc, const void* val, size_t valsize);

/*
* @brief Define value of CallResult as host object with ownership
* @param[in,out] self pointer to structure
* @param[in] classid class identifier of the host object
* @param[in] data pointer to host object
* @param[in] destroy delete function of the host object
* @return true on success, false on memory allocation error or too many return values defined
*/
bool papuga_add_CallResult_hostobject( papuga_CallResult* self, int classid, void* data, papuga_Deleter destroy);

/*
* @brief Define value of CallResult as serialization defined in the call result structure
* @param[in,out] self pointer to structure
* @return true on success, false on memory allocation error
*/
bool papuga_add_CallResult_serialization( papuga_CallResult* self);

/*
* @brief Define value of CallResult as iterator
* @param[in,out] self pointer to structure
* @param[in] data pointer to host object
* @param[in] destroy delete function of the host object
* @param[in] getNext function to fetch next element of iterator
* @return true on success, false on memory allocation error or too many return values defined
*/
bool papuga_add_CallResult_iterator( papuga_CallResult* self, void* data, papuga_Deleter destroy, papuga_GetNext getNext);

/*
* @brief Define value of CallResult as iterator
* @param[in,out] self pointer to structure
* @param[in] value pointer to value to add
* @return true on success, false on memory allocation error or too many return values defined
*/
bool papuga_add_CallResult_value( papuga_CallResult* self, const papuga_ValueVariant* value);

/*
* @brief Report an error of the call
* @param[in,out] self pointer to structure
* @param[in] msg format string of the error message
* @param[in] ... arguments of the error message
*/
void papuga_CallResult_reportError( papuga_CallResult* self, const char* msg, ...);

/*
* @brief Test if the call result has an error reported
* @param[in,out] self_ pointer to structure
* @return true, if yes
*/
#define papuga_CallResult_hasError( self_)		((self_)->errorbuf.ptr[0] != 0)

/*
* @brief Get the error message of the call result
* @param[in,out] self_ pointer to structure
* @return the pointer to the message string
*/
#define papuga_CallResult_lastError( self_)		((self_)->errorbuf.ptr)

#ifdef __cplusplus
}
#endif
#endif



