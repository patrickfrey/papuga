/*
 * Copyright (c) 2017 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
#ifndef _PAPUGA_CALL_ARGS_H_INCLUDED
#define _PAPUGA_CALL_ARGS_H_INCLUDED
/*
* @brief Representation of the arguments of a call to papuga language bindings
* @file callResult.h
*/
#include "papuga/typedefs.h"

#ifdef __cplusplus
extern "C" {
#endif
/*
* @brief Constructor of a CallArgs
* @param[out] self pointer to structure initialized by constructor
*/
void papuga_init_CallArgs( papuga_CallArgs* self, char* membuf, size_t membufsize);

/*
* @brief Destructor of a CallArgs
* @param[in] self pointer to structure to free
*/
void papuga_destroy_CallArgs( papuga_CallArgs* self);

#ifdef __cplusplus
}
#endif
#endif



