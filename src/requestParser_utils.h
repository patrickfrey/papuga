/*
 * Copyright (c) 2017 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/// \brief Utilities for XML/JSON request parsers
/// \file requestParser_utils.h
#ifndef _PAPUGA_REQUEST_PARSER_UTILS_H_INCLUDED
#define _PAPUGA_REQUEST_PARSER_UTILS_H_INCLUDED
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

void fillErrorLocation( char* errlocbuf, size_t errlocbufsize, const char* source, size_t errpos, const char* marker);

#ifdef __cplusplus
}
#endif
#endif
