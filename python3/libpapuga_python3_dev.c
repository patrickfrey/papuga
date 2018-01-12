/*
* Copyright (c) 2017 Patrick P. Frey
*
* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/
/*
 * \brief Library implementation for Python (v3) bindings built by papuga
 * \file libpapuga_python3_dev.c
 */

#include "papuga/lib/python3_dev.h"
#include "papuga.h"
#include "private/dll_tags.h"
#include <stddef.h>
#include <math.h>
#include <float.h>
#include <stdio.h>
#include <string.h>
#include <stdio.h>
#include <signal.h>
#include <inttypes.h>
#include <stdarg.h>
#include <limits.h>
/* Python includes: */
#include <Python.h>
#include <structmember.h>

#undef PAPUGA_LOWLEVEL_DEBUG

typedef struct papuga_python_IteratorObject {
	PyObject_HEAD
	papuga_Iterator impl;
	const papuga_python_ClassEntryMap* cemap;
	int checksum;
	bool eof;
} papuga_python_IteratorObject;

static PyTypeObject* getTypeObject( const papuga_python_ClassEntryMap* cemap, int classid)
{
	if (classid <= 0 || classid > cemap->hoarsize) return NULL;
	return cemap->hoar[ classid-1];
}

static PyTypeObject* getTypeStruct( const papuga_python_ClassEntryMap* cemap, int structid)
{
	if (structid <= 0 || structid > cemap->soarsize) return NULL;
	return cemap->soar[ structid-1];
}

#ifdef PAPUGA_LOWLEVEL_DEBUG
static bool checkCircular_rec( PyObject* pyobj, PyObject** pyobjlist, size_t pyobjlistsize, size_t pyobjlistpos)
{
	PyObject** pydictref = NULL;
	size_t pidx = 0;

	if (!pyobj) return true;
	for (; pidx < pyobjlistpos; ++pidx)
	{
		if (pyobj == pyobjlist[ pidx]) return false;
	}
	if (pyobjlistpos == pyobjlistsize) return true;
	pyobjlist[ pyobjlistpos] = pyobj;

	if (PyDict_Check( pyobj))
	{
		PyObject* keyitem = NULL;
		PyObject* valitem = NULL;
		Py_ssize_t pos = 0;

		while (pidx < pyobjlistsize && PyDict_Next( pyobj, &pos, &keyitem, &valitem))
		{
			if (!checkCircular_rec( valitem, pyobjlist, pyobjlistsize, pyobjlistpos+1)) return false;
		}
	}
	else if (PyTuple_Check( pyobj))
	{
		Py_ssize_t ii = 0, size = PyTuple_GET_SIZE( pyobj);
		for (; ii < size; ++ii)
		{
			PyObject* item = PyTuple_GET_ITEM( pyobj, ii);
			if (!checkCircular_rec( item, pyobjlist, pyobjlistsize, pyobjlistpos+1)) return false;
		}
	}
	else if (PyList_Check( pyobj))
	{
		Py_ssize_t ii = 0, size = PyList_GET_SIZE( pyobj);
		for (; ii < size; ++ii)
		{
			PyObject* item = PyList_GET_ITEM( pyobj, ii);
			if (!checkCircular_rec( item, pyobjlist, pyobjlistsize, pyobjlistpos+1)) return false;
		}
	}
	else if (NULL!=(pydictref = _PyObject_GetDictPtr( pyobj)))
	{
		if (!checkCircular_rec( *pydictref, pyobjlist, pyobjlistsize, pyobjlistpos+1)) return false;
	}
	return true;
}

static bool checkCircular( PyObject* pyobj)
{
	PyObject* pyobjlist[ 64];
	size_t pyobjlistsize = sizeof(pyobjlist) / sizeof(pyobjlist[0]);
	return checkCircular_rec( pyobj, pyobjlist, pyobjlistsize, 0);
}

static void printIndent( FILE* out, int indent)
{
	while (indent--) fputc( '\t',  out);
}

static void printPyObject_rec( FILE* out, int indent, PyObject* pyobj, int depth)
{
	PyObject** pydictref = NULL;
	if (!pyobj) return;
	if (depth >= 0)
	{
		if (depth == 0)
		{
			fprintf( out, "[] #%d", (int)pyobj->ob_refcnt);
			return;
		}
	}
	if (pyobj == Py_None)
	{
		fprintf( out, "NULL #%d", (int)pyobj->ob_refcnt);
	}
	else if (pyobj == Py_True)
	{
		fprintf( out, "True #%d", (int)pyobj->ob_refcnt);
	}
	else if (pyobj == Py_False)
	{
		fprintf( out, "False #%d", (int)pyobj->ob_refcnt);
	}
	else if (PyLong_Check( pyobj))
	{
		fprintf( out, "%d #%d", (int)PyLong_AS_LONG( pyobj), (int)pyobj->ob_refcnt);
	}
	else if (PyFloat_Check( pyobj))
	{
		fprintf( out, "%f #%d", PyFloat_AS_DOUBLE( pyobj), (int)pyobj->ob_refcnt);
	}
	else if (PyBytes_Check( pyobj))
	{
		char* str;
		Py_ssize_t ii,strsize;
		if (0==PyBytes_AsStringAndSize( pyobj, &str, &strsize))
		{
			if (strsize > 1024)
			{
				strsize = 1024;
				while (str[strsize] < 0) ++strsize;
			}
			if (str) for (ii=0; ii<strsize; ++ii)
			{
				fputc( str[ii], out);
			}
		}
		fprintf( out, "#%d", (int)pyobj->ob_refcnt);
	}
	else if (PyUnicode_Check( pyobj))
	{
		Py_ssize_t ii,utf8bytes;
		char* utf8str;

		if (0<=PyUnicode_READY( pyobj))
		{
			utf8str = PyUnicode_AsUTF8AndSize( pyobj, &utf8bytes);
			if (utf8bytes > 1024) utf8bytes = 1024;
			if (utf8str) for (ii=0; ii<utf8bytes; ++ii)
			{
				fputc( utf8str[ii], out);
			}
		}
		fprintf( out, "#%d", (int)pyobj->ob_refcnt);
	}
	else if (indent >= 0 && PyDict_Check( pyobj))
	{
		PyObject* keyitem = NULL;
		PyObject* valitem = NULL;
		Py_ssize_t pos = 0;

		fprintf( out, "begin dict #%d\n", (int)pyobj->ob_refcnt);
		while (PyDict_Next( pyobj, &pos, &keyitem, &valitem))
		{
			printIndent( out, indent+1);
			printPyObject_rec( out, -1, keyitem, depth>0?(depth-1):depth);
			fputc( '=', out);
			printPyObject_rec( out, indent+1, valitem, depth>0?(depth-1):depth);
			fputc( '\n', out);
			fflush( out);
		}
		printIndent( out, indent);
		fputs( "end dict", out);
	}
	else if (indent >= 0 && PyTuple_Check( pyobj))
	{
		Py_ssize_t ii = 0, size = PyTuple_GET_SIZE( pyobj);
		fprintf( out, "begin tuple #%d\n", (int)pyobj->ob_refcnt);
		for (; ii < size; ++ii)
		{
			PyObject* item = PyTuple_GET_ITEM( pyobj, ii);
			printIndent( out, indent+1);
			fprintf( out, "%d:", (int)ii);
			printPyObject_rec( out, indent+1, item, depth>0?(depth-1):depth);
			fputc( '\n', out);
			fflush( out);
		}
		printIndent( out, indent);
		fputs( "end tuple", out);
	}
	else if (indent >= 0 && PyList_Check( pyobj))
	{
		Py_ssize_t ii = 0, size = PyList_GET_SIZE( pyobj);
		fprintf( out, "begin list #%d\n", (int)pyobj->ob_refcnt);
		for (; ii < size; ++ii)
		{
			PyObject* item = PyList_GET_ITEM( pyobj, ii);
			printIndent( out, indent+1);
			fprintf( out, "%d:", (int)ii);
			printPyObject_rec( out, indent+1, item, depth>0?(depth-1):depth);
			fputc( '\n', out);
			fflush( out);
		}
		printIndent( out, indent);
		fputs( "end list", out);
	}
	else if (NULL!=(pydictref = _PyObject_GetDictPtr( pyobj)))
	{
		fputs( " -> ", out);
		printPyObject_rec( out, indent, *pydictref, depth>0?(depth-1):depth);
	}
	else
	{
		fputs( "<invalid>", out);
	}
}

static void printPyObject( FILE* out, const char* msg, PyObject* pyobj, int depth)
{
	fprintf( out, "PyObject %s:\n", msg);
	printPyObject_rec( out, 0, pyobj, depth);
	fputc( '\n', out);
	fflush( out);
}

static void printValueVariant( FILE* out, const char* msg, const papuga_ValueVariant* val)
{
	papuga_Allocator allocator;
	papuga_init_Allocator( &allocator, 0, 0);

	if (papuga_ValueVariant_defined( val))
	{
		fprintf( out, "%s undefined\n", msg);
	}
	else if (val->valuetype == papuga_TypeSerialization)
	{
		const char* str = papuga_Serialization_tostring( val->value.serialization, &allocator);
		if (str)
		{
			fprintf( out, "%s as serialization:\n%s\n", msg, str);
		}
		else
		{
			fprintf( out, "%s as serialization (too big)\n", msg);
		}
	}
	else if (papuga_ValueVariant_isatomic( val))
	{
		size_t len;
		papuga_ErrorCode errcode = papuga_Ok;
		const char* str = papuga_ValueVariant_tostring( val, &allocator, &len, &errcode);
		fprintf( out, "%s as atomic value:", msg);
		if (errcode != papuga_Ok || len < fwrite( str, 1, len, out))
		{
			fprintf( out, "%s as atomic value: (error %s)\n", msg, papuga_ErrorCode_tostring( errcode));
		}
		else
		{
			fprintf( out, "\n");
		}
	}
	else
	{
		fprintf( out, "%s as %s\n", msg, papuga_Type_name( val->valuetype));
	}
	papuga_destroy_Allocator( &allocator);
}

static bool checkReferenceCount_rec( PyObject* pyobj)
{
	PyObject** pydictref = NULL;

	if (!pyobj) return true;
	if (pyobj == Py_None || pyobj == Py_True || pyobj == Py_False) return true;

	if (PyDict_Check( pyobj))
	{
		PyObject* keyitem = NULL;
		PyObject* valitem = NULL;
		Py_ssize_t pos = 0;

		while (PyDict_Next( pyobj, &pos, &keyitem, &valitem))
		{
			if (!checkReferenceCount_rec( keyitem)) return false;
			if (!checkReferenceCount_rec( valitem)) return false;
		}
	}
	else if (PyTuple_Check( pyobj))
	{
		Py_ssize_t ii = 0, size = PyTuple_GET_SIZE( pyobj);
		for (; ii < size; ++ii)
		{
			PyObject* item = PyTuple_GET_ITEM( pyobj, ii);
			if (!checkReferenceCount_rec( item)) return false;
		}
	}
	else if (PyList_Check( pyobj))
	{
		Py_ssize_t ii = 0, size = PyList_GET_SIZE( pyobj);
		for (; ii < size; ++ii)
		{
			PyObject* item = PyList_GET_ITEM( pyobj, ii);
			if (!checkReferenceCount_rec( item)) return false;
		}
	}
	else if (NULL!=(pydictref = _PyObject_GetDictPtr( pyobj)))
	{
		if (!checkReferenceCount_rec( *pydictref)) return false;
	}
	else if (PyLong_Check( pyobj) || PyFloat_Check( pyobj) || PyBytes_Check( pyobj) || PyUnicode_Check( pyobj))
	{
		return true;
	}
	if (pyobj->ob_refcnt == 1) return true;
	fprintf( stderr, "reference count of <%s> is %d\n", pyobj->ob_type->tp_name, (int)pyobj->ob_refcnt);
	return false;
}

static bool checkReferenceCount( PyObject* pyobj)
{
	return checkReferenceCount_rec( pyobj);
}
#endif

#define KNUTH_HASH 2654435761U
static int calcClassObjectCheckSum( papuga_python_ClassObject* cobj)
{
	return (cobj->classid * KNUTH_HASH) + (((uintptr_t)cobj->self << 2) ^ ((uintptr_t)cobj->destroy << 3));
}
static int calcStructObjectCheckSum( papuga_python_StructObject* cobj)
{
	return ((cobj->structid * cobj->elemarsize) * KNUTH_HASH);
}
static int calcIteratorCheckSum( const papuga_python_IteratorObject* iobj)
{
	return (int)(unsigned int)(((uintptr_t)iobj->impl.data+107) * KNUTH_HASH) ^ (uintptr_t)iobj->impl.destroy ^ ((uintptr_t)iobj->impl.getNext << 6) ^ ((uintptr_t)(iobj->cemap) << 3);
}

static papuga_python_ClassObject* getClassObject( PyObject* pyobj, const papuga_python_ClassEntryMap* cemap, papuga_ErrorCode* errcode)
{
	papuga_python_ClassObject* cobj;
	PyTypeObject* pytype = pyobj->ob_type;
	if ((int)pytype->tp_basicsize != (int)sizeof(papuga_python_ClassObject)) return NULL;
	cobj = (papuga_python_ClassObject*)pyobj;
	if (pytype != getTypeObject( cemap, cobj->classid)) return NULL;
	if (cobj->checksum != calcClassObjectCheckSum( cobj)) return NULL;
	return cobj;
}

static bool init_ValueVariant_pyobj_single( papuga_ValueVariant* value, papuga_Allocator* allocator, PyObject* pyobj, const papuga_python_ClassEntryMap* cemap, papuga_ErrorCode* errcode)
{
	papuga_python_ClassObject* cobj;
	if (pyobj == Py_None)
	{
		papuga_init_ValueVariant( value);
	}
	else if (pyobj == Py_True)
	{
		papuga_init_ValueVariant_bool( value, true);
	}
	else if (pyobj == Py_False)
	{
		papuga_init_ValueVariant_bool( value, false);
	}
	else if (PyLong_Check( pyobj))
	{
		papuga_init_ValueVariant_int( value, PyLong_AS_LONG( pyobj));
	}
	else if (PyFloat_Check( pyobj))
	{
		papuga_init_ValueVariant_double( value, PyFloat_AS_DOUBLE( pyobj));
	}
	else if (PyBytes_Check( pyobj))
	{
		char* str;
		Py_ssize_t strsize;
		if (0==PyBytes_AsStringAndSize( pyobj, &str, &strsize))
		{
			papuga_init_ValueVariant_string( value, str, strsize);
		}
		else
		{
			papuga_init_ValueVariant( value);
		}
	}
	else if (PyUnicode_Check( pyobj))
	{
		Py_ssize_t utf8bytes;
		char* utf8str;

		if (0>PyUnicode_READY( pyobj))
		{
			*errcode = papuga_NoMemError;
			return false;
		}
		utf8str = PyUnicode_AsUTF8AndSize( pyobj, &utf8bytes);
		papuga_init_ValueVariant_string( value, utf8str, utf8bytes);
	}
	else if (NULL!=(cobj = getClassObject( pyobj, cemap, errcode)))
	{
		papuga_HostObject* hobj = papuga_Allocator_alloc_HostObject( allocator, cobj->classid, cobj->self, NULL/*deleter*/);
		if (hobj == NULL)
		{
			*errcode = papuga_NoMemError;
			return false;
		}
		papuga_init_ValueVariant_hostobj( value, hobj);
	}
	else
	{
		return false;
	}
	return true;
}

/* Forward declarations: */
static bool serialize_element( papuga_Serialization* ser, papuga_Allocator* allocator, PyObject* pyobj, const papuga_python_ClassEntryMap* cemap, papuga_ErrorCode* errcode);
static bool serialize_struct( papuga_Serialization* ser, papuga_Allocator* allocator, PyObject* pyobj, const papuga_python_ClassEntryMap* cemap, papuga_ErrorCode* errcode);

static bool serialize_members( papuga_Serialization* ser, papuga_Allocator* allocator, PyObject* pyobj, const papuga_python_ClassEntryMap* cemap, papuga_ErrorCode* errcode)
{
#define GET_PYSTRUCT_MEMBER( Type, pyobj, offset) *(Type*)((unsigned char*)(void*)pyobj + offset)

	PyMemberDef const* mi = pyobj->ob_type->tp_members;
	if (mi) for (; mi->name; ++mi)
	{
		switch (mi->type)
		{
			case T_BOOL:	if (!papuga_Serialization_pushName_charp( ser, mi->name)) goto ERROR_NOMEM;
					if (!papuga_Serialization_pushValue_bool( ser, GET_PYSTRUCT_MEMBER( char, pyobj, mi->offset))) goto ERROR_NOMEM;
					break;
			case T_CHAR:	if (!papuga_Serialization_pushName_charp( ser, mi->name)) goto ERROR_NOMEM;
					if (!papuga_Serialization_pushValue_int( ser, GET_PYSTRUCT_MEMBER( char, pyobj, mi->offset))) goto ERROR_NOMEM;
					break;
			case T_BYTE:	if (!papuga_Serialization_pushName_charp( ser, mi->name)) goto ERROR_NOMEM;
					if (!papuga_Serialization_pushValue_int( ser, GET_PYSTRUCT_MEMBER( char, pyobj, mi->offset))) goto ERROR_NOMEM;
					break;
			case T_UBYTE:	if (!papuga_Serialization_pushName_charp( ser, mi->name)) goto ERROR_NOMEM;
					if (!papuga_Serialization_pushValue_int( ser, GET_PYSTRUCT_MEMBER( unsigned char, pyobj, mi->offset))) goto ERROR_NOMEM;
					break;
			case T_SHORT:	if (!papuga_Serialization_pushName_charp( ser, mi->name)) goto ERROR_NOMEM;
					if (!papuga_Serialization_pushValue_int( ser, GET_PYSTRUCT_MEMBER( short, pyobj, mi->offset))) goto ERROR_NOMEM;
					break;
			case T_USHORT:	if (!papuga_Serialization_pushName_charp( ser, mi->name)) goto ERROR_NOMEM;
					if (!papuga_Serialization_pushValue_int( ser, GET_PYSTRUCT_MEMBER( unsigned short, pyobj, mi->offset))) goto ERROR_NOMEM;
					break;
			case T_INT:	if (!papuga_Serialization_pushName_charp( ser, mi->name)) goto ERROR_NOMEM;
					if (!papuga_Serialization_pushValue_int( ser, GET_PYSTRUCT_MEMBER( int, pyobj, mi->offset))) goto ERROR_NOMEM;
					break;
			case T_UINT:	if (!papuga_Serialization_pushName_charp( ser, mi->name)) goto ERROR_NOMEM;
					if (!papuga_Serialization_pushValue_int( ser, GET_PYSTRUCT_MEMBER( unsigned int, pyobj, mi->offset))) goto ERROR_NOMEM;
					break;
			case T_LONG:	if (!papuga_Serialization_pushName_charp( ser, mi->name)) goto ERROR_NOMEM;
					if (!papuga_Serialization_pushValue_int( ser, GET_PYSTRUCT_MEMBER( long, pyobj, mi->offset))) goto ERROR_NOMEM;
					break;
			case T_ULONG:	if (!papuga_Serialization_pushName_charp( ser, mi->name)) goto ERROR_NOMEM;
					if (!papuga_Serialization_pushValue_int( ser, GET_PYSTRUCT_MEMBER( unsigned long, pyobj, mi->offset))) goto ERROR_NOMEM;
					break;
			case T_LONGLONG:if (!papuga_Serialization_pushName_charp( ser, mi->name)) goto ERROR_NOMEM;
					if (!papuga_Serialization_pushValue_int( ser, GET_PYSTRUCT_MEMBER( long long, pyobj, mi->offset))) goto ERROR_NOMEM;
					break;
			case T_ULONGLONG: if (!papuga_Serialization_pushName_charp( ser, mi->name)) goto ERROR_NOMEM;
					if (!papuga_Serialization_pushValue_int( ser, GET_PYSTRUCT_MEMBER( unsigned long long, pyobj, mi->offset))) goto ERROR_NOMEM;
					break;
			case T_PYSSIZET:if (!papuga_Serialization_pushName_charp( ser, mi->name)) goto ERROR_NOMEM;
					if (!papuga_Serialization_pushValue_int( ser, GET_PYSTRUCT_MEMBER( Py_ssize_t, pyobj, mi->offset))) goto ERROR_NOMEM;
					break;
			case T_FLOAT:	if (!papuga_Serialization_pushName_charp( ser, mi->name)) goto ERROR_NOMEM;
					if (!papuga_Serialization_pushValue_double( ser, GET_PYSTRUCT_MEMBER( float, pyobj, mi->offset))) goto ERROR_NOMEM;
					break;
			case T_DOUBLE:	if (!papuga_Serialization_pushName_charp( ser, mi->name)) goto ERROR_NOMEM;
					if (!papuga_Serialization_pushValue_double( ser, GET_PYSTRUCT_MEMBER( double, pyobj, mi->offset))) goto ERROR_NOMEM;
					break;
			case T_STRING:
			{
					const char* memval = GET_PYSTRUCT_MEMBER( char*, pyobj, mi->offset);
					if (memval)
					{
						if (!papuga_Serialization_pushName_charp( ser, mi->name)) goto ERROR_NOMEM;
						if (!papuga_Serialization_pushValue_charp( ser, memval)) goto ERROR_NOMEM;
					}
					break;
			}
			case T_OBJECT:
			case T_OBJECT_EX:
			{
				PyObject* memobj = GET_PYSTRUCT_MEMBER( PyObject*, pyobj, mi->offset);
				if (memobj && memobj != Py_None)
				{
					papuga_ValueVariant singleval;
					if (!papuga_Serialization_pushName_charp( ser, mi->name)) goto ERROR_NOMEM;
					if (init_ValueVariant_pyobj_single( &singleval, allocator, memobj, cemap, errcode))
					{
						if (!papuga_Serialization_pushValue( ser, &singleval)) goto ERROR_NOMEM;
					}
					else if (*errcode == papuga_Ok)
					{
						/* ... the member is a substructure, we have to enclose it in open/close brackets */
						if (!papuga_Serialization_pushOpen( ser)) goto ERROR_NOMEM;
						if (!serialize_struct( ser, allocator, memobj, cemap, errcode)) return false;
						if (!papuga_Serialization_pushClose( ser)) goto ERROR_NOMEM;
					}
					else
					{
						return false;
					}
				}
				break;
			}
			default:
				*errcode = papuga_NotImplemented;
				return false;
		}
	}
	return true;
ERROR_NOMEM:
	*errcode = papuga_NoMemError;
	return false;
}

static bool serialize_struct( papuga_Serialization* ser, papuga_Allocator* allocator, PyObject* pyobj, const papuga_python_ClassEntryMap* cemap, papuga_ErrorCode* errcode)
{
	PyObject** pydictref = NULL;
	if (PyDict_Check( pyobj))
	{
		PyObject* keyitem = NULL;
		PyObject* valitem = NULL;
		Py_ssize_t pos = 0;

		while (PyDict_Next( pyobj, &pos, &keyitem, &valitem))
		{
			papuga_ValueVariant keyval;
			/* Serialize key: */
			if (!init_ValueVariant_pyobj_single( &keyval, allocator, keyitem, cemap, errcode))
			{
				if (*errcode == papuga_Ok) *errcode = papuga_TypeError;
				return false;
			}
			if (keyval.valuetype == papuga_TypeString && keyval.encoding == papuga_UTF8 && keyval.length > 0 && keyval.value.string[0] == '_') continue;
			/* ... do not serialize internal members (key string starting with '_') */
			if (!papuga_Serialization_pushName( ser, &keyval)) goto ERROR_NOMEM;

			/* Serialize value: */
			if (!serialize_element( ser, allocator, valitem, cemap, errcode)) return false;
		}
	}
	else if (PyTuple_Check( pyobj))
	{
		Py_ssize_t ii = 0, size = PyTuple_GET_SIZE( pyobj);
		for (; ii < size; ++ii)
		{
			PyObject* item = PyTuple_GET_ITEM( pyobj, ii);
			if (!serialize_element( ser, allocator, item, cemap, errcode)) return false;
		}
	}
	else if (PyList_Check( pyobj))
	{
		Py_ssize_t ii = 0, size = PyList_GET_SIZE( pyobj);
		for (; ii < size; ++ii)
		{
			PyObject* item = PyList_GET_ITEM( pyobj, ii);
			if (!serialize_element( ser, allocator, item, cemap, errcode)) return false;
		}
	}
	else if (NULL!=(pydictref = _PyObject_GetDictPtr( pyobj)))
	{
		if (!serialize_struct( ser, allocator, *pydictref, cemap, errcode)) goto ERROR_NOMEM;
	}
	else if (pyobj->ob_type->tp_members)
	{
		if (!serialize_members( ser, allocator, pyobj, cemap, errcode)) return false;
	}
	else
	{
		*errcode = papuga_TypeError;
		return false;
	}
	return true;
ERROR_NOMEM:
	*errcode = papuga_NoMemError;
	return false;
}

static bool serialize_element( papuga_Serialization* ser, papuga_Allocator* allocator, PyObject* pyobj, const papuga_python_ClassEntryMap* cemap, papuga_ErrorCode* errcode)
{
	papuga_ValueVariant elemval;
	if (init_ValueVariant_pyobj_single( &elemval, allocator, pyobj, cemap, errcode))
	{
		if (!papuga_Serialization_pushValue( ser, &elemval)) goto ERROR_NOMEM;
	}
	else
	{
		if (*errcode != papuga_Ok) return false;
		if (!papuga_Serialization_pushOpen( ser)) goto ERROR_NOMEM;
		if (!serialize_struct( ser, allocator, pyobj, cemap, errcode)) return false;
		if (!papuga_Serialization_pushClose( ser)) goto ERROR_NOMEM;
	}
	return true;
ERROR_NOMEM:
	*errcode = papuga_NoMemError;
	return false;
}

static bool init_ValueVariant_pyobj( papuga_ValueVariant* value, papuga_Allocator* allocator, PyObject* pyobj, const papuga_python_ClassEntryMap* cemap, papuga_ErrorCode* errcode)
{
	if (init_ValueVariant_pyobj_single( value, allocator, pyobj, cemap, errcode))
	{
		return true;
	}
	else
	{
		papuga_Serialization* ser;
		if (*errcode != papuga_Ok) return false;
		ser = papuga_Allocator_alloc_Serialization( allocator);
		if (!ser) goto ERROR_NOMEM;

		papuga_init_ValueVariant_serialization( value, ser);
		if (!serialize_struct( ser, allocator, pyobj, cemap, errcode)) return false;
		return true;
	}
ERROR_NOMEM:
	*errcode = papuga_NoMemError;
	return false;
}

static PyObject* papuga_Iterator_iter( PyObject *selfobj)
{
	Py_XINCREF(selfobj);
	return selfobj;
}

static void papuga_Iterator_dealloc( PyObject *selfobj_)
{
	papuga_python_IteratorObject* selfobj = (papuga_python_IteratorObject*)selfobj_;
	if (selfobj->checksum == calcIteratorCheckSum( selfobj))
	{
		if (selfobj->impl.destroy)
		{
			selfobj->impl.destroy( selfobj->impl.data);
			selfobj->impl.destroy = 0;
		}
	}
	else
	{
		papuga_python_error( "%s", papuga_ErrorCode_tostring( papuga_InvalidAccess));
	}
}

static PyObject* papuga_Iterator_next( PyObject *selfobj_)
{
	papuga_CallResult result;
	papuga_ErrorCode errcode;
	char membuf[ 4096];
	char errbuf[ 128];

	papuga_python_IteratorObject* selfobj = (papuga_python_IteratorObject*)selfobj_;
	if (selfobj->checksum != calcIteratorCheckSum( selfobj))
	{
		papuga_python_error( "%s", papuga_ErrorCode_tostring( papuga_InvalidAccess));
		return NULL;
	}
	if (selfobj->eof)
	{
		PyErr_SetNone( PyExc_StopIteration);
		return NULL;
	}
	papuga_init_CallResult( &result, membuf, sizeof(membuf), errbuf, sizeof(errbuf));
	if (selfobj->impl.getNext( selfobj->impl.data, &result))
	{
		PyObject* rt = papuga_python_move_CallResult( &result, selfobj->cemap, &errcode);
		if (!rt) papuga_python_error( "%s", papuga_ErrorCode_tostring( errcode));
		return rt;
	}
	else
	{
		if (papuga_ErrorBuffer_hasError( &result.errorbuf))
		{
			papuga_python_error( "%s", papuga_ErrorBuffer_lastError( &result.errorbuf));
		}
		papuga_destroy_CallResult( &result);
		selfobj->eof = true;
		PyErr_SetNone( PyExc_StopIteration);
		return NULL;
	}
}

static PyTypeObject g_papuga_IteratorType = {
	PyVarObject_HEAD_INIT(NULL, 0)
	"papuga_Iterator",	/*tp_name*/
	sizeof(papuga_python_IteratorObject),/*tp_basicsize*/
	0,			/*tp_itemsize*/
	papuga_Iterator_dealloc,/*tp_dealloc*/
	0,			/*tp_print*/
	0,			/*tp_getattr*/
	0,			/*tp_setattr*/
	0,			/*tp_compare*/
	0,			/*tp_repr*/
	0,			/*tp_as_number*/
	0,			/*tp_as_sequence*/
	0,			/*tp_as_mapping*/
	0,			/*tp_hash */
	0,			/*tp_call*/
	0,			/*tp_str*/
	0,			/*tp_getattro*/
	0,			/*tp_setattro*/
	0,			/*tp_as_buffer*/
	0,			/*tp_flags */
	"papuga iterator object.",/* tp_doc */
	0,			/* tp_traverse */
	0,			/* tp_clear */
	0,			/* tp_richcompare */
	0,			/* tp_weaklistoffset */
	papuga_Iterator_iter,	/* tp_iter: __iter__() method */
	papuga_Iterator_next/* tp_iternext: next() method */
};

static PyObject* createPyObjectFromIterator( papuga_Iterator* iterator, const papuga_python_ClassEntryMap* cemap, papuga_ErrorCode* errcode)
{
	papuga_python_IteratorObject* itr;
	PyObject* iterobj = PyType_GenericAlloc( &g_papuga_IteratorType, 1);
	if (!iterobj)
	{
		*errcode = papuga_NoMemError;
		return NULL;
	}
	itr = (papuga_python_IteratorObject*)iterobj;
	papuga_init_Iterator( &itr->impl, iterator->data, iterator->destroy, iterator->getNext);
	itr->cemap = cemap;
	itr->eof = false;
	itr->checksum = calcIteratorCheckSum( itr);
	return iterobj;
}

static PyObject* createPyObjectFromVariant( papuga_Allocator* allocator, const papuga_ValueVariant* value, const papuga_python_ClassEntryMap* cemap, papuga_ErrorCode* errcode);

/*
* @brief Initialize an allocated host object in the Python context
* @param[in] selfobj pointer to the allocated and zeroed python object
* @param[in] structid type interface identifier of the object
* @param[in] self pointer to host object representation
*/
static void papuga_python_init_struct( PyObject* selfobj, int structid)
{
	papuga_python_StructObject* cobj = (papuga_python_StructObject*)selfobj;
	cobj->structid = structid;
	cobj->elemarsize = (Py_TYPE(selfobj)->tp_basicsize - sizeof(papuga_python_StructObject)) / sizeof(papuga_python_StructObjectElement);
	cobj->checksum = calcStructObjectCheckSum( cobj);
}

/*
* @brief Create a structure (return value structure) representation in the Python context
* @param[in] sructid struct interface identifier of the object
* @param[in] cemap map of struct ids to python class descriptions
* @param[in,out] errcode error code in case of NULL returned
* @return object without reference increment, NULL on error
*/
static PyObject* papuga_python_create_struct( int structid, const papuga_python_ClassEntryMap* cemap, papuga_ErrorCode* errcode)
{
	PyObject* selfobj;
	PyTypeObject* typeobj = getTypeStruct( cemap, structid);
	if (!typeobj)
	{
		*errcode = papuga_InvalidAccess;
		return NULL;
	}
	selfobj = PyType_GenericAlloc( typeobj, 1);
	if (!selfobj)
	{
		*errcode = papuga_NoMemError;
		return NULL;
	}
	papuga_python_init_struct( selfobj, structid);
	return selfobj;
}


typedef struct papuga_PyStructNode
{
	papuga_ValueVariant keyval;
	PyObject* valobj;
} papuga_PyStructNode;

static void papuga_init_PyStructNode( papuga_PyStructNode* nd)
{
	papuga_init_ValueVariant( &nd->keyval);
	nd->valobj = NULL;
}

static void papuga_destroy_PyStructNode( papuga_PyStructNode* nd)
{
	Py_XDECREF( nd->valobj);
}

static void papuga_PyStructNode_set_key( papuga_PyStructNode* nd, const papuga_ValueVariant* keyval)
{
	papuga_init_ValueVariant_copy( &nd->keyval, keyval);
}

static void papuga_PyStructNode_move_value( papuga_PyStructNode* nd, PyObject* val)
{
	nd->valobj = val;
}

typedef struct papuga_PyStruct
{
	papuga_Stack stk;
	char stk_local_mem[ 2048];
	int nofKeyValuePairs;
	int structid;
} papuga_PyStruct;

static void papuga_init_PyStruct( papuga_PyStruct* pystruct, int structid)
{
	pystruct->nofKeyValuePairs = 0;
	pystruct->structid = structid;
	papuga_init_Stack( &pystruct->stk, sizeof(papuga_PyStructNode)/*elemsize*/, 128/*nodesize*/, pystruct->stk_local_mem, sizeof(pystruct->stk_local_mem));
}

static void papuga_destroy_PyStruct( papuga_PyStruct* pystruct)
{
	papuga_PyStructNode* nd;
	while (!!(nd=(papuga_PyStructNode*)papuga_Stack_pop( &pystruct->stk)))
	{
		papuga_destroy_PyStructNode( nd);
	}
	papuga_destroy_Stack( &pystruct->stk);
}

static bool papuga_PyStruct_push_node( papuga_PyStruct* pystruct, papuga_PyStructNode* nd)
{
	void* ndmem = papuga_Stack_push( &pystruct->stk);
	if (!ndmem) return false;
	memcpy( ndmem, nd, sizeof(*nd));
	papuga_init_PyStructNode( nd);
	return true;
}

static PyObject* papuga_PyStruct_create_dict( const papuga_PyStruct* pystruct, papuga_PyStructNode** nodes, size_t nofnodes, papuga_Allocator* allocator, const papuga_python_ClassEntryMap* cemap, papuga_ErrorCode* errcode)
{
	PyObject* rt = PyDict_New();
	if (rt)
	{
		long currIndex = 0;
		int ei, ee;
		for (ei = 0, ee = nofnodes; ei != ee; ++ei)
		{
			papuga_PyStructNode* nd = nodes[ ei];
			PyObject* keyobj = NULL;
			if (!nd) break;
			if (!papuga_ValueVariant_defined( &nd->keyval))
			{
				keyobj = PyLong_FromLong( currIndex++);
				if (!keyobj)
				{
					*errcode = papuga_NoMemError;
					break;
				}
			}
			else if ((papuga_Type)nd->keyval.valuetype == papuga_TypeInt)
			{
				currIndex = papuga_ValueVariant_toint( &nd->keyval, errcode);
				keyobj = PyLong_FromLong( currIndex++);
				if (!keyobj)
				{
					*errcode = papuga_NoMemError;
					break;
				}
				if (currIndex == 0)
				{
					Py_DECREF( keyobj);
					*errcode = papuga_OutOfRangeError;
					break;
				}
			}
			else
			{
				keyobj = createPyObjectFromVariant( allocator, &nd->keyval, cemap, errcode);
				if (!keyobj) break;
			}
			if (0>PyDict_SetItem( rt, keyobj, nd->valobj))
			{
				Py_DECREF( keyobj);
				*errcode = papuga_NoMemError;
				break;
			}
			Py_DECREF( keyobj);
		}
		if (ei != ee)
		{
			Py_DECREF( rt);
			return NULL;
		}
	}
	else
	{
		*errcode = papuga_NoMemError;
		return NULL;
	}
	return rt;
}

static PyObject* papuga_PyStruct_create_list( papuga_PyStruct* pystruct, papuga_PyStructNode** nodes, size_t nofnodes, papuga_ErrorCode* errcode)
{
	PyObject* rt = PyList_New( papuga_Stack_size( &pystruct->stk));
	if (rt)
	{
		int ei, ee;
		for (ei = 0, ee = nofnodes; ei != ee; ++ei)
		{
			papuga_PyStructNode* nd = nodes[ ei];
			if (0>PyList_SetItem( rt, ei, nd->valobj)) break;
			nd->valobj = NULL; /*... reference is stolen by .._SetItem, therefore we hide the old reference with its count as we do not need it anymore (INCREF would also be possible) */
		}
		if (ei != ee)
		{
			Py_DECREF( rt);
			*errcode = papuga_NoMemError;
			return NULL;
		}
	}
	else
	{
		*errcode = papuga_NoMemError;
		return NULL;
	}
	return rt;
}

static PyObject* papuga_PyStruct_create_struct( papuga_PyStruct* pystruct, papuga_PyStructNode** nodes, size_t nofnodes, papuga_Allocator* allocator, const papuga_python_ClassEntryMap* cemap, papuga_ErrorCode* errcode)
{
	const PyMemberDef* members;
	PyObject* rt = papuga_python_create_struct( pystruct->structid, cemap, errcode);
	papuga_python_StructObject* cobj = (papuga_python_StructObject*)rt;
	int currIndex = 0;
	int ei, ee;
	if (!rt) return NULL;

	members = rt->ob_type->tp_members;
	if (!members)
	{
		*errcode = papuga_InvalidAccess;
		Py_DECREF( rt);
		return NULL;
	}
	for (ei = 0, ee = nofnodes; ei != ee; ++ei)
	{
		papuga_PyStructNode* nd = nodes[ ei];
		int memberidx;

		if (!nd) break;
		if (papuga_ValueVariant_defined( &nd->keyval))
		{
			PyMemberDef const* mi;
			char const* keystr;
			size_t keylen;

			if (papuga_ValueVariant_isstring( &nd->keyval))
			{
				keystr = papuga_ValueVariant_tostring( &nd->keyval, allocator, &keylen, errcode);
				mi = members;
				memberidx = 0;
				for (; mi->name; ++mi,++memberidx)
				{
					size_t ki = 0;
					for (; ki<keylen && keystr[ki] == mi->name[ki]; ++ki){}
					if (ki == keylen && mi->name[ki] == '\0') break;
				}
				if (!mi->name)
				{
					*errcode = papuga_InvalidAccess;
					break;
				}
				currIndex = memberidx+1;
			}
			else if ((papuga_Type)nd->keyval.valuetype == papuga_TypeInt)
			{
				memberidx = papuga_ValueVariant_toint( &nd->keyval, errcode);
				if (!memberidx && *errcode != papuga_Ok)
				{
					*errcode = papuga_InvalidAccess;
					break;
				}
				currIndex = memberidx+1;
				if (!currIndex)
				{
					*errcode = papuga_InvalidAccess;
					break;
				}
			}
			else
			{
				*errcode = papuga_TypeError;
				break;
			}
		}
		else
		{
			memberidx = currIndex++;
		}
		if (memberidx >= (int)cobj->elemarsize)
		{
			*errcode = papuga_InvalidAccess;
			break;
		}
		if (cobj->elemar[ memberidx].pyobj)
		{
			*errcode = papuga_DuplicateDefinition;
			break;
		}
		cobj->elemar[ memberidx].pyobj = nd->valobj;
		nd->valobj = NULL;
	}
	if (ei != ee)
	{
		Py_DECREF( rt);
		return NULL;
	}
	for (ei=0; ei<cobj->elemarsize; ++ei)
	{
		if (!cobj->elemar[ ei].pyobj)
		{
			Py_INCREF( cobj->elemar[ ei].pyobj = Py_None);
		}
	}
	return rt;
}

static PyObject* papuga_PyStruct_create_object( papuga_PyStruct* pystruct, papuga_Allocator* allocator, const papuga_python_ClassEntryMap* cemap, papuga_ErrorCode* errcode)
{
	PyObject* rt = NULL;
	size_t nofnodes = papuga_Stack_size( &pystruct->stk);
	papuga_PyStructNode* nodesbuf[ 1024];
	papuga_PyStructNode** nodes = (nofnodes > sizeof(nodesbuf)/sizeof(nodesbuf[0]))
		? (papuga_PyStructNode**)malloc( nofnodes * sizeof(papuga_PyStructNode*))
		: nodesbuf;
	if (!nodes)
	{
		*errcode = papuga_NoMemError;
		return false;
	}
	if (!papuga_Stack_top_n( &pystruct->stk, (void**)nodes, nofnodes))
	{
		if (nodes != nodesbuf) free( nodes);
		*errcode = papuga_NoMemError;
		return false;
	}
	if (pystruct->structid)
	{
		rt = papuga_PyStruct_create_struct( pystruct, nodes, nofnodes, allocator, cemap, errcode);
	}
	else if (pystruct->nofKeyValuePairs > 0)
	{
		rt = papuga_PyStruct_create_dict( pystruct, nodes, nofnodes, allocator, cemap, errcode);
	}
	else
	{
		rt = papuga_PyStruct_create_list( pystruct, nodes, nofnodes, errcode);
	}
	if (nodes != nodesbuf) free( nodes);
	return rt;
}

static bool papuga_init_PyStruct_serialization( papuga_PyStruct* pystruct, int structid, papuga_Allocator* allocator, papuga_SerializationIter* seriter, const papuga_python_ClassEntryMap* cemap, papuga_ErrorCode* errcode)
{
	papuga_PyStructNode nd;
	papuga_init_PyStruct( pystruct, structid);
	papuga_init_PyStructNode( &nd);

	for (; papuga_SerializationIter_tag(seriter) != papuga_TagClose; papuga_SerializationIter_skip(seriter))
	{
		if (papuga_SerializationIter_tag(seriter) == papuga_TagName)
		{
			const papuga_ValueVariant* keyval = papuga_SerializationIter_value(seriter);
			if (papuga_ValueVariant_defined( &nd.keyval) || !papuga_ValueVariant_isatomic( keyval))
			{
				*errcode = papuga_TypeError;
				goto ERROR;
			}
			papuga_PyStructNode_set_key( &nd, keyval);
			++pystruct->nofKeyValuePairs;
		}
		else if (papuga_SerializationIter_tag(seriter) == papuga_TagValue)
		{
			PyObject* val = createPyObjectFromVariant( allocator, papuga_SerializationIter_value(seriter), cemap, errcode);
			if (!val) goto ERROR;
			papuga_PyStructNode_move_value( &nd, val);

			if (!papuga_PyStruct_push_node( pystruct, &nd))
			{
				*errcode = papuga_NoMemError;
				goto ERROR;
			}
		}
		else if (papuga_SerializationIter_tag(seriter) == papuga_TagOpen)
		{
			papuga_PyStruct subpystruct;
			int substructid = 0;
			PyObject* subval;
			const papuga_ValueVariant* openval = papuga_SerializationIter_value(seriter);

			papuga_SerializationIter_skip(seriter);
			if (papuga_ValueVariant_defined( openval))
			{
				substructid = papuga_ValueVariant_toint( openval, errcode);
				if (!substructid) goto ERROR;
			}
			if (!papuga_init_PyStruct_serialization( &subpystruct, substructid, allocator, seriter, cemap, errcode))
			{
				goto ERROR;
			}
			subval = papuga_PyStruct_create_object( &subpystruct, allocator, cemap, errcode);
			papuga_destroy_PyStruct( &subpystruct);
			if (!subval) goto ERROR;
			papuga_PyStructNode_move_value( &nd, subval);
			if (!papuga_PyStruct_push_node( pystruct, &nd))
			{
				*errcode = papuga_NoMemError;
				goto ERROR;
			}
			if (papuga_SerializationIter_eof(seriter))
			{
				*errcode = papuga_UnexpectedEof;
				goto ERROR;
			}
		}
	}
	if (papuga_ValueVariant_defined( &nd.keyval))
	{
		*errcode = papuga_UnexpectedEof;
		goto ERROR;
	}
	return true;
ERROR:
	papuga_destroy_PyStruct( pystruct);
	papuga_destroy_PyStructNode( &nd);
	return false;
}

static PyObject* createPyObjectFromVariant( papuga_Allocator* allocator, const papuga_ValueVariant* value, const papuga_python_ClassEntryMap* cemap, papuga_ErrorCode* errcode)
{
	PyObject* rt = NULL;
	switch (value->valuetype)
	{
		case papuga_TypeVoid:
			Py_INCREF( Py_None);
			rt = Py_None;
			break;
		case papuga_TypeDouble:
			rt = PyFloat_FromDouble( value->value.Double);
			break;
		case papuga_TypeInt:
			if (value->value.Int > LONG_MAX || value->value.Int < LONG_MIN)
			{
				*errcode = papuga_OutOfRangeError;
				return NULL;
			}
			rt = PyLong_FromLong( value->value.Int);
			break;
		case papuga_TypeBool:
			if (value->value.Bool)
			{
				Py_INCREF( Py_True);
				rt = Py_True;
			}
			else
			{
				Py_INCREF( Py_False);
				rt = Py_False;
			}
			break;
		case papuga_TypeString:
		{
			if ((papuga_StringEncoding)value->encoding == papuga_UTF8)
			{
				rt = PyUnicode_FromStringAndSize( value->value.string, value->length);
			}
			else
			{
				size_t len;
				const char* str = papuga_ValueVariant_tostring( value, allocator, &len, errcode);
				if (!str) return NULL;
				rt = PyUnicode_FromStringAndSize( str, len);
			}
			if (rt == NULL)
			{
				/* check if malloc is possible to guess the error cause: */
				void* mem = malloc( value->length+1);
				if (mem)
				{
					free( mem);
					*errcode = papuga_EncodingError;
					/* ... PF:HACK: This might not always work, implements sane encoding error checking later */
				}
			}
			break;
		}
		case papuga_TypeHostObject:
		{
			papuga_HostObject* hobj = value->value.hostObject;
			rt = papuga_python_create_object( hobj->data, hobj->classid, hobj->destroy, cemap, errcode);
			if (rt) papuga_release_HostObject( hobj);
			break;
		}
		case papuga_TypeSerialization:
		{
			papuga_PyStruct pystruct;
			papuga_SerializationIter seriter;
			int structid = papuga_Serialization_structid( value->value.serialization);

			papuga_init_SerializationIter( &seriter, value->value.serialization);
			if (!papuga_init_PyStruct_serialization( &pystruct, structid, allocator, &seriter, cemap, errcode))
			{
				papuga_destroy_PyStruct( &pystruct);
				return NULL;
			}
			rt = papuga_PyStruct_create_object( &pystruct, allocator, cemap, errcode);
			papuga_destroy_PyStruct( &pystruct);
			break;
		}
		case papuga_TypeIterator:
		{
			papuga_Iterator* itr = value->value.iterator;
			rt = createPyObjectFromIterator( itr, cemap, errcode);
			if (rt) papuga_release_Iterator( itr);
			break;
		}
		default:
			*errcode = papuga_TypeError;
			return NULL;
	}
	if (rt == NULL && *errcode == papuga_Ok)
	{
		*errcode = papuga_NoMemError;
	}
	return rt;
}

static PyObject* papuga_python_create_tuple( PyObject** ar, size_t arsize, papuga_ErrorCode* errcode)
{
	PyObject* rt = PyTuple_New( arsize);
	if (rt)
	{
		size_t ei = 0, ee = arsize;
		for (; ei != ee; ++ei)
		{
			if (0>PyTuple_SetItem( rt, ei, ar[ei])) break;
			ar[ei] = NULL; /*... reference is stolen by .._SetItem, therefore we hide the old reference with its count as we do not need it anymore (INCREF would also be possible) */
		}
		if (ei != ee)
		{
			*errcode = papuga_NoMemError;
			Py_XDECREF( rt);
			return NULL;
		}
	}
	else
	{
		*errcode = papuga_NoMemError;
		return NULL;
	}
	return rt;
}

DLL_PUBLIC int papuga_python_init(void)
{
	if (PyType_Ready(&g_papuga_IteratorType) < 0) return -1;
	Py_INCREF( &g_papuga_IteratorType);
	return 0;
}

DLL_PUBLIC void papuga_python_init_object( PyObject* selfobj, void* self, int classid, papuga_Deleter destroy)
{
	papuga_python_ClassObject* cobj = (papuga_python_ClassObject*)selfobj;
	cobj->classid = classid;
	cobj->self = self;
	cobj->destroy = destroy;
	cobj->checksum = calcClassObjectCheckSum( cobj);
}

DLL_PUBLIC PyObject* papuga_python_create_object( void* self, int classid, papuga_Deleter destroy, const papuga_python_ClassEntryMap* cemap, papuga_ErrorCode* errcode)
{
	PyObject* selfobj;
	PyTypeObject* typeobj = getTypeObject( cemap, classid);
	if (!typeobj)
	{
		*errcode = papuga_InvalidAccess;
		return NULL;
	}
	selfobj = PyType_GenericAlloc( typeobj, 1);
	if (!selfobj)
	{
		*errcode = papuga_NoMemError;
		return NULL;
	}
	papuga_python_init_object( selfobj, self, classid, destroy);
	return selfobj;
}

DLL_PUBLIC void papuga_python_destroy_object( PyObject* selfobj)
{
	papuga_python_ClassObject* cobj = (papuga_python_ClassObject*)selfobj;
	if (cobj->checksum == calcClassObjectCheckSum( cobj))
	{
		cobj->destroy( cobj->self);
		Py_TYPE(selfobj)->tp_free( selfobj);
	}
	else
	{
		papuga_python_error( "%s", papuga_ErrorCode_tostring( papuga_InvalidAccess));
	}
}

DLL_PUBLIC void papuga_python_destroy_struct( PyObject* selfobj)
{
	papuga_python_StructObject* cobj = (papuga_python_StructObject*)selfobj;
	if (cobj->checksum == calcStructObjectCheckSum( cobj))
	{
		int ii;
		for (ii=0; ii<cobj->elemarsize; ++ii)
		{
			Py_XDECREF( cobj->elemar[ii].pyobj);
		}
		Py_TYPE(selfobj)->tp_free( selfobj);
	}
	else
	{
		papuga_python_error( "%s", papuga_ErrorCode_tostring( papuga_InvalidAccess));
	}
}

DLL_PUBLIC bool papuga_python_set_CallArgs( papuga_CallArgs* as, PyObject* args, const char** kwargnames, const papuga_python_ClassEntryMap* cemap)
{
	if (args == NULL) return true;

#ifdef PAPUGA_LOWLEVEL_DEBUG
	if (!checkCircular( args))
	{
		fprintf( stderr, "circular reference in call arguments\n");
		as->errcode = papuga_LogicError;
		goto ERROR;
	}
	printPyObject( stderr, "call arguments", args, -1);
#endif
	if (PyDict_Check( args))
	{
		Py_ssize_t ai = 0, ae = PyDict_Size( args);
		PyObject* argitem;

		if (ae > papuga_MAX_NOF_ARGUMENTS)
		{
			as->errcode = papuga_NofArgsError;
			goto ERROR;
		}
		for (; kwargnames[ ai]; ++ai)
		{
			if (as->argc >= papuga_MAX_NOF_ARGUMENTS)
			{
				as->errcode = papuga_NofArgsError;
				goto ERROR;
			}
			argitem = PyDict_GetItemString( args, kwargnames[ ai]);
			if (argitem)
			{
				if (!init_ValueVariant_pyobj( &as->argv[ ai], &as->allocator, argitem, cemap, &as->errcode))
				{
					as->erridx = ai;
					goto ERROR;
				}
			}
			else
			{
				papuga_init_ValueVariant( &as->argv[ ai]);
			}
#ifdef PAPUGA_LOWLEVEL_DEBUG
			printValueVariant( stderr, "call argument", &as->argv[ ai]);
#endif
			++as->argc;
		}
	}
	else if (PyTuple_Check( args))
	{
		Py_ssize_t ai = 0, ae = PyTuple_GET_SIZE( args);
		if (ae > papuga_MAX_NOF_ARGUMENTS)
		{
			as->errcode = papuga_NofArgsError;
			goto ERROR;
		}
		for (; ai < ae; ++ai)
		{
			PyObject* argitem = PyTuple_GetItem( args, ai);
			if (argitem)
			{
				if (!init_ValueVariant_pyobj( &as->argv[ ai], &as->allocator, argitem, cemap, &as->errcode))
				{
					as->erridx = ai;
					goto ERROR;
				}
			}
			else
			{
				papuga_init_ValueVariant( &as->argv[ ai]);
			}
			++as->argc;
		}
	}
	else
	{
		as->errcode = papuga_TypeError;
		goto ERROR;
	}
	return true;
ERROR:
	papuga_destroy_CallArgs( as);
	return false;
}

DLL_PUBLIC PyObject* papuga_python_move_CallResult( papuga_CallResult* retval, const papuga_python_ClassEntryMap* cemap, papuga_ErrorCode* errcode)
{
	PyObject* rt = 0;
	PyObject* ar[ papuga_MAX_NOF_RETURNS];
	int ai = 0, ae = retval->nofvalues;
	for (;ai != ae; ++ai)
	{
#ifdef PAPUGA_LOWLEVEL_DEBUG
		printValueVariant( stderr, "call result", &retval->valuear[ ai]);
#endif
		ar[ai] = createPyObjectFromVariant( &retval->allocator, &retval->valuear[ai], cemap, errcode);
		if (!ar[ai])
		{
			goto ERROR;
		}
#ifdef PAPUGA_LOWLEVEL_DEBUG
		fprintf( stderr, "call result %d:\n", (int)ai);
		if (!checkCircular( ar[ai]))
		{
			fprintf( stderr, "circular reference in call result %d\n", (int)ai);
			*errcode = papuga_LogicError;
			goto ERROR;
		}
		if (!checkReferenceCount( ar[ai]))
		{
			fprintf( stderr, "reference counting bad in call result %d\n", (int)ai);
			*errcode = papuga_LogicError;
			goto ERROR;
		}
		printPyObject( stderr, "value", ar[ai], -1);
#endif
	}
	if (retval->nofvalues > 1)
	{
		rt = papuga_python_create_tuple( ar, retval->nofvalues, errcode);
	}
	else if (retval->nofvalues == 1)
	{
		rt = ar[ 0];
	}
	else
	{
		Py_INCREF( Py_None);
		rt = Py_None;
	}
	papuga_destroy_CallResult( retval);
	return rt;
ERROR:
	papuga_destroy_CallResult( retval);
	for (; ai>=0; --ai) Py_XDECREF( ar[ai]);
	return NULL;
}

DLL_PUBLIC void papuga_python_error( const char* msg, ...)
{
	char buf[ 2048];
	va_list ap;
	va_start(ap, msg);
	
	vsnprintf( buf, sizeof(buf), msg, ap);
	va_end(ap);

	PyErr_SetString( PyExc_RuntimeError, buf);
}


