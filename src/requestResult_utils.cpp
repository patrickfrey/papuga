/*
 * Copyright (c) 2017 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/// \brief Some helper functions for mapping a request result to XML/JSON
/// \file requestResult_utils.cpp
#include "requestResult_utils.hpp"
#include "papuga/typedefs.h"
#include "papuga/valueVariant.h"
#include <string>
#include <cstring>
#include <cstdlib>

using namespace papuga;

void* papuga::encodeRequestResultString( const std::string& out, papuga_StringEncoding enc, size_t* len, papuga_ErrorCode* err)
{
	if (enc == papuga_UTF8)
	{
		void* rt = (void*)std::malloc( out.size()+1);
		if (!rt)
		{
			*err = papuga_NoMemError;
			return NULL;
		}
		*len = out.size();
		std::memcpy( (char*)rt, out.c_str(), (*len)+1);
		return rt;
	}
	else
	{
		papuga_ValueVariant outvalue;
		papuga_init_ValueVariant_string( &outvalue, out.c_str(), out.size());
		size_t usize = papuga_StringEncoding_unit_size( enc);
		size_t rtbufsize = (out.size()+16) * usize;
		void* rtbuf = std::malloc( rtbufsize);
		if (!rtbuf)
		{
			*err = papuga_NoMemError;
			return NULL;
		}
		const void* rtstr = papuga_ValueVariant_tostring_enc( &outvalue, enc, rtbuf, rtbufsize, len, err);
		if (!rtstr)
		{
			std::free( rtbuf);
			return NULL;
		}
		void* rt = (void*)std::realloc( rtbuf, (*len + 1) * usize);
		if (!rt) rt = rtbuf;
		std::memset( (char*)rt + (*len) * usize, 0, usize); //... null termination
		return rt;
	}
}



