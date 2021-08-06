/*
 * Copyright (c) 2017 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/*
* \brief Class definition structure needed to execute papuga XML and JSON requests
* \file request.h
*/
#ifndef _PAPUGA_CLASSDEF_H_INCLUDED
#define _PAPUGA_CLASSDEF_H_INCLUDED
#include "papuga/typedefs.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
* @brief Class method function type
* @param[in] self pointer to data object
* @param[out] retval call result structure
* @param[in] argc number of arguments passed
* @param[in] argv pointer to array of arguments passed
*/
typedef bool (*papuga_ClassMethod)( void* self, papuga_CallResult* retval, size_t argc, const papuga_ValueVariant* argv);
/*
* @brief Class constructor function type
* @param[out] errbuf buffer for error messages
* @param[in] argc number of arguments passed
* @param[in] argv pointer to array of arguments passed
* @return pointer to data of object created
*/
typedef void* (*papuga_ClassConstructor)( papuga_ErrorBuffer* errbuf, size_t argc, const papuga_ValueVariant* argv);
/*
* @brief Class destructor function type
* @param[in] self pointer to data object to destroy
*/
typedef void (*papuga_ClassDestructor)( void* self);

/*
* @brief Structure defining a class
*/
typedef struct papuga_ClassDef
{
	const char* name;					/*< name of the class */
	const papuga_ClassConstructor constructor;		/*< constructor of the class */
	const papuga_ClassDestructor destructor;		/*< destructor of the class */
	const papuga_ClassMethod* methodtable;			/*< method table of the class */
	const char** methodnames;				/*< method names of the class, array parallel to 'methodtable' */
	int methodtablesize;					/*< number of functions defined in the method table and the array of method names of the class */
} papuga_ClassDef;

#define papuga_ClassDef_NULL	{0,0,0,0,0,0}

#ifdef __cplusplus
}
#endif
#endif

