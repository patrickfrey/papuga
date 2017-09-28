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

/*
* @brief Map of class identifiers to Python object type structures (for creating objects with classid as only info)
*/
typedef struct papuga_python_ClassEntryMap
{
	size_t size;				/*< size of map in elements */
	PyTypeObject** ar;			/*< pointers to Python object type structures */
} papuga_python_ClassEntryMap;

/*
* @brief Papuga class object frame
*/
typedef struct papuga_python_ClassObject
{
	PyObject_HEAD				/*< Python object header */
	void* self;				/*< pointer to host object representation */
	int classid;				/*< class identifier of the object */
	papuga_Deleter destroy;			/*< destructor function */
	int checksum;				/*< checksum for verification */
} papuga_python_ClassObject;

/*
* @brief Initialize papuga globals for Python3
* @return 0 on success, -1 on error
* @remark this function has to be called before using any of the functions of this module
*/
int papuga_python_init(void);

/*
* @brief Initialize an allocated host object in the Python context
* @param[in] selfobj pointer to the allocated and zeroed python object
* @param[in] self pointer to host object data
* @param[in] classid class identifier of the object
* @param[in] destroy destructor function of the host object data ('self')
* @param[in] self pointer to host object representation
*/
void papuga_python_init_object( PyObject* selfobj, void* self, int classid, papuga_Deleter destroy);

/*
* @brief Create a host object representation in the Python context
* @param[in] self pointer to host object data (pass with ownership, destroyed on error)
* @param[in] classid class identifier of the object
* @param[in] destroy destructor function of the host object data ('self')
* @param[in] cemap map of class ids to python class descriptions
* @param[in,out] errbuf where to report errors
* @return object without reference increment, NULL on error
*/
PyObject* papuga_python_create_object( void* self, int classid, papuga_Deleter destroy, const papuga_python_ClassEntryMap* cemap, papuga_ErrorCode* errcode);

/*
* @brief Destroy a host object representation in the Python context created with papuga_python_create_object
* @param[in] self pointer to host object data
*/
void papuga_python_destroy_object( PyObject* selfobj);

/*
* @brief Fills a structure with the arguments passed in a Python binding function/method call
* @param[out] argstruct argument structure initialized
* @param[in] args positional arguments or NULL
* @param[in] kwargnames NULL terminated list of argument names in their order of definition
* @param[in] cemap map of class ids to python class descriptions
* @return true on success, false on error, see error code in argstruct to determine the error
*/
bool papuga_python_init_CallArgs( papuga_CallArgs* argstruct, PyObject* args, const char** kwargnames, const papuga_python_ClassEntryMap* cemap);

/*
* @brief Transfers the call result of a binding function into the Python context, freeing the call result structure
* @param[in,out] retval return values to move to the Python context
* @param[in] cemap map of class ids to python class descriptions
* @param[out] errcode error code
* @return Python3 return value with reference counter increment on success, NULL on failure or if no values returned
*/
PyObject* papuga_python_move_CallResult( papuga_CallResult* retval, const papuga_python_ClassEntryMap* cemap, papuga_ErrorCode* errcode);

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

