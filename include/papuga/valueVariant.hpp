/*
 * Copyright (c) 2017 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
#ifndef _PAPUGA_VALUE_VARIANT_HPP_INCLUDED
#define _PAPUGA_VALUE_VARIANT_HPP_INCLUDED
/// \brief Some functions on value variant using C++ features like STL
/// \file valueVariant.hpp
#include "papuga/typedefs.h"
#include <string>

namespace papuga {

/// \brief Convert variant value to string, if possible
/// \param[in] value variant value to convert
/// \param[out] error code returned in case of error
/// \return result string of empty string in case of error
/// \note does not throw
/// \remark only converting atomic values, not handling structures
std::string ValueVariant_tostring( const papuga_ValueVariant& value, papuga_ErrorCode& errcode);

/// \brief Append value variant to a string, if possible
/// \param[in,out] dest where to append result to
/// \param[in] value variant value to convert
/// \param[out] error code returned in case of error
/// \return result string of empty string in case of error
/// \note does not throw
/// \remark only converting atomic values, not handling structures
bool ValueVariant_append_string( std::string& dest, const papuga_ValueVariant& value, papuga_ErrorCode& errcode);

}//namespace
#endif


