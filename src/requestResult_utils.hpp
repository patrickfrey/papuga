/*
 * Copyright (c) 2017 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
#ifndef _PAPUGA_REQUEST_RESULT_UTILS_HPP_INCLUDED
#define _PAPUGA_REQUEST_RESULT_UTILS_HPP_INCLUDED
/// \brief Some helper functions for mapping a request result to XML/JSON
/// \file requestResult_utils.hpp
#include "papuga/typedefs.h"
#include <string>

namespace papuga {

void* encodeRequestResultString( const std::string& out, papuga_StringEncoding enc, size_t* len, papuga_ErrorCode* err);

}//namespace
#endif

