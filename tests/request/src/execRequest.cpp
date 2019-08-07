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
		ctx->out << "out of memory logging method call" << std::endl;
	}
	va_end( arguments);
}

void logContentEvent( void* self, const char* title, int itemid, const papuga_ValueVariant* value)
{
	LoggerContext* ctx = (LoggerContext*)self;
	try
	{
		papuga_ErrorCode errcode = papuga_Ok;
		std::string valuestr;
		if (value) valuestr = papuga::ValueVariant_tostring( *value, errcode);
		if (valuestr.empty() && errcode != papuga_Ok)
		{
			ctx->out << "ERROR " << papuga_ErrorCode_tostring( errcode) << std::endl;
		}
		else
		{
			ctx->out << "EV " << (title?title:"") << " " << itemid << " '" << valuestr << "'" << std::endl;
		}
	}
	catch (const std::bad_alloc&)
	{
		ctx->out << "out of memory logging method call" << std::endl;
	}
}

static void reportRequestError( papuga_ErrorBuffer& errorbuf, const papuga_RequestError& errstruct, papuga_ContentType doctype, papuga_StringEncoding encoding, const std::string& doc)
{
	if (errstruct.classname)
	{
		if (errstruct.methodname)
		{
			if (errstruct.argcnt >= 0)
			{
				papuga_ErrorBuffer_reportError( &errorbuf, "%s in method %s::%s argument %d", papuga_ErrorCode_tostring( errstruct.errcode), errstruct.classname, errstruct.methodname, errstruct.argcnt);
			}
			else
			{
				papuga_ErrorBuffer_reportError( &errorbuf, "%s in method %s::%s", papuga_ErrorCode_tostring( errstruct.errcode), errstruct.classname, errstruct.methodname);
			}
		}
		else
		{
			if (errstruct.argcnt >= 0)
			{
				papuga_ErrorBuffer_reportError( &errorbuf, "%s in constructor of %s", papuga_ErrorCode_tostring( errstruct.errcode), errstruct.classname);
			}
			else
			{
				papuga_ErrorBuffer_reportError( &errorbuf, "%s in constructor of %s", papuga_ErrorCode_tostring( errstruct.errcode), errstruct.classname, errstruct.argcnt);
			}
		}
	}
	else
	{
		papuga_ErrorBuffer_reportError( &errorbuf, "%s", papuga_ErrorCode_tostring( errstruct.errcode));
	}
	if (errstruct.variable)
	{
		papuga_ErrorBuffer_appendMessage( &errorbuf, " accessing variable '%s'", errstruct.variable);
	}
	if (errstruct.itemid >= 0)
	{
		if (errstruct.structpath[0])
		{
			papuga_ErrorBuffer_appendMessage( &errorbuf, " resolving '%d' at '%s'", errstruct.itemid, errstruct.structpath);
		}
		else
		{
			papuga_ErrorBuffer_appendMessage( &errorbuf, " resolving item '%d'", errstruct.itemid);
		}
	}
	if (errstruct.errormsg[0])
	{
		papuga_ErrorBuffer_appendMessage( &errorbuf, " message: %s", errstruct.errormsg);
	}
	if (errstruct.scopestart > 0)
	{
		papuga_Allocator allocator;
		char allocator_mem[ 4096];
		papuga_ErrorCode errcode;

		papuga_init_Allocator( &allocator, allocator_mem, sizeof(allocator_mem));
		const char* locinfo = papuga_request_content_tostring( &allocator, doctype, encoding, doc.c_str(), doc.size(), errstruct.scopestart, 3/*max depth*/, &errcode);
		if (locinfo)
		{
			papuga_ErrorBuffer_appendMessage( &errorbuf, " error scope: %s", locinfo);
		}
	}
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
	char content_mem[ 4096];
	char errbuf_mem[ 4096];
	papuga_ErrorBuffer errorbuf;
	papuga_RequestError errstruct;
	papuga_Request* request = 0;
	papuga_RequestParser* parser = 0;
	papuga_RequestContext* ctx = 0;
	RequestVariable const* vi;
	LoggerContext logctx;
	char* resstr = 0;
	std::size_t reslen = 0;
	papuga_Allocator allocator;
	const char* rootname = 0;

	papuga_init_Allocator( &allocator, content_mem, sizeof(content_mem));
	papuga_RequestLogger logger = {&logctx, &logMethodCall, &logContentEvent};

	// Init output:
	papuga_init_ErrorBuffer( &errorbuf, errbuf_mem, sizeof(errbuf_mem));

	// Init locals:
	ctx = papuga_create_RequestContext( 0/*class name*/);
	if (!ctx) {errstruct.errcode = papuga_NoMemError; goto ERROR;}
	parser = papuga_create_RequestParser( &allocator, doctype, encoding, doc.c_str(), doc.size(), &errstruct.errcode);
	if (!parser) goto ERROR;
	request = papuga_create_Request( atm, &logger);
	if (!request) {errstruct.errcode = papuga_NoMemError; goto ERROR;}

	// Parse the request document and feed it to the request:
	if (!papuga_RequestParser_feed_request( parser, request, &errstruct.errcode))
	{
		char buf[ 2048];
		int pos = papuga_RequestParser_get_position( parser, buf, sizeof(buf));
		if (pos >= 0)
		{
			papuga_ErrorBuffer_reportError( &errorbuf, "error feeding request at position %d: %s, location: %s", pos, papuga_ErrorCode_tostring( errstruct.errcode), buf);
		}
		else
		{
			papuga_ErrorBuffer_reportError( &errorbuf, "error feeding request: %s, location: %s", papuga_ErrorCode_tostring( errstruct.errcode), buf);
		}
		resstr = papuga_ErrorBuffer_lastError(&errorbuf);
		reslen = std::strlen( resstr);
		resultblob.append( resstr, reslen);
		rt = false;
		goto RELEASE;
	}
	// Add variables to the request:
	vi = variables;
	if (vi) for (; vi->name; ++vi)
	{
		papuga_ValueVariant value;
		papuga_init_ValueVariant_charp( &value, vi->value);
		if (!papuga_RequestContext_define_variable( ctx, vi->name, &value))
		{
			errstruct.errcode = papuga_NoMemError;
			goto ERROR;
		}
	}
	// Execute the request and initialize the result:
	papuga_RequestResult* results;
	int nofResults;
	if (!papuga_RequestContext_execute_request( ctx, request, &allocator, &logger, &results, &nofResults, &errstruct))
	{
		goto ERROR;
	}
	{
		const papuga_StructInterfaceDescription* structdefs = papuga_Request_struct_descriptions( request);
		int ri=0;
		for (; ri != nofResults; ++ri)
		{
			papuga_ValueVariant resultval;
			papuga_init_ValueVariant_serialization( &resultval, &results[ ri].serialization);
			rootname = results[ri].name;

			// Map the result:
			switch (doctype)
			{
				case papuga_ContentType_XML:  resstr = (char*)papuga_ValueVariant_toxml( &resultval, &allocator, structdefs, encoding, rootname, "element", &reslen, &errcode); break;
				case papuga_ContentType_JSON: resstr = (char*)papuga_ValueVariant_tojson( &resultval, &allocator, structdefs, encoding, rootname, "element", &reslen, &errcode); break;
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
		}
	}
	goto RELEASE;
ERROR:
	rt = false;
	reportRequestError( errorbuf, errstruct, doctype, encoding, doc);
	resstr = papuga_ErrorBuffer_lastError(&errorbuf);
	reslen = std::strlen( resstr);
	resultblob.append( resstr, reslen);
RELEASE:
	if (ctx) papuga_destroy_RequestContext( ctx);
	if (parser) papuga_destroy_RequestParser( parser);
	if (request) papuga_destroy_Request( request);
	papuga_destroy_Allocator( &allocator);
	return rt;
}


