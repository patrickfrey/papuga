/*
 * Copyright (c) 2017 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/// \brief Function to execute a request
/// \file execRequest.cpp

#include "execRequest.hpp"
#include "papuga/requestParser.h"
#include "papuga/requestHandler.h"
#include "papuga/errors.hpp"
#include "papuga/valueVariant.h"
#include "papuga/valueVariant.hpp"
#include "papuga/serialization.hpp"
#include <cstring>
#include <cstdlib>
#include <cstdarg>
#include <stdexcept>
#include <iostream>
#include <sstream>

#undef PAPUGA_LOWLEVEL_DEBUG

struct LoggerContext
{
	std::ostringstream out;
};

static void logMethodCall( void* self, int nofItems, ...)
{
	LoggerContext* ctx = (LoggerContext*)self;
	va_list arguments;
	va_start( arguments, nofItems );
	try
	{
		papuga_ErrorCode errcode = papuga_Ok;
		std::size_t nofargs = 0;
		for( int ai = 0; ai < nofItems; ++ai)
		{
			papuga_RequestLogItem itype = (papuga_RequestLogItem)va_arg( arguments, int);
			if (ai) ctx->out << " ";
			typedef const char* charp;
			switch (itype)
			{
				case papuga_LogItemClassName:
					ctx->out << va_arg( arguments,charp);
					break;
				case papuga_LogItemMethodName:
					ctx->out << va_arg( arguments,charp);
					break;
				case papuga_LogItemResult:
				{
					papuga_ValueVariant* arg = va_arg( arguments, papuga_ValueVariant*);
					if (papuga_ValueVariant_isatomic( arg))
					{
						ctx->out << papuga::ValueVariant_tostring( *arg, errcode);
					}
					else
					{
						ctx->out << "<" << papuga_Type_name( arg->valuetype) << ">";
					}
					break;
				}
				case papuga_LogItemArgc:
					nofargs = va_arg( arguments,size_t);
					ctx->out << nofargs;
					break;
				case papuga_LogItemArgv:
				{
					papuga_ValueVariant* ar = va_arg( arguments, papuga_ValueVariant*);
					std::size_t ii=0, ie=nofargs;
					for (; ii!=ie; ++ii)
					{
						if (ii) ctx->out << " ";
						if (papuga_ValueVariant_isatomic( ar+ii))
						{
							ctx->out << papuga::ValueVariant_tostring( ar[ii], errcode);
						}
						else
						{
							ctx->out << "<" << papuga_Type_name( ar[ii].valuetype) << ">";
						}
					}
					break;
				}
				case papuga_LogItemMessage:
					ctx->out << va_arg( arguments,charp);
					break;
			}
		}
		if (errcode != papuga_Ok)
		{
			std::cerr << "error in logger: " << papuga_ErrorCode_tostring( errcode) << std::endl;
		}
		ctx->out << std::endl;
	}
	catch (const std::bad_alloc&)
	{
		std::cerr << "out of memory logging method call" << std::endl;
	}
	va_end( arguments);
}

bool papuga_execute_request(
			const papuga_RequestAutomaton* atm,
			papuga_ContentType doctype,
			papuga_StringEncoding encoding,
			const std::string& doc,
			const RequestVariable* variables,
			papuga_ErrorCode& errcode,
			std::string& resultblob,
			std::string& logout)
{
	bool rt = true;
	char errbuf_mem[ 4096];
	char content_mem[ 4096];
	papuga_ErrorBuffer errorbuf;
	int errorpos = -1;
	papuga_Request* request = 0;
	papuga_RequestParser* parser = 0;
	papuga_RequestContext* ctx = 0;
	papuga_Serialization* resultser;
	papuga_ValueVariant result;
	RequestVariable const* vi;
	LoggerContext logctx;
	char* resstr = 0;
	std::size_t reslen = 0;
	papuga_Allocator allocator;
	const char* rootname = 0;
	const papuga_StructInterfaceDescription* structdefs = 0;

	papuga_init_Allocator( &allocator, content_mem, sizeof(content_mem));
	papuga_RequestLogger logger = {&logctx, &logMethodCall};

	// Init output:
	errcode = papuga_Ok;

	// Init locals:
	ctx = papuga_create_RequestContext();
	if (!ctx) goto ERROR;
	papuga_init_ErrorBuffer( &errorbuf, errbuf_mem, sizeof(errbuf_mem));
	parser = papuga_create_RequestParser( &allocator, doctype, encoding, doc.c_str(), doc.size(), &errcode);
	if (!parser) goto ERROR;
	request = papuga_create_Request( atm);
	if (!request) goto ERROR;

	// Parse the request document and feed it to the request:
	if (!papuga_RequestParser_feed_request( parser, request, &errcode))
	{
		char buf[ 2048];
		int pos = papuga_RequestParser_get_position( parser, buf, sizeof(buf));
		papuga_ErrorBuffer_reportError( &errorbuf, "error at position %d: %s, feeding request, location: %s", pos, papuga_ErrorCode_tostring( errcode), buf);
		goto ERROR;
	}
#ifdef PAPUGA_LOWLEVEL_DEBUG
	{
		std::size_t requestdumplen;
		const char* requestdump = papuga_Request_tostring( request, &allocator, papuga_UTF8, 5/*maxdepth*/, &requestdumplen, &errcode);
		if (!requestdump) throw papuga::error_exception( errcode, "dumping request");
		std::cerr << "ITEMS REQUEST:\n" << requestdump << std::endl;
	}
#endif
	// Add variables to the request:
	vi = variables;
	if (vi) for (; vi->name; ++vi)
	{
		papuga_ValueVariant value;
		papuga_init_ValueVariant_charp( &value, vi->value);
		if (!papuga_RequestContext_add_variable( ctx, vi->name, &value))
		{
			errcode = papuga_NoMemError;
			goto ERROR;
		}
	}
	// Execute the request and initialize the result:
	if (!papuga_RequestContext_execute_request( ctx, request, &logger, &errorbuf, &errorpos))
	{
		errcode = papuga_HostObjectError;
		goto ERROR;
	}
	resultser = papuga_Allocator_alloc_Serialization( &allocator);
	if (!resultser)
	{
		errcode = papuga_NoMemError;
		goto ERROR;
	}
	papuga_init_ValueVariant_serialization( &result, resultser);
	if (!papuga_Serialization_serialize_request_result( resultser, ctx, request))
	{
		errcode = papuga_NoMemError;
		goto ERROR;
	}
#ifdef PAPUGA_LOWLEVEL_DEBUG
	{
		size_t dumplen = 0;
		char* dumpstr = papuga_ValueVariant_todump( &result, &dumplen);
		if (!dumpstr) throw papuga::error_exception( papuga_NoMemError, "dumping result");
		std::cerr << "RESULT DUMP:\n" << dumpstr << std::endl;
	}
#endif
	rootname = papuga_Request_resultname( request);
	structdefs = papuga_Request_struct_descriptions( request);

	// Map the result:
	switch (doctype)
	{
		case papuga_ContentType_XML:  resstr = (char*)papuga_ValueVariant_toxml( &result, &allocator, structdefs, encoding, rootname, "element", &reslen, &errcode); break;
		case papuga_ContentType_JSON: resstr = (char*)papuga_ValueVariant_tojson( &result, &allocator, structdefs, encoding, rootname, &reslen, &errcode); break;
		case papuga_ContentType_Unknown:
		default: break;
	}
	if (!resstr) goto ERROR;
	resultblob.append( resstr, reslen);
	try
	{
		logout.append( logctx.out.str());
	}
	catch (const std::bad_alloc&)
	{}
	goto RELEASE;
ERROR:
	rt = false;
	if (papuga_ErrorBuffer_lastError(&errorbuf))
	{
		if (errorpos >= 0)
		{
			// Evaluate more info about the location of the error, we append the scope of the document to the error message:
			const char* locinfo = papuga_request_content_tostring( &allocator, doctype, encoding, doc.c_str(), doc.size(), errorpos, 3/*max depth*/, &errcode);
			if (locinfo)
			{
				papuga_ErrorBuffer_appendMessage( &errorbuf, " (error scope: %s)", locinfo);
			}
		}
		char* errstr = papuga_ErrorBuffer_lastError(&errorbuf);
		size_t errlen = strlen( errstr);
		try
		{
			resultblob.append( errstr, errlen);
		}
		catch (const std::bad_alloc&)
		{}
	}
RELEASE:
	if (ctx) papuga_destroy_RequestContext( ctx);
	if (parser) papuga_destroy_RequestParser( parser);
	if (request) papuga_destroy_Request( request);
	papuga_destroy_Allocator( &allocator);
	return rt;
}


