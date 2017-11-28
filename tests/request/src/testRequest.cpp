/*
 * Copyright (c) 2017 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
#include "papuga.hpp"
#include <iostream>
#include <stdexcept>
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <cmath>
#include <vector>
#include <string>

#undef PAPUGA_LOWLEVEL_DEBUG

int main( int argc, const char* argv[])
{
	if (argc <= 1 || std::strcmp( argv[1], "-h") == 0 || std::strcmp( argv[1], "--help") == 0)
	{
		std::cerr << "testRequest <testno>" << std::endl
				<< "\t<testno>     :Index of test to execute (default all)" << std::endl;
		return 0;
	}
	try
	{
		std::cerr << "OK" << std::endl;
		return 0;
	}
	catch (const std::runtime_error& err)
	{
		std::cerr << "ERROR " << err.what() << std::endl;
		return -1;
	}
	catch (const std::bad_alloc& )
	{
		std::cerr << "ERROR out of memory" << std::endl;
		return -2;
	}
	catch (...)
	{
		std::cerr << "EXCEPTION uncaught" << std::endl;
		return -3;
	}
}

