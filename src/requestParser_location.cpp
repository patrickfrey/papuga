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
#include <cstring>
#include <cstdlib>
#include <stdexcept>
#include <iostream>

#define LOCATION_INFO_VALUE_MAX_LENGTH 48

extern "C" const char* papuga_request_content_tostring( papuga_ContentType doctype, papuga_StringEncoding encoding, const char* docstr, size_t doclen, int scopestart, int maxdepth, char* buf, size_t bufsize)
{
	papuga_RequestParser* parser = 0;
	std::string locinfo;
	int taglevel = 0;
	papuga_RequestElementType elemtype;
	int pi = 1;
	papuga_ValueVariant elemval;
	papuga_ErrorCode errcode = papuga_Ok;

	try
	{
		parser = papuga_create_RequestParser( doctype, encoding, docstr, doclen, &errcode);
		if (!parser) return NULL;

		while (papuga_RequestElementType_None != (elemtype = papuga_RequestParser_next( parser, &elemval)) && pi < scopestart) ++pi;
		// .... skip to the start position

		// Report location scope until end of next Element
		while (bufsize > locinfo.size() && elemtype != papuga_RequestElementType_None)
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
						if (!papuga::ValueVariant_append_string( locinfo, elemval, errcode))
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
						locinfo.append( " .");
						elemtype = papuga_RequestElementType_None;
						break;
					}
					elemtype = papuga_RequestParser_next( parser, &elemval);
					break;
				case papuga_RequestElementType_AttributeName:
					if (taglevel <= maxdepth)
					{
						locinfo.append( " -");
						if (!papuga::ValueVariant_append_string( locinfo, elemval, errcode))
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
								if (!papuga::ValueVariant_append_string( locinfo, elemval, errcode))
								{
									locinfo.append( "??");
								}
								else
								{
									locinfo.append( "...");
								}
							}
							else if (!papuga::ValueVariant_append_string( locinfo, elemval, errcode))
							{
								locinfo.append( "??");
							}
							locinfo.append( "\"");
						}
						else if (!papuga::ValueVariant_append_string( locinfo, elemval, errcode))
						{
							locinfo.append( elemtype == papuga_RequestElementType_Value ? " ??":"??");
						}
					}
					if (taglevel == 0)
					{
						locinfo.append( " .");
						elemtype = papuga_RequestElementType_None;
						break;
					}
					elemtype = papuga_RequestParser_next( parser, &elemval);
					break;
			}
		}
	}
	catch (...)
	{
		if (bufsize) buf[0] = 0;
		return NULL;
	}
	if (parser) papuga_destroy_RequestParser( parser);
	if (!bufsize) return NULL;
	if (locinfo.size() <= bufsize-1)
	{
		bufsize = locinfo.size();
	}
	std::memcpy( buf, locinfo.c_str(), bufsize);
	buf[ bufsize-1] = 0;
	return buf;
}


