/*
* Copyright (c) 2017 Patrick P. Frey
*
* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/
/// \brief Library interface for libpapuga_python3_gen for generating Python (v3) language bindings
/// \file papuga/lib/python3_gen.hpp
#ifndef _PAPUGA_PYTHON3_GEN_LIB_HPP_INCLUDED
#define _PAPUGA_PYTHON3_GEN_LIB_HPP_INCLUDED
#include "papuga/interfaceDescription.h"
#include <string>
#include <map>
#include <iostream>

namespace papuga {

/// \brief Generate the source and documentation of the Python (v3) language bindings for an interface described
/// \param[out] out where to print the generated item
/// \param[err] err stream to report errors and warnings
/// \param[in] what name of item to generate
/// \param[in] args arguments passed to the generator
/// \param[in] descr interface description
/// \return true on success, false else
bool generatePython3Source(
	std::ostream& out,
	std::ostream& err,
	const std::string& what,
	const std::multimap<std::string,std::string>& args,
	const papuga_InterfaceDescription& descr);

}//namespace
#endif

