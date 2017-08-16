/*
* Copyright (c) 2017 Patrick P. Frey
*
* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/
/// \brief Library interface for libpapuga_python3_gen for generating PHP (v7) bindings
/// \file libpapuga_python3_gen.cpp
#include "papuga/lib/python3_gen.hpp"
#include "private/dll_tags.h"
#include "private/gen_utils.hpp"
#include "printPython3Doc.hpp"
#include "printPython3Mod.hpp"
#include "fmt/format.h"
#include <string>
#include <cstdio>
#include <cstdarg>
#include <stdexcept>
#include <sstream>

using namespace papuga;

DLL_PUBLIC bool papuga::generatePython3Source(
	std::ostream& out,
	std::ostream& err,
	const std::string& what,
	const std::multimap<std::string,std::string>& args,
	const papuga_InterfaceDescription& descr)
{
	try
	{
		if (what == "module")
		{
			printPython3ModSource( out, descr, getGeneratorArguments( args, "include"));
		}
		else if (what == "setup")
		{
			std::string c_includedir( getGeneratorArgument( args, "incdir", 0));
			std::string c_libdir( getGeneratorArgument( args, "libdir", 0));
			printPython3ModSetup( out, descr, c_includedir, c_libdir);
		}
		else if (what == "doc")
		{
			printPython3Doc( out, descr);
		}
		else
		{
			char buf[ 256];
			std::snprintf( buf, sizeof(buf), "unknown item '%s'", what.c_str());
			throw std::runtime_error( buf);
		}
		return true;
	}
	catch (const fmt::FormatError& ex)
	{
		err << "format error generating Python (v3) binding source '" << what << "': " << ex.what() << std::endl;
	}
	catch (const std::exception& ex)
	{
		err << "error generating Python (v3) binding source '" << what << "': " << ex.what() << std::endl;
	}
	return false;
}


