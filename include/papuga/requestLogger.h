/*
 * Copyright (c) 2017 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/*
* \brief Structures for the context (state) of the execution of an XML/JSON request
* \file requestHandler.h
*/
#ifndef _PAPUGA_REQUEST_LOGGER_H_INCLUDED
#define _PAPUGA_REQUEST_LOGGER_H_INCLUDED
#include "papuga/typedefs.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
	papuga_LogItemClassName,		//< [char*] name of the class called
	papuga_LogItemMethodName,		//< [char*] name of the method called
	papuga_LogItemResult,			//< [papuga_ValueVariant*] the result of the method call
	papuga_LogItemArgc,			//< [size_t] number of arguments of the method called
	papuga_LogItemArgv,			//< [papuga_ValueVariant*] array of arguments of the method called
	papuga_LogItemMessage			//< [char*] message string to log
} papuga_RequestLogItem;

/*
 * @brief Request logging procedure type
 * @param[in] self logger context
 * @param[in] nofItems number of items logged
 * @param[in] ... pairs of papuga_LogItem, and a type depending on it describing an item logged
 * @example papuga_LoggerProcedure( 2, papuga_LogItemClassName, "context", papuga_LogItemMethodName, "run");
 */
typedef void (*papuga_RequestLoggerProcedure)( void* self, int nofItems, ...);

/*
 * @brief Logger structure with object and methods
 */
typedef struct papuga_RequestLogger
{
	void* self;
	papuga_RequestLoggerProcedure logMethodCall;
} papuga_RequestLogger;

#ifdef __cplusplus
}
#endif
#endif

