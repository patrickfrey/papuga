/*
 * Copyright (c) 2019 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
#ifndef _PAPUGA_URI_ENCODE_H_INCLUDED
#define _PAPUGA_URI_ENCODE_H_INCLUDED
/*
 * @brief Url encoder for exported links
 * @file "uriEncode.h"
 */
#include "papuga/typedefs.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
* @brief Encode an URI reference in a document link according to HTML5
* @param[out] destbuf pointer to buffer for the result (UTF-8)
* @param[in] destbufsize allocation size of 'destbuf' in bytes
* @param[out] destlen length of the result string
* @param[out] input pointer to input string (UTF-8)
* @param[out] inputlen length of the input string in bytes
* @param[out] err error code in case of error (untouched if call succeeds)
* @return a pointer to the buffer of the result in case of success or NULL if failed
*/
const char* papuga_uri_encode_Html5( char* destbuf, size_t destbufsize, size_t* destlen, const char* input, size_t inputlen, papuga_ErrorCode* err);

/*
* @brief Encode an URI reference in a document link according to RFC 3986
* @param[out] destbuf pointer to buffer for the result (UTF-8)
* @param[in] destbufsize allocation size of 'destbuf' in bytes
* @param[out] destlen length of the result string
* @param[out] input pointer to input string (UTF-8)
* @param[out] inputlen length of the input string in bytes
* @param[out] err error code in case of error (untouched if call succeeds)
* @return a pointer to the buffer of the result in case of success or NULL if failed
*/
const char* papuga_uri_encode_Rfc3986( char* destbuf, size_t destbufsize, size_t* destlen, const char* input, size_t inputlen, papuga_ErrorCode* err);

#ifdef __cplusplus
}
#endif
#endif
