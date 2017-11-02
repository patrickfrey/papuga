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
	if (classid <= 0 && classid > cemap->hoarsize) return 0;
	return cemap->hoar[ classid-1];
}

static PyTypeObject* getTypeStruct( const papuga_python_ClassEntryMap* cemap, int structid)
{
	if (structid <= 0 && structid > cemap->soarsize) return 0;
	return cemap->soar[ structid-1];
}

#ifdef PAPUGA_LOWLEVEL_DEBUG
#if 0
static void log_ValueVariant( const char* msg, const papuga_ValueVariant* val)
{
	char buf[ 256];
	char const* str;
	size_t len;
	papuga_Allocator allocator;
	papuga_ErrorCode errcode = papuga_Ok;
	char allocbuf[ 1024];
	papuga_init_Allocator( &allocator, allocbuf, sizeof(allocbuf));

	str = papuga_ValueVariant_tostring( val, &allocator, &len, &errcode);
	if (str)
	{
		if (len > sizeof(buf)) len = sizeof(buf)-1;
		memcpy( buf, str, len);
		buf[ len] = 0;
		str = buf;
	}
	else
	{
		str = "<not printable>";
	}
	fprintf( stdout, "%s '%s'\n", msg, str);
}
#endif
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
	if (pytype->tp_basicsize != sizeof(papuga_python_ClassObject)) return NULL;
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
				if (memobj)
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
		papuga_Serialization* ser = 0;
		if (*errcode != papuga_Ok) return false;
		ser = papuga_Allocator_alloc_Serialization( allocator);
		if (!ser) goto ERROR_NOMEM;
	
		papuga_init_ValueVariant_serialization( value, ser);
		if (!serialize_struct( ser, allocator, pyobj, cemap, errcode)) return false;

#ifdef PAPUGA_LOWLEVEL_DEBUG
		char* str = papuga_Serialization_tostring( ser);
		if (ser)
		{
			fprintf( stdout, "SERIALIZE STRUCT:\n%s\n", str);
			free( str);
		}
#endif
		return true;
	}
ERROR_NOMEM:
	*errcode = papuga_NoMemError;
	return false;
}

static PyObject* papuga_Iterator_iter( PyObject *selfobj)
{
	Py_INCREF(selfobj);
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
	char errbuf[ 2048];

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
	papuga_init_CallResult( &result, errbuf, sizeof( errbuf));
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

typedef struct papuga_PyStructNode
{
	PyObject* keyobj;
	PyObject* valobj;
} papuga_PyStructNode;

void papuga_init_PyStructNode( papuga_PyStructNode* nd)
{
	nd->keyobj = NULL;
	nd->valobj = NULL;
}

void papuga_destroy_PyStructNode( papuga_PyStructNode* nd)
{
	Py_XDECREF( nd->keyobj);
	Py_XDECREF( nd->valobj);
}

void papuga_PyStructNode_set_key( papuga_PyStructNode* nd, PyObject* key)
{
	Py_XINCREF( nd->keyobj = key);
}

void papuga_PyStructNode_set_value( papuga_PyStructNode* nd, PyObject* val)
{
	Py_XINCREF( nd->valobj = val);
}

typedef struct papuga_PyStruct
{
	papuga_Stack stk;
	char stk_local_mem[ 2048];
	int nofKeyValuePairs;
} papuga_PyStruct;

static void papuga_init_PyStruct( papuga_PyStruct* pystruct)
{
	pystruct->nofKeyValuePairs = 0;
	papuga_init_Stack( &pystruct->stk, sizeof(papuga_PyStructNode), pystruct->stk_local_mem, sizeof(pystruct->stk_local_mem));
}

static void papuga_destroy_PyStruct( papuga_PyStruct* pystruct)
{
	unsigned int ei = 0, ee = papuga_Stack_size( &pystruct->stk);
	for (; ei != ee; ++ei)
	{
		papuga_PyStructNode* nd = (papuga_PyStructNode*)papuga_Stack_element( &pystruct->stk, ei);
		papuga_destroy_PyStructNode( nd);
	}
	papuga_destroy_Stack( &pystruct->stk);
}

static bool papuga_PyStruct_push_node( papuga_PyStruct* pystruct, papuga_PyStructNode* nd)
{
	if (!papuga_Stack_push( &pystruct->stk, nd)) return false;
	papuga_init_PyStructNode( nd);
	return true;
}

static PyObject* papuga_PyStruct_create_dict( papuga_PyStruct* pystruct, papuga_ErrorCode* errcode)
{
	PyObject* rt = PyDict_New();
	if (rt)
	{
		long currIndex = 0;
		unsigned int ei = 0, ee = papuga_Stack_size( &pystruct->stk);
		for (; ei != ee; ++ei)
		{
			papuga_PyStructNode* nd = (papuga_PyStructNode*)papuga_Stack_element( &pystruct->stk, ei);
			if (!nd) break;
			if (!nd->keyobj)
			{
				nd->keyobj = PyLong_FromLong( currIndex++);
				if (nd->keyobj == 0)
				{
					*errcode = papuga_NoMemError;
					break;
				}
			}
			else if (PyLong_Check( nd->keyobj))
			{
				currIndex = PyLong_AsLong( nd->keyobj)+1;
				if (currIndex == 0)
				{
					*errcode = papuga_OutOfRangeError;
					break;
				}
			}
			if (0>PyDict_SetItem( rt, nd->keyobj, nd->valobj))
			{
				*errcode = papuga_NoMemError;
				break;
			}
#ifdef PAPUGA_LOWLEVEL_DEBUG
			if (!checkCircular( rt))
			{
				fprintf( stderr, "circular reference in list created from papuga_pyStruct");
				PyDict_Clear( rt);
				*errcode = papuga_LogicError;
				return NULL;
			}
#endif
		}
		if (ei != ee)
		{
			PyDict_Clear( rt);
			rt = NULL;
		}
	}
#ifdef PAPUGA_LOWLEVEL_DEBUG
	if (!checkCircular( rt))
	{
		fprintf( stderr, "circular reference in dictionary created from papuga_pyStruct");
		PyDict_Clear( rt);
		*errcode = papuga_LogicError;
		return NULL;
	}
#endif
	return rt;
}

static PyObject* papuga_PyStruct_create_list( papuga_PyStruct* pystruct, papuga_ErrorCode* errcode)
{
	PyObject* rt = PyList_New( papuga_Stack_size( &pystruct->stk));
	if (rt)
	{
		unsigned int ei = 0, ee = papuga_Stack_size( &pystruct->stk);
		for (; ei != ee; ++ei)
		{
			papuga_PyStructNode* nd = (papuga_PyStructNode*)papuga_Stack_element( &pystruct->stk, ei);
			if (0>PyList_SetItem( rt, ei, nd->valobj))
			{
				*errcode = papuga_NoMemError;
				break;
			}
		}
		if (ei != ee)
		{
			rt = NULL;
		}
	}
#ifdef PAPUGA_LOWLEVEL_DEBUG
	if (!checkCircular( rt))
	{
		fprintf( stderr, "circular reference in list created from papuga_pyStruct");
		PyDict_Clear( rt);
		*errcode = papuga_LogicError;
		return NULL;
	}
#endif
	return rt;
}

static PyObject* papuga_PyStruct_create_object( papuga_PyStruct* pystruct, papuga_ErrorCode* errcode)
{
	PyObject* rt = NULL;
	if (pystruct->nofKeyValuePairs > 0)
	{
		rt = papuga_PyStruct_create_dict( pystruct, errcode);
	}
	else
	{
		rt = papuga_PyStruct_create_list( pystruct, errcode);
	}
	return rt;
}

static bool papuga_init_PyStruct_serialization( papuga_PyStruct* pystruct, papuga_Allocator* allocator, papuga_SerializationIter* seriter, const papuga_python_ClassEntryMap* cemap, papuga_ErrorCode* errcode)
{
	papuga_PyStructNode nd;
	papuga_init_PyStruct( pystruct);
	papuga_init_PyStructNode( &nd);

	for (; papuga_SerializationIter_tag(seriter) != papuga_TagClose; papuga_SerializationIter_skip(seriter))
	{
		if (papuga_SerializationIter_tag(seriter) == papuga_TagName)
		{
			if (nd.keyobj || !papuga_ValueVariant_isatomic( papuga_SerializationIter_value(seriter)))
			{
				*errcode = papuga_TypeError;
				goto ERROR;
			}
			papuga_PyStructNode_set_key( &nd, createPyObjectFromVariant( allocator, papuga_SerializationIter_value(seriter), cemap, errcode));
			if (!nd.keyobj) goto ERROR;

			++pystruct->nofKeyValuePairs;
		}
		else if (papuga_SerializationIter_tag(seriter) == papuga_TagValue)
		{
			papuga_PyStructNode_set_value( &nd, createPyObjectFromVariant( allocator, papuga_SerializationIter_value(seriter), cemap, errcode));
			if (!nd.valobj) goto ERROR;

			if (!papuga_PyStruct_push_node( pystruct, &nd))
			{
				*errcode = papuga_NoMemError;
				goto ERROR;
			}
		}
		else if (papuga_SerializationIter_tag(seriter) == papuga_TagOpen)
		{
			papuga_PyStruct subpystruct;

			papuga_SerializationIter_skip(seriter);
			if (!papuga_init_PyStruct_serialization( &subpystruct, allocator, seriter, cemap, errcode))
			{
				goto ERROR;
			}
			papuga_PyStructNode_set_value( &nd, papuga_PyStruct_create_object( &subpystruct, errcode));
			if (!nd.valobj) goto ERROR;

			if (!papuga_PyStruct_push_node( pystruct, &nd))
			{
				papuga_destroy_PyStruct( &subpystruct);
				*errcode = papuga_NoMemError;
				goto ERROR;
			}
			papuga_destroy_PyStruct( &subpystruct);
			if (papuga_SerializationIter_eof(seriter))
			{
				*errcode = papuga_UnexpectedEof;
				goto ERROR;
			}
		}
	}
	if (nd.keyobj)
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
				if (!str)
				{
					return NULL;
				}
				else
				{
					rt = PyUnicode_FromStringAndSize( str, len);
				}
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
			papuga_init_SerializationIter( &seriter, value->value.serialization);
#ifdef PAPUGA_LOWLEVEL_DEBUG
			{
				char* serstr = papuga_Serialization_tostring( value->value.serialization);
				if (serstr)
				{
					fprintf( stderr, "create structure from serialization:\n%s\n", serstr);
					free( serstr);
				}
			}
#endif
			if (!papuga_init_PyStruct_serialization( &pystruct, allocator, &seriter, cemap, errcode))
			{
				papuga_destroy_PyStruct( &pystruct);
				break;
			}
			rt = papuga_PyStruct_create_object( &pystruct, errcode);
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
#ifdef PAPUGA_LOWLEVEL_DEBUG
	if (!checkCircular( rt))
	{
		fprintf( stderr, "circular reference in created object");
		PyDict_Clear( rt);
		*errcode = papuga_LogicError;
		return NULL;
	}
#endif
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
			if (0>PyTuple_SetItem( rt, ei, ar[ei]))
			{
				*errcode = papuga_NoMemError;
				break;
			}
		}
		if (ei != ee)
		{
			_PyTuple_Resize( &rt, 0);
			rt = NULL;
		}
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

DLL_PUBLIC void papuga_python_init_struct( PyObject* selfobj, int structid)
{
	papuga_python_StructObject* cobj = (papuga_python_StructObject*)selfobj;
	cobj->structid = structid;
	cobj->elemarsize = (Py_TYPE(selfobj)->tp_basicsize - sizeof(papuga_python_StructObject)) / sizeof(papuga_python_StructObjectElement);
	cobj->checksum = calcStructObjectCheckSum( cobj);
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

DLL_PUBLIC PyObject* papuga_python_create_struct( int structid, const papuga_python_ClassEntryMap* cemap, papuga_ErrorCode* errcode)
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
		for (int ii=0; ii<cobj->elemarsize; ++ii)
		{
			Py_XDECREF( cobj->elemar[ii].pyobjref);
		}
		Py_TYPE(selfobj)->tp_free( selfobj);
	}
	else
	{
		papuga_python_error( "%s", papuga_ErrorCode_tostring( papuga_InvalidAccess));
	}
}

DLL_PUBLIC bool papuga_python_init_CallArgs( papuga_CallArgs* as, PyObject* args, const char** kwargnames, const papuga_python_ClassEntryMap* cemap)
{
	papuga_init_CallArgs( as);

	if (PyDict_Check( args))
	{
		int argi;
		Py_ssize_t argcnt = 0;
		PyObject* argitem;

		for (argi=0; kwargnames[ argi]; ++argi)
		{
			if (argi > papuga_MAX_NOF_ARGUMENTS)
			{
				papuga_destroy_CallArgs( as);
				as->errcode = papuga_NofArgsError;
				return false;
			}
			argitem = PyDict_GetItemString( args, kwargnames[argi]);
			if (argitem)
			{
				if (!init_ValueVariant_pyobj( &as->argv[ argi], &as->allocator, argitem, cemap, &as->errcode))
				{
					as->erridx = argi;
					papuga_destroy_CallArgs( as);
					return false;
				}
				++argcnt;
			}
			else
			{
				papuga_init_ValueVariant( &as->argv[ argi]);
			}
			++as->argc;
		}
		if (argcnt != PyDict_Size( args))
		{
			as->errcode = papuga_NofArgsError;
			return false;
		}
	}
	else if (PyTuple_Check( args))
	{
		Py_ssize_t ai = 0, ae = PyTuple_GET_SIZE( args);
		if (ae > papuga_MAX_NOF_ARGUMENTS)
		{
			papuga_destroy_CallArgs( as);
			as->errcode = papuga_NofArgsError;
			return false;
		}
		for (; ai < ae; ++ai)
		{
			PyObject* argitem = PyTuple_GET_ITEM( args, ai);
			if (!init_ValueVariant_pyobj( &as->argv[ ai], &as->allocator, argitem, cemap, &as->errcode))
			{
				as->erridx = ai;
				papuga_destroy_CallArgs( as);
				return false;
			}
			++as->argc;
		}
	}
	else
	{
		as->errcode = papuga_TypeError;
		return false;
	}
	return true;
}

DLL_PUBLIC PyObject* papuga_python_move_CallResult( papuga_CallResult* retval, const papuga_python_ClassEntryMap* cemap, papuga_ErrorCode* errcode)
{
	PyObject* rt = 0;
	PyObject* ar[ papuga_MAX_NOF_RETURNS];
	size_t ai = 0, ae = retval->nofvalues;
	for (;ai != ae; ++ai)
	{
		ar[ai] = createPyObjectFromVariant( &retval->allocator, &retval->valuear[ai], cemap, errcode);
		if (!ar[ai])
		{
			papuga_destroy_CallResult( retval);
			return NULL;
		}
	}
	if (ai > 1)
	{
		rt = papuga_python_create_tuple( ar, ae, errcode);
	}
	else if (ai == 1)
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


