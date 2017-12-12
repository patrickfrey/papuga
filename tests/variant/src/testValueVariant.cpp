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
#include <limits>

#define PAPUGA_LOWLEVEL_DEBUG
papuga_Allocator g_allocator;

static void initValue( papuga_ValueVariant& value, const int64_t& input)
{
	papuga_init_ValueVariant_int( &value, input);
}
static void initValue( papuga_ValueVariant& value, const double& input)
{
	papuga_init_ValueVariant_double( &value, input);
}
static void convString( int64_t& output, const papuga_ValueVariant& value)
{
	papuga_ErrorCode errcode = papuga_Ok;
	output = papuga_ValueVariant_toint( &value, &errcode);
	if (errcode != papuga_Ok) throw papuga::error_exception( errcode, "convert string to int");
}
static void convString( double& output, const papuga_ValueVariant& value)
{
	papuga_ErrorCode errcode = papuga_Ok;
	output = papuga_ValueVariant_todouble( &value, &errcode);
	if (errcode != papuga_Ok) throw papuga::error_exception( errcode, "convert string to double");
}
template <typename TYPE>
bool compareValue( const TYPE& a, const TYPE& b)
{
	return a == b;
}
template <>
bool compareValue<double>( const double& a, const double& b)
{
	double diff;
	if (a > 1000.0)
	{
		diff = 1.0 - a/b;
		if (diff < 0.0) diff = -diff;
	}
	else
	{
		diff = a > b ? (a - b) : (b - a);
	}
	return (diff < std::numeric_limits<double>::epsilon()*100);
}

static void checkVariantValue( const int64_t& vv, const papuga_ValueVariant& value)
{
	if (value.valuetype != papuga_TypeInt) throw std::runtime_error( "numeric type does not as expected (int)");
	if (!compareValue( vv, value.value.Int)) throw std::runtime_error( "numeric value does not as expected (int)");
}
static void checkVariantValue( const double& vv, const papuga_ValueVariant& value)
{
	if (value.valuetype != papuga_TypeDouble) throw std::runtime_error( "numeric type does not as expected (double)");
	if (!compareValue( vv, value.value.Double)) throw std::runtime_error( "numeric value does not as expected (double)");
}



template <typename INPUT>
bool testToString( int idx, const INPUT& input)
{
	bool rt = false;
	papuga_ValueVariant value;
	initValue( value, input);
	size_t len;
	papuga_ErrorCode err = papuga_Ok;
	const char* str = papuga_ValueVariant_tostring( &value, &g_allocator, &len, &err);
	if (!str) throw papuga::error_exception( err, "convert to string");
	std::cerr << "[" << idx << "] convert " << papuga_Type_name( value.valuetype) << " '" << input << "' to string '" << std::string( str, len) << "'";
	INPUT res;
	convString( res, value);
	papuga_ValueVariant strvalue;
	papuga_ValueVariant numvalue;
	papuga_init_ValueVariant_string( &strvalue, str, len);
	papuga_ValueVariant* numeric = papuga_ValueVariant_tonumeric( &strvalue, &numvalue, &err);
	if (!numeric) throw papuga::error_exception( err, "convert to numeric");
	checkVariantValue( input, *numeric);
	std::cerr << " back to '" << res << "'";
	if (compareValue( res, input))
	{
		std::cerr << " OK" << std::endl;
		rt = true;
	}
	else
	{
		std::cerr << " DIFF" << std::endl;
		rt = false;
	}
	return rt;
}

int main( int argc, const char* argv[])
{
	papuga_init_Allocator( &g_allocator, 0, 0);
	if (argc >= 2 && (std::strcmp( argv[1], "-h") == 0 || std::strcmp( argv[1], "--help") == 0))
	{
		std::cerr << "testValueVariant" << std::endl;
		return 0;
	}
	try
	{
		int testidx = 0;
		int errcnt = 0;
		double PI = std::atan(1)*4; 
		errcnt += (int)!testToString<int64_t>( ++testidx, 0);
		errcnt += (int)!testToString<int64_t>( ++testidx, 1);
		errcnt += (int)!testToString<int64_t>( ++testidx, 1U);
		errcnt += (int)!testToString<int64_t>( ++testidx, 1ULL << 16);
		errcnt += (int)!testToString<int64_t>( ++testidx, +12212);
		errcnt += (int)!testToString<int64_t>( ++testidx, -31312);
		errcnt += (int)!testToString<int64_t>( ++testidx, 1ULL << 32);
		errcnt += (int)!testToString<int64_t>( ++testidx, 1ULL << 63);
		errcnt += (int)!testToString<int64_t>( ++testidx, +99170709832174L);
		errcnt += (int)!testToString<int64_t>( ++testidx, -921391321311323L);
		errcnt += (int)!testToString<int64_t>( ++testidx, std::numeric_limits<unsigned int>::max());
		errcnt += (int)!testToString<int64_t>( ++testidx, std::numeric_limits<unsigned int>::min());
		errcnt += (int)!testToString<int64_t>( ++testidx, std::numeric_limits<int>::max());
		errcnt += (int)!testToString<int64_t>( ++testidx, std::numeric_limits<int>::min());
		errcnt += (int)!testToString<int64_t>( ++testidx, std::numeric_limits<int64_t>::min());
		errcnt += (int)!testToString<int64_t>( ++testidx, std::numeric_limits<int64_t>::max());
		errcnt += (int)!testToString<double>( ++testidx, PI);
		errcnt += (int)!testToString<double>( ++testidx, std::numeric_limits<float>::min());
		errcnt += (int)!testToString<double>( ++testidx, std::numeric_limits<float>::max());
		if (errcnt)
		{
			char msgbuf[ 256];
			std::snprintf( msgbuf, sizeof( msgbuf), "%d out of %d tests failed", errcnt, testidx);
			throw std::runtime_error( msgbuf);
		}
		std::cerr << std::endl << "OK done " << testidx << " tests" << std::endl;
		papuga_destroy_Allocator( &g_allocator);
		return 0;
	}
	catch (const std::runtime_error& err)
	{
		std::cerr << std::endl << "ERROR " << err.what() << std::endl;
		return -1;
	}
	catch (const std::bad_alloc& )
	{
		std::cerr << std::endl << "ERROR out of memory" << std::endl;
		return -2;
	}
	catch (...)
	{
		std::cerr << std::endl << "EXCEPTION uncaught" << std::endl;
		return -3;
	}
}

