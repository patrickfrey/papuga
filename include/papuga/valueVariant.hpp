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
#include "papuga/interfaceDescription.h"
#include <string>

namespace papuga {

/// \brief Convert an atomic variant value to string 
/// \param[in] value variant value to convert
/// \param[out] error code returned in case of error
/// \return result string of empty string in case of error
/// \note does not throw
/// \remark only converting atomic values, not handling structures
std::string ValueVariant_tostring( const papuga_ValueVariant& value, papuga_ErrorCode& errcode);

/// \brief Dump a value variant value including serializations to string
/// \param[in] value variant value to convert
/// \param[in] structdefs structure descriptions
/// \param[in] deterministic true, for deterministic ouput
/// \param[out] errcode code returned in case of error
/// \return result string of empty string in case of error
/// \note does not throw
std::string ValueVariant_todump( const papuga_ValueVariant& value, const papuga_StructInterfaceDescription* structdefs, bool deterministic, papuga_ErrorCode& errcode);

/// \brief Append value variant to a string, if possible
/// \param[in,out] dest where to append result to
/// \param[in] value variant value to convert
/// \param[out] errcode code returned in case of error
/// \return result string of empty string in case of error
/// \note does not throw
/// \remark only converting atomic values, not handling structures
bool ValueVariant_append_string( std::string& dest, const papuga_ValueVariant& value, papuga_ErrorCode& errcode);

}//namespace
#endif


