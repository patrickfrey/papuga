/*
* Copyright (c) 2017 Patrick P. Frey
*
* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/
/// \brief Library implementation for Python (v3) bindings built by papuga
/// \file libpapuga_python3_dev.c

#include "papuga/lib/python3_dev.h"
#include "papuga/valueVariant.h"
#include "papuga/callResult.h"
#include "papuga/errors.h"
#include "papuga/serialization.h"
#include "papuga/hostObject.h"
#include "papuga/iterator.h"
#include "papuga/stack.h"
#include "papuga/hostObject.h"
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
#include <Python.h>

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
	if (classid <= 0 && classid > cemap->size) return 0;
	return cemap->ar[ classid-1];
}

#define KNUTH_HASH 2654435761U
static int calcObjectCheckSum( papuga_python_ClassObject* cobj)
{
	return (cobj->classid * KNUTH_HASH) + ((uintptr_t)cobj->self % 0xFfFf ^ ((uintptr_t)cobj->self >> 16));
}
static int calcIteratorCheckSum( const papuga_python_IteratorObject* iobj)
{
	return (int)(unsigned int)(((uintptr_t)iobj->impl.data+107) * KNUTH_HASH) ^ (uintptr_t)iobj->impl.destroy ^ ((uintptr_t)iobj->impl.getNext << 6) ^ ((uintptr_t)(iobj->cemap) << 3);
}

static bool isLittleEndian()
{
	union
	{
		char data[2];
		short val;
	} u;
	u.val = 1;
	return (bool)u.data[0];
}

static papuga_python_ClassObject* getClassObject( PyObject* pyobj, const papuga_python_ClassEntryMap* cemap, papuga_ErrorCode* errcode)
{
	PyTypeObject* pytype = pyobj->ob_type;
	if (pytype->tp_basicsize != sizeof(papuga_python_ClassObject)) return NULL;
	papuga_python_ClassObject* cobj = (papuga_python_ClassObject*)pyobj;
	if (pytype != getTypeObject( cemap, cobj->classid));
	if (cobj->checksum != calcObjectCheckSum( cobj)) return NULL;
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
		if (0>PyUnicode_READY( pyobj))
		{
			*errcode = papuga_NoMemError;
			return false;
		}
		switch (PyUnicode_KIND( pyobj))
		{
			case PyUnicode_WCHAR_KIND:
				if (sizeof(wchar_t) == 2)
				{
					papuga_init_ValueVariant_langstring( value, papuga_UTF16, PyUnicode_2BYTE_DATA( pyobj), PyUnicode_GET_SIZE( pyobj));
				}
				else if (sizeof(wchar_t) == 4)
				{
					papuga_init_ValueVariant_langstring( value, papuga_UTF32, PyUnicode_4BYTE_DATA( pyobj), PyUnicode_GET_SIZE( pyobj));
				}
				else
				{
					*errcode = papuga_TypeError;
					return false;
				}
				break;
			case PyUnicode_1BYTE_KIND:/*UTF-8*/
				papuga_init_ValueVariant_charp( value, PyUnicode_1BYTE_DATA( pyobj));
				break;
			case PyUnicode_2BYTE_KIND:
				papuga_init_ValueVariant_langstring( value, papuga_UTF16, PyUnicode_2BYTE_DATA( pyobj), PyUnicode_GET_SIZE( pyobj));
				break;
			case PyUnicode_4BYTE_KIND:
				papuga_init_ValueVariant_langstring( value, papuga_UTF32, PyUnicode_4BYTE_DATA( pyobj), PyUnicode_GET_SIZE( pyobj));
				break;
		}
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

static bool serialize_element( papuga_Serialization* ser, papuga_Allocator* allocator, PyObject* pyobj, const papuga_python_ClassEntryMap* cemap, papuga_ErrorCode* errcode);
static bool serialize_struct( papuga_Serialization* ser, papuga_Allocator* allocator, PyObject* pyobj, const papuga_python_ClassEntryMap* cemap, papuga_ErrorCode* errcode)
{
	if (PyDict_Check( pyobj))
	{
		PyObject* keyitem;
		PyObject* valitem;
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
	else if (PySequence_Check( pyobj))
	{
		Py_ssize_t ii,len;
		PyObject* seqobj = PySequence_Fast( pyobj, papuga_ErrorCode_tostring( papuga_TypeError));
		if (!seqobj)
		{
			*errcode = papuga_TypeError;
			return false;
		}
		ii=0, len = PySequence_Size( seqobj);
		for (; ii<len; ++ii)
		{
			PyObject* item = PySequence_Fast_GET_ITEM( seqobj, ii);
			if (!serialize_element( ser, allocator, item, cemap, errcode)) return false;
		}
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
	else if (*errcode != papuga_Ok) return false;
	if (PyDict_Check( pyobj) || PySequence_Check( pyobj))
	{
		papuga_Serialization* ser = papuga_Allocator_alloc_Serialization( allocator);
		if (!ser)
		{
			*errcode = papuga_NoMemError;
			return false;
		}
		papuga_init_ValueVariant_serialization( value, ser);
		if (!papuga_Serialization_pushOpen( ser)) goto ERROR_NOMEM;		
		if (!serialize_struct( ser, allocator, pyobj, cemap, errcode)) return false;
		if (!papuga_Serialization_pushClose( ser)) goto ERROR_NOMEM;
#ifdef PAPUGA_LOWLEVEL_DEBUG
		char* str = papuga_Serialization_tostring( ser);
		if (ser)
		{
			fprintf( stderr, "SERIALIZE STRUCT:\n%s\n", str);
			free( str);
		}
#endif
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

static PyObject* createPyObjectFromVariant( papuga_Allocator* allocator, papuga_ValueVariant* value, const papuga_python_ClassEntryMap* cemap, papuga_ErrorCode* errcode);

typedef struct papuga_PyStructNode
{
	PyObject* keyobj;
	PyObject* valobj;
} papuga_PyStructNode;

void papuga_init_PyStructNode( papuga_PyStructNode* nd)
{
	nd->keyobj = 0;
	nd->valobj = 0;
}

typedef struct papuga_PyStruct
{
	papuga_Stack stk;
	char stk_local_mem[ 2048];
	int nofKeyValuePairs;
	int nofElements;
} papuga_PyStruct;

static void papuga_init_PyStruct( papuga_PyStruct* pystruct)
{
	pystruct->nofKeyValuePairs = 0;
	pystruct->nofElements = 0;
	papuga_init_Stack( &pystruct->stk, sizeof(papuga_PyStructNode), pystruct->stk_local_mem, sizeof(pystruct->stk_local_mem));
}

static void papuga_destroy_PyStruct( papuga_PyStruct* pystruct)
{
	papuga_destroy_Stack( &pystruct->stk);
}

static PyObject* papuga_PyStruct_create_dict( papuga_PyStruct* pystruct, papuga_ErrorCode* errcode)
{
	PyObject* rt = PyDict_New();
	if (rt)
	{
		long currIndex = 0;
		unsigned int ei = 0, ee = pystruct->nofElements;
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
		}
		if (ei != ee)
		{
			PyDict_Clear( rt);
			rt = NULL;
		}
	}
	return rt;
}

static PyObject* papuga_PyStruct_create_list( papuga_PyStruct* pystruct, papuga_ErrorCode* errcode)
{
	PyObject* rt = PyList_New( pystruct->nofElements);
	if (rt)
	{
		unsigned int ei = 0, ee = pystruct->nofElements;
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
	return rt;
}

static PyObject* papuga_PyStruct_create_tuple( papuga_PyStruct* pystruct, papuga_ErrorCode* errcode)
{
	PyObject* rt = PyTuple_New( pystruct->nofElements);
	if (rt)
	{
		unsigned int ei = 0, ee = pystruct->nofElements;
		for (; ei != ee; ++ei)
		{
			papuga_PyStructNode* nd = (papuga_PyStructNode*)papuga_Stack_element( &pystruct->stk, ei);
			if (0>PyTuple_SetItem( rt, ei, nd->valobj))
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

static bool papuga_init_PyStruct_serialization( papuga_PyStruct* pystruct, papuga_Allocator* allocator, papuga_Node** ni, const papuga_Node* ne, const papuga_python_ClassEntryMap* cemap, papuga_ErrorCode* errcode)
{
	papuga_PyStructNode nd;
	papuga_init_PyStruct( pystruct);
	papuga_init_PyStructNode( &nd);
	for (; *ni != ne && (*ni)->tag != papuga_TagClose; ++(*ni))
	{
		if ((*ni)->tag == papuga_TagName)
		{
			if (nd.keyobj || !papuga_ValueVariant_isatomic( &(*ni)->value))
			{
				*errcode = papuga_TypeError;
				goto ERROR;
			}
			nd.keyobj = createPyObjectFromVariant( allocator, &(*ni)->value, cemap, errcode);
			++pystruct->nofKeyValuePairs;
			if (!nd.keyobj) goto ERROR;
		}
		else if ((*ni)->tag == papuga_TagValue)
		{
			nd.valobj = createPyObjectFromVariant( allocator, &(*ni)->value, cemap, errcode);
			if (!nd.valobj) goto ERROR;
			if (!papuga_Stack_push( &pystruct->stk, &nd))
			{
				*errcode = papuga_TypeError;
				goto ERROR;
			}
			++pystruct->nofElements;
			papuga_init_PyStructNode( &nd);
		}
		else if ((*ni)->tag == papuga_TagOpen)
		{
			papuga_PyStruct subpystruct;

			++(*ni);
			if (!papuga_init_PyStruct_serialization( &subpystruct, allocator, ni, ne, cemap, errcode))
			{
				goto ERROR;
			}
			nd.valobj = papuga_PyStruct_create_object( &subpystruct, errcode);
			papuga_destroy_PyStruct( &subpystruct);
			if (!nd.valobj) goto ERROR;
			if (!papuga_Stack_push( &pystruct->stk, &nd))
			{
				*errcode = papuga_TypeError;
				goto ERROR;
			}
			++pystruct->nofElements;
			papuga_init_PyStructNode( &nd);

			if (*ni == ne)
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
	return false;
}

static PyObject* createPyObjectFromVariant( papuga_Allocator* allocator, papuga_ValueVariant* value, const papuga_python_ClassEntryMap* cemap, papuga_ErrorCode* errcode)
{
	PyObject* rt = NULL;
	switch (value->valuetype)
	{
		case papuga_TypeVoid:
			return Py_None;
		case papuga_TypeDouble:
			rt = PyFloat_FromDouble( value->value.Double);
			break;
		case papuga_TypeUInt:
			if (value->value.UInt > LONG_MAX)
			{
				*errcode = papuga_OutOfRangeError;
				return NULL;
			}
			rt = PyLong_FromLong( value->value.UInt);
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
			return (value->value.Bool)? Py_True : Py_False;
		case papuga_TypeString:
		{
			rt = PyUnicode_FromStringAndSize( value->value.string, value->length);
			break;
		}
		case papuga_TypeLangString:
		{
			if (isLittleEndian())
			{
				switch ((papuga_StringEncoding)value->encoding)
				{
					case papuga_UTF8:
						rt = PyUnicode_FromStringAndSize( value->value.langstring, value->length);
						break;
					case papuga_UTF16BE:
					case papuga_UTF32BE:
					{
						size_t len;
						const char* str = papuga_ValueVariant_tostring( value, allocator, &len, errcode);
						if (!str) return false;
						rt = PyUnicode_FromStringAndSize( str, len);
						break;
					}
					case papuga_UTF16LE:
					case papuga_UTF16:
						rt = PyUnicode_FromKindAndData( PyUnicode_2BYTE_KIND, value->value.langstring, value->length);
						break;
					case papuga_UTF32LE:
					case papuga_UTF32:
						rt = PyUnicode_FromKindAndData( PyUnicode_4BYTE_KIND, value->value.langstring, value->length);
						break;
					case papuga_Binary:
						rt = PyBytes_FromStringAndSize( value->value.langstring, value->length);
						break;
				}
			}
			else
			{
				switch ((papuga_StringEncoding)value->encoding)
				{
					case papuga_UTF8:
						rt = PyUnicode_FromStringAndSize( value->value.string, value->length);
						break;
					case papuga_UTF16LE:
					case papuga_UTF32LE:
					{
						size_t len;
						const char* str = papuga_ValueVariant_tostring( value, allocator, &len, errcode);
						if (!str) return false;
						rt = PyUnicode_FromStringAndSize( str, len);
						break;
					}
					case papuga_UTF16BE:
					case papuga_UTF16:
						rt = PyUnicode_FromKindAndData( PyUnicode_2BYTE_KIND, value->value.langstring, value->length);
						break;
					case papuga_UTF32BE:
					case papuga_UTF32:
						rt = PyUnicode_FromKindAndData( PyUnicode_4BYTE_KIND, value->value.langstring, value->length);
						break;
					case papuga_Binary:
						rt = PyBytes_FromStringAndSize( value->value.langstring, value->length);
						break;
					default:
						*errcode = papuga_NotImplemented;
						return NULL;
				}
			}
			break;
		}
		case papuga_TypeHostObject:
		{
			papuga_HostObject* hobj = value->value.hostObject;
			rt = papuga_python_create_object( hobj->data, hobj->classid, cemap, errcode);
			break;
		}
		case papuga_TypeSerialization:
		{
			papuga_PyStruct pystruct;
			papuga_Node* ni = value->value.serialization->ar;
			papuga_Node* ne = ni + value->value.serialization->arsize;
			if (ni == ne)
			{
				return Py_None;
			}
			if (!papuga_init_PyStruct_serialization( &pystruct, allocator, &ni, ne, cemap, errcode))
			{
				break;
			}
			if (pystruct.nofKeyValuePairs == 0)
			{
				if (pystruct.nofElements == 1)
				{
					papuga_PyStructNode* nd = (papuga_PyStructNode*)papuga_Stack_element( &pystruct.stk, 0);
					rt = nd->valobj;
				}
				else
				{
					rt = papuga_PyStruct_create_tuple( &pystruct, errcode);
				}
			}
			else
			{
				rt = papuga_PyStruct_create_dict( &pystruct, errcode);
			}
			papuga_destroy_PyStruct( &pystruct);
			break;
		}
		case papuga_TypeIterator:
		{
			papuga_Iterator* itr = value->value.iterator;
			rt = createPyObjectFromIterator( itr, cemap, errcode);
			if (rt)
			{
				papuga_release_Iterator( itr);
			}
			else
			{
				return NULL;
			}
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



DLL_PUBLIC int papuga_python_init(void)
{
	if (PyType_Ready(&g_papuga_IteratorType) < 0) return -1;
	Py_INCREF( &g_papuga_IteratorType);
	return 0;
}

DLL_PUBLIC void papuga_python_init_object( PyObject* selfobj, int classid, void* self)
{
	papuga_python_ClassObject* cobj = (papuga_python_ClassObject*)selfobj;
	cobj->classid = classid;
	cobj->self = self;
	cobj->checksum = calcObjectCheckSum( cobj);
}

DLL_PUBLIC PyObject* papuga_python_create_object( void* self, int classid, const papuga_python_ClassEntryMap* cemap, papuga_ErrorCode* errcode)
{
	PyTypeObject* typeobj = getTypeObject( cemap, classid);
	if (!typeobj)
	{
		*errcode = papuga_InvalidAccess;
		return NULL;
	}
	PyObject* selfobj = PyType_GenericAlloc( typeobj, 1);
	if (!selfobj)
	{
		*errcode = papuga_NoMemError;
		return NULL;
	}
	papuga_python_init_object( selfobj, classid, self);
	return selfobj;
}

DLL_PUBLIC bool papuga_python_init_CallArgs( papuga_python_CallArgs* as, PyObject* args, const char** kwargnames, const papuga_python_ClassEntryMap* cemap)
{
	as->erridx = -1;
	as->errcode = papuga_Ok;
	as->argc = 0;

	papuga_init_Allocator( &as->allocator, as->allocbuf, sizeof( as->allocbuf));

	if (PyDict_Check( args))
	{
		int argi;
		Py_ssize_t argcnt = 0;

		for (argi=0; kwargnames[ argi]; ++argi)
		{
			if (argi > papuga_PYTHON_MAX_NOF_ARGUMENTS)
			{
				papuga_python_destroy_CallArgs( as);
				as->errcode = papuga_NofArgsError;
				return false;
			}
			PyObject* argitem = PyDict_GetItemString( args, kwargnames[argi]);
			if (argitem)
			{
				if (!init_ValueVariant_pyobj( &as->argv[ argi], &as->allocator, argitem, cemap, &as->errcode))
				{
					as->erridx = argi;
					papuga_python_destroy_CallArgs( as);
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
		if (ae > papuga_PYTHON_MAX_NOF_ARGUMENTS)
		{
			papuga_python_destroy_CallArgs( as);
			as->errcode = papuga_NofArgsError;
			return false;
		}
		for (; ai < ae; ++ai)
		{
			PyObject* argitem = PyTuple_GET_ITEM( args, ai);
			if (!init_ValueVariant_pyobj( &as->argv[ ai], &as->allocator, argitem, cemap, &as->errcode))
			{
				as->erridx = ai;
				papuga_python_destroy_CallArgs( as);
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

DLL_PUBLIC void papuga_python_destroy_CallArgs( papuga_python_CallArgs* argstruct)
{
	papuga_destroy_Allocator( &argstruct->allocator);
}

DLL_PUBLIC PyObject* papuga_python_move_CallResult( papuga_CallResult* retval, const papuga_python_ClassEntryMap* cemap, papuga_ErrorCode* errcode)
{
	PyObject* rt = createPyObjectFromVariant( &retval->allocator, &retval->value, cemap, errcode);
	papuga_destroy_CallResult( retval);
	Py_INCREF( rt);
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

