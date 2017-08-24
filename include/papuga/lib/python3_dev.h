/*
* Copyright (c) 2017 Patrick P. Frey
*
* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/
/*
* @brief Library interface for PYTHON (v3) bindings built by papuga
* @file papuga/lib/python3_dev.h
*/
#ifndef _PAPUGA_PYTHON_DEV_LIB_H_INCLUDED
#define _PAPUGA_PYTHON_DEV_LIB_H_INCLUDED
#include "papuga/typedefs.h"
#include "papuga/allocator.h"
#include <Python.h>

#define papuga_PYTHON_MAX_NOF_ARGUMENTS 64

/*
* @brief Call arguments structure
*/
typedef struct papuga_python_CallArgs
{
	int erridx;			/*< index of argument (starting with 1), that caused the error or 0 */
	papuga_ErrorCode errcode;	/*< papuga error code */
	size_t argc;			/*< number of arguments passed to call */
	void* self;			/*< pointer to host-object of the method called */
	papuga_Allocator allocator;	/*< allocator used for deep copies */
	papuga_ValueVariant argv[ papuga_PYTHON_MAX_NOF_ARGUMENTS];	/* argument list */
	int allocbuf[ 1024];		/*< static buffer for allocator to start with (to avoid early malloc) */
} papuga_python_CallArgs;

/*
* @brief Map of class identifiers to Python object type structures (for creating objects with classid as only info)
*/
typedef struct papuga_python_ClassEntryMap
{
	size_t size;				/*< size of map in elements */
	PyTypeObject** ar;			/*< pointers to Python object type structures */
} papuga_python_ClassEntryMap;

/*
* @brief Initialize papuga globals for Python3
* @remark this function has to be called before using any of the functions of this module
*/
void papuga_python_init();

/*
* @brief Create a non initialized (NULL) host object in the Python context
* @param[in] ce class description (method table for Python)
*/
PyObject* papuga_python_create_object( papuga_HostObject* hostobjref);

/*
* @brief Fills a structure with the arguments passed in a Python binding function/method call
* @param[in] argc number of function arguments
* @param[out] arg argument structure initialized
*/
bool papuga_python_init_CallArgs( papuga_python_CallArgs* argstruct, PyObject* pyargs);

/*
* @brief Frees the arguments of a papuga call (to call after the call)
* @param[in] arg argument structure freed
*/
void papuga_python_destroy_CallArgs( papuga_python_CallArgs* argstruct);

/*
* @brief Transfers the call result of a binding function into the Python context, freeing the call result structure
* @param[in,out] retval return values to move to the Python context
* @param[in] cemap map of class ids to zend class descriptions
* @param[in,out] errbuf buffer for error messages
* @return Python3 return value without reference counter increment on success, NULL on failure or if no values returned
*/
PyObject* papuga_python_move_CallResult( papuga_CallResult* retval, const papuga_python_ClassEntryMap* cemap, papuga_ErrorBuffer* errbuf);

/*
* @brief Report an error to Python
* @param[in] msg error message format string
* @param[in] ... format string arguments
*/
void papuga_python_error( const char* msg, ...)
#ifdef __GNUC__
	__attribute__ ((format (printf, 1, 2)))
#endif
;
#endif

