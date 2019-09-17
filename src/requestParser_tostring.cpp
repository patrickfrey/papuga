/*
 * Copyright (c) 2017 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/// \brief Function to print some info about the location scope of an error in a request source
/// \file requestParser_location.cpp

#include "papuga/requestParser.h"
#include "papuga/errors.hpp"
#include "papuga/valueVariant.hpp"
#include "papuga/allocator.h"
#include <cstring>
#include <cstdlib>
#include <stdexcept>
#include <iostream>

#define LOCATION_INFO_VALUE_MAX_LENGTH 48

#define B10000000 0x80
#define B11000000 0xC0

static bool isUTF8MidChar( unsigned char ch)
{
	return (ch >= B10000000 && (ch & B11000000) == B10000000);
}

extern "C" const char* papuga_request_content_tostring( papuga_Allocator* allocator, papuga_ContentType doctype, papuga_StringEncoding encoding, const char* docstr, size_t doclen, int scopestart, int maxdepth, int* reslength, papuga_ErrorCode* errcode)
{
	char* rt = NULL;
	papuga_RequestParser* parser = 0;
	std::string locinfo;
	int taglevel = 0;
	papuga_RequestElementType elemtype;
	int pi = 1;
	papuga_ValueVariant elemval;

	try
	{
		parser = papuga_create_RequestParser( allocator, doctype, encoding, docstr, doclen, errcode);
		if (!parser)
		{
			if (reslength) *reslength = 0;
			return NULL;
		}
		while (papuga_RequestElementType_None != (elemtype = papuga_RequestParser_next( parser, &elemval)) && pi < scopestart) ++pi;
		// .... skip to the start position

		// Report location scope until end of next Element
		while (elemtype != papuga_RequestElementType_None)
		{
			switch (elemtype)
			{
				case papuga_RequestElementType_None:
					break;
				case papuga_RequestElementType_Open:
					++taglevel;
					if (taglevel <= maxdepth)
					{
						locinfo.append( " ");
						if (!papuga::ValueVariant_append_string( locinfo, elemval, *errcode))
						{
							locinfo.append( "??");
						}
						locinfo.append( ": {");
					}
					elemtype = papuga_RequestParser_next( parser, &elemval);
					break;
				case papuga_RequestElementType_Close:
					--taglevel;
					if (taglevel == maxdepth)
					{
						locinfo.append( " ... }");
					}
					else
					{
						locinfo.append( " }");
					}
					if (taglevel == 0)
					{
						elemtype = papuga_RequestElementType_None;
						break;
					}
					elemtype = papuga_RequestParser_next( parser, &elemval);
					break;
				case papuga_RequestElementType_AttributeName:
					if (taglevel <= maxdepth)
					{
						locinfo.append( " -");
						if (!papuga::ValueVariant_append_string( locinfo, elemval, *errcode))
						{
							locinfo.append( "??");
						}
						locinfo.append( ":");
					}
					elemtype = papuga_RequestParser_next( parser, &elemval);
					break;
				case papuga_RequestElementType_AttributeValue:
				case papuga_RequestElementType_Value:
					if (taglevel <= maxdepth)
					{
						if (elemval.valuetype == papuga_TypeString)
						{
							locinfo.append( elemtype == papuga_RequestElementType_Value ? " \"":"\"");
							if (elemval.length > LOCATION_INFO_VALUE_MAX_LENGTH)
							{
								elemval.length = LOCATION_INFO_VALUE_MAX_LENGTH;
								if (elemval.encoding == papuga_UTF8)
								{
									while (elemval.length > 0 && isUTF8MidChar( elemval.value.string[elemval.length-1]))
									{
										--elemval.length;
									}
								}
								if (!papuga::ValueVariant_append_string( locinfo, elemval, *errcode))
								{
									locinfo.append( "??");
								}
								else
								{
									locinfo.append( "...");
								}
							}
							else if (!papuga::ValueVariant_append_string( locinfo, elemval, *errcode))
							{
								locinfo.append( "??");
							}
							locinfo.append( "\"");
						}
						else if (!papuga::ValueVariant_append_string( locinfo, elemval, *errcode))
						{
							locinfo.append( elemtype == papuga_RequestElementType_Value ? " ??":"??");
						}
					}
					if (taglevel == 0)
					{
						elemtype = papuga_RequestElementType_None;
						break;
					}
					elemtype = papuga_RequestParser_next( parser, &elemval);
					break;
			}
		}
	}
	catch (const std::bad_alloc&)
	{
		if (reslength) *reslength = 0;
		*errcode = papuga_NoMemError;
		goto EXIT;
	}
	catch (...)
	{
		if (reslength) *reslength = 0;
		*errcode = papuga_TypeError;
		goto EXIT;
	}
	rt = (char*)papuga_Allocator_alloc( allocator, locinfo.size()+1, 1);
	if (!rt)
	{
		if (reslength) *reslength = 0;
		*errcode = papuga_NoMemError;
		goto EXIT;
	}
	std::memcpy( rt, locinfo.c_str(), locinfo.size());
	if (reslength) *reslength = locinfo.size();
	rt[ locinfo.size()] = 0;
EXIT:
	if (parser) papuga_destroy_RequestParser( parser);
	return rt;
}


