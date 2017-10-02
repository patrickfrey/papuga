/*
 * Copyright (c) 2017 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
#ifndef _PAPUGA_SERIALIZATION_HPP_INCLUDED
#define _PAPUGA_SERIALIZATION_HPP_INCLUDED
/// \brief Some functions on serialization using C++ features
/// \file serialization.hpp
#include "papuga/typedefs.h"
#include <string>

namespace papuga {

/// \brief Print serialization as human readable string
/// \param[in] value serialization to print to string
/// \param[out] error code returned in case of error
/// \return result string of empty string in case of error
/// \note does not throw
std::string Serialization_tostring( const papuga_Serialization& value, papuga_ErrorCode& errcode);

}//namespace
#endif


