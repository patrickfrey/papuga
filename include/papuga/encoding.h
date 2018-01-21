/*
 * Copyright (c) 2017 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
#ifndef _PAPUGA_ENCODING_H_INCLUDED
#define _PAPUGA_ENCODING_H_INCLUDED
/*
* @brief Parsing and mapping string encoding names
* @file encoding.h
*/
#include "papuga/typedefs.h"

#ifdef __cplusplus
extern "C" {
#endif

bool papuga_getStringEncodingFromName( papuga_StringEncoding* encoding, const char* name);
const char* papuga_stringEncodingName( papuga_StringEncoding encoding);

#ifdef __cplusplus
}
#endif
#endif

