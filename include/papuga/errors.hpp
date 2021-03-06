/*
* Copyright (c) 2017 Patrick P. Frey
*
* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/
#ifndef _PAPUGA_ERRORS_HPP_INCLUDED
#define _PAPUGA_ERRORS_HPP_INCLUDED
/// \brief Map of error codes to std::runtime_error exception
/// \file errors.hpp
#include "papuga/typedefs.h"
#include "papuga/errors.h"
#include <stdexcept>

namespace papuga {

std::runtime_error error_exception( const papuga_ErrorCode& ec, const char* where);
std::runtime_error runtime_error( const char* fmt, ...)
#ifdef __GNUC__
	__attribute__ ((format (printf, 1, 2)))
#endif
;

}//namespace
#endif

