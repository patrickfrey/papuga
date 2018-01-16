/*
 * Copyright (c) 2017 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/// \brief Function to execute a request
/// \file execRequest.cpp

#include "execRequest.h"
#include "papuga/requestParser.h"
#include "papuga/requestHandler.h"
#include "papuga/requestResult.h"
#include "papuga/errors.hpp"
#include "papuga/valueVariant.hpp"
#include <cstring>
#include <cstdlib>
#include <stdexcept>
#include <iostream>

#define PAPUGA_LOWLEVEL_DEBUG

extern "C" bool papuga_execute_request(
			const papuga_RequestAutomaton* atm,
			papuga_ContentType doctype,
			papuga_StringEncoding encoding,
			const char* docstr,
			size_t doclen,
			const RequestVariable* variables,
			papuga_ErrorCode* errcode,
			char** resstr,
			size_t* reslen)
{
	bool rt = true;
	char errbuf_mem[ 4096];
	papuga_ErrorBuffer errorbuf;
	int errorpos = -1;
	papuga_Request* request = 0;
	papuga_RequestParser* parser = 0;
	papuga_RequestContext ctx;
	papuga_RequestResult result;
	RequestVariable const* vi;

	// Init output:
	*errcode = papuga_Ok;
	*resstr = 0;
	*reslen = 0;

	// Init locals:
	papuga_init_RequestContext( &ctx);
	papuga_init_ErrorBuffer( &errorbuf, errbuf_mem, sizeof(errbuf_mem));
	parser = papuga_create_RequestParser( doctype, encoding, docstr, doclen, errcode);
	if (!parser) goto ERROR;
	request = papuga_create_Request( atm);
	if (!request) goto ERROR;

	// Parse the request document and feed it to the request:
	if (!papuga_RequestParser_feed_request( parser, request, errcode))
	{
		char buf[ 2048];
		int pos = papuga_RequestParser_get_position( parser, buf, sizeof(buf));
		papuga_ErrorBuffer_reportError( &errorbuf, "error at position %d: %s, feeding request, location: %s", pos, papuga_ErrorCode_tostring( *errcode), buf);
		goto ERROR;
	}
#ifdef PAPUGA_LOWLEVEL_DEBUG
	{
		papuga_Allocator allocator;
		papuga_init_Allocator( &allocator, 0, 0);
		const char* requestdump = papuga_Request_tostring( request, &allocator, errcode);
		if (!requestdump) throw papuga::error_exception( *errcode, "dumping request");
		std::cerr << "ITEMS REQUEST:\n" << requestdump << std::endl;
		papuga_destroy_Allocator( &allocator);
	}
#endif
	// Add variables to the request:
	vi = variables;
	if (vi) for (; vi->name; ++vi)
	{
		papuga_ValueVariant value;
		papuga_init_ValueVariant_charp( &value, vi->value);
		if (!papuga_RequestContext_add_variable( &ctx, vi->name, &value))
		{
			*errcode = papuga_NoMemError;
			goto ERROR;
		}
	}
	// Execute the request and initialize the result:
	if (!papuga_RequestContext_execute_request( &ctx, request, &errorbuf, &errorpos))
	{
		*errcode = papuga_HostObjectError;
		goto ERROR;
	}
	if (!papuga_set_RequestResult( &result, &ctx, request))
	{
		goto ERROR;
	}
#ifdef PAPUGA_LOWLEVEL_DEBUG
	{
		size_t dumplen = 0;
		char* dumpstr = papuga_RequestResult_tostring( &result, &dumplen);
		if (!dumpstr) throw papuga::error_exception( papuga_NoMemError, "dumping result");
		std::cerr << "RESULT DUMP:\n" << dumpstr << std::endl;
		std::free( dumpstr);
	}
#endif
	// Map the result:
	switch (doctype)
	{
		case papuga_ContentType_XML:  *resstr = (char*)papuga_RequestResult_toxml( &result, encoding, reslen, errcode); break;
		case papuga_ContentType_JSON: *resstr = (char*)papuga_RequestResult_tojson( &result, encoding, reslen, errcode); break;
		case papuga_ContentType_Unknown:
		default: break;
	}
	if (!*resstr) goto ERROR;
	goto RELEASE;
ERROR:
	rt = false;
	if (papuga_ErrorBuffer_lastError(&errorbuf))
	{
		if (errorpos >= 0)
		{
			// Evaluate more info about the location of the error, we append the scope of the document to the error message:
			char locinfobuf[ 4096];
			const char* locinfo = papuga_request_error_location( doctype, encoding, docstr, doclen, errorpos, locinfobuf, sizeof(locinfobuf));
			if (locinfo)
			{
				papuga_ErrorBuffer_appendMessage( &errorbuf, " (error scope: %s)", locinfo);
			}
		}
		char* errstr = papuga_ErrorBuffer_lastError(&errorbuf);
		size_t errlen = strlen( errstr);
		*resstr = (char*)std::malloc( errlen+1);
		if (*resstr)
		{
			std::memcpy( *resstr, errstr, errlen+1);
			*reslen = errlen;
		}
	}
RELEASE:
	papuga_destroy_RequestContext( &ctx);
	if (parser) papuga_destroy_RequestParser( parser);
	if (request) papuga_destroy_Request( request);
	return rt;
}


