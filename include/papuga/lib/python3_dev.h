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

#ifdef __cplusplus
extern "C" {
#endif

/*
* @brief Maps for Python object type structures (for creating objects with id as only info)
*/
typedef struct papuga_python_ClassEntryMap
{
	size_t hoarsize;				/*< size of 'hostobjar' in elements */
	PyTypeObject** hoar;				/*< pointers to Python object type structures for host object references */
	size_t soarsize;				/*< size of 'soar' in elements */
	PyTypeObject** soar;				/*< pointers to Python object type structures for structure objects (return value structures) */
} papuga_python_ClassEntryMap;

/*
* @brief Papuga class object
*/
typedef struct papuga_python_ClassObject
{
	PyObject_HEAD					/*< Python object header */
	papuga_HostObject* obj;				/*< host object reference */
	int checksum;					/*< checksum for verification */
} papuga_python_ClassObject;

/*
* @brief Papuga struct object element
*/
typedef struct papuga_python_StructObjectElement
{
	PyObject* pyobj;				/*< pointer to member object */
} papuga_python_StructObjectElement;

/*
* @brief Papuga struct object
*/
typedef struct papuga_python_StructObject {
	PyObject_HEAD
	int structid;					/*< object identifier of the object */
	int checksum;
	int elemarsize;
	papuga_python_StructObjectElement elemar[1];
} papuga_python_StructObject;

/*
* @brief Get the offset of the member definition in the structure
*/
#define papuga_python_StructObjectElement_offset(idx)	((int)(uintptr_t)&(((papuga_python_StructObject*)0)->elemar[(idx)]))


/*
* @brief Initialize papuga globals for Python3
* @return 0 on success, -1 on error
* @remark this function has to be called before using any of the functions of this module
*/
int papuga_python_init(void);

/*
* @brief Initialize an allocated host object in the Python context
* @param[in] selfobj pointer to the allocated and zeroed python object
* @param[in] hobj host object reference
*/
void papuga_python_init_object( PyObject* selfobj, papuga_HostObject* hobj);

/*
* @brief Create a host object representation in the Python context
* @param[in] hobj host object reference
* @param[in] cemap map of class ids to python class descriptions
* @param[in,out] errcode error code in case of NULL returned
* @return object without reference increment, NULL on error
*/
PyObject* papuga_python_create_object( papuga_HostObject* hobj, const papuga_python_ClassEntryMap* cemap, papuga_ErrorCode* errcode);

/*
* @brief Destroy a host object representation in the Python context created with papuga_python_create_object
* @param[in] self pointer to host object data
*/
void papuga_python_destroy_object( PyObject* selfobj);

/*
* @brief Destroy a structure (return value structure) representation in the Python context created with papuga_python_create_struct
* @param[in] self pointer to host object data
*/
void papuga_python_destroy_struct( PyObject* selfobj);

/*
* @brief Fills a structure with the arguments passed in a Python binding function/method call
* @param[out] argstruct argument structure filled
* @param[in] args positional arguments or NULL
* @param[in] kwargnames NULL terminated list of argument names in their order of definition
* @param[in] cemap map of class ids to python class descriptions
* @return true on success, false on error, see error code in argstruct to determine the error
*/
bool papuga_python_set_CallArgs( papuga_CallArgs* argstruct, PyObject* args, const char** kwargnames, const papuga_python_ClassEntryMap* cemap);

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

#ifdef __cplusplus
}
#endif
#endif

