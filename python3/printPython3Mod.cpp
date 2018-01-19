/*
* Copyright (c) 2017 Patrick P. Frey
*
* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/
/// \brief Module for printing the Python (v3) module C source
/// \file printPython3Mod.cpp
#include "printPython3Mod.hpp"
#include "private/dll_tags.h"
#include "private/gen_utils.hpp"
#include "papuga/lib/python3_dev.h"
#include "fmt/format.h"
#include <string>
#include <algorithm>
#include <cstdio>
#include <cstdarg>
#include <stdexcept>
#include <sstream>

using namespace papuga;

static void define_method(
		std::ostream& out,
		const papuga_InterfaceDescription& descr,
		const papuga_ClassDescription& classdef,
		const papuga_MethodDescription& method)
{
	out << "static const char* g_paramname_" << classdef.name << "__" << method.name << "[] = {";
	const papuga_ParameterDescription* pi = method.parameter;
	for (; pi->name; ++pi)
	{
		out << "\"" << pi->name << "\",";
	}
	out << "NULL};" << std::endl << std::endl;

	out << fmt::format( papuga::cppCodeSnippet( 0,
		"static PyObject* {classname}__{methodname}(PyObject* selfobj, PyObject* args)",
		"{",
			"PyObject* rt;",
			"void* self = ((papuga_python_ClassObject*)selfobj)->self;",
			"papuga_CallArgs argstruct;",
			"papuga_Allocator allocator;",
			"papuga_CallResult retstruct;",
			"papuga_ErrorCode errcode = papuga_Ok;",
			"const char* msg;",
			"char membuf_args[ 4096];",
			"char membuf_retv[ 4096];",
			"char membuf_err[ 256];",
			"",
			"papuga_init_CallArgs( &argstruct, membuf_args, sizeof(membuf_args));",
			"if (!papuga_python_set_CallArgs( &argstruct, args, g_paramname_{classname}__{methodname}, &g_class_entry_map))",
			"{",
				"papuga_destroy_CallArgs( &argstruct);",
				"papuga_python_error( \"error in '%s': %s\", \"{classname}->{methodname}\", papuga_ErrorCode_tostring( argstruct.errcode));",
				"return NULL;",
			"}",
			"papuga_init_Allocator( &allocator, membuf_retv, sizeof(membuf_retv));",
			"papuga_init_CallResult( &retstruct, &allocator, true, membuf_err, sizeof(membuf_err));",
			"if (!{funcname}( self, &retstruct, argstruct.argc, argstruct.argv))",
			"{",
				"msg = papuga_CallResult_lastError( &retstruct);",
				"papuga_destroy_CallArgs( &argstruct);",
				"papuga_destroy_CallResult( &retstruct);",
				"papuga_python_error( \"error in '%s': %s\", \"{classname}->{methodname}\", msg);",
				"return NULL;",
			"}",
			"papuga_destroy_CallArgs( &argstruct);",
			"rt = papuga_python_move_CallResult( &retstruct, &g_class_entry_map, &errcode);",
			"if (!rt)",
			"{",
				"papuga_python_error( \"error in '%s': %s\", \"{classname}->{methodname}\", papuga_ErrorCode_tostring( errcode));",
			"}",
			"return rt;",
		"}",
		0),
			fmt::arg("methodname", method.name),
			fmt::arg("classname", classdef.name),
			fmt::arg("funcname", method.funcname)
		) << std::endl;
}

static void define_constructor(
		std::ostream& out,
		int classid,
		const papuga_InterfaceDescription& descr,
		const papuga_ClassDescription& classdef)
{
	out << "static const char* g_paramname_constructor__" << classdef.name << "[] = {";
	const papuga_ParameterDescription* pi = classdef.constructor->parameter;
	for (; pi->name; ++pi)
	{
		out << "\"" << pi->name << "\",";
	}
	out << "NULL};" << std::endl << std::endl;

	out << fmt::format( papuga::cppCodeSnippet( 0,
		"static int init__{classname}( PyObject* selfobj, PyObject* args, PyObject *kwargs)",
		"{",
			"void* self;",
			"papuga_CallArgs argstruct;",
			"papuga_ErrorBuffer errbuf;",
			"const char* msg;",
			"char membuf_args[ 4096];",
			"char membuf_err[ 4096];",
			"",
			"papuga_init_CallArgs( &argstruct, membuf_args, sizeof(membuf_args));",
			"if (!papuga_python_set_CallArgs( &argstruct, args ? args:kwargs, g_paramname_constructor__{classname}, &g_class_entry_map))",
			"{",
				"papuga_destroy_CallArgs( &argstruct);",
				"papuga_python_error( \"error in constructor of '%s': %s\", \"{classname}\", papuga_ErrorCode_tostring( argstruct.errcode));",
				"return -1;",
			"}",
			"papuga_init_ErrorBuffer( &errbuf, membuf_err, sizeof(membuf_err));",
			"self = {constructor}( &errbuf, argstruct.argc, argstruct.argv);",
			"if (!self)",
			"{",
				"msg = papuga_ErrorBuffer_lastError( &errbuf);",
				"papuga_destroy_CallArgs( &argstruct);",
				"papuga_python_error( \"error in constructor of '%s': %s\", \"{classname}\", msg);",
				"return -1;",
			"}",
			"papuga_python_init_object( selfobj, self, {classid}, {destructor});",
			"papuga_destroy_CallArgs( &argstruct);",
			"return 0;",
		"}",
		0),
			fmt::arg("classname", classdef.name),
			fmt::arg("classid", classid),
			fmt::arg("constructor", classdef.constructor->funcname),
			fmt::arg("destructor", classdef.funcname_destructor)
		) << std::endl;
}

static std::string getAnnotationText(
		const papuga_Annotation* ann,
		const papuga_AnnotationType type)
{
	papuga_Annotation const* di = ann;
	if (di) for (; di->text; ++di)
	{
		if (di->type == type)
		{
			std::string rt;
			char const* si = di->text;
			for (;*si; ++si)
			{
				if ((unsigned int)*si <= 32)
				{
					if (!rt.empty() && rt[ rt.size()-1] == ' ') continue;
					rt.push_back( ' ');
				}
				else
				{
					rt.push_back( *si);
				}
			}
			return rt;
		}
	}
	return "";
}

static void define_methodtable(
		std::ostream& out,
		const papuga_InterfaceDescription& descr,
		const papuga_ClassDescription& classdef)
{
	out << fmt::format( papuga::cppCodeSnippet( 0,
		"static PyMethodDef g_methods_{classname}[] =",
		"{",
		0),
			fmt::arg("classname", classdef.name)
		);
	papuga_MethodDescription const* mi = classdef.methodtable;
	for (; mi->name; ++mi)
	{
		std::string description = getAnnotationText( mi->doc, papuga_AnnotationType_Description);
		out << fmt::format( papuga::cppCodeSnippet( 1,
			"{{\"{methodname}\", &{classname}__{methodname}, METH_VARARGS|METH_KEYWORDS, \"{description}\"}},",
			0),
				fmt::arg("classname", classdef.name),
				fmt::arg("methodname", mi->name),
				fmt::arg("description", description)
			);
	}
	out << fmt::format( papuga::cppCodeSnippet( 1,
			"{{NULL, NULL, 0, NULL}}",
			"};",
			0)) << std::endl;
}

static void define_class(
		std::ostream& out,
		const papuga_InterfaceDescription& descr,
		const papuga_ClassDescription& classdef)
{
	const std::string constructor_name =
		classdef.constructor ? (std::string("init__") + classdef.name):"NULL";

	out << fmt::format( papuga::cppCodeSnippet( 0,
		"static PyTypeObject g_typeobject_{classname} =",
		"{",
		"PyVarObject_HEAD_INIT(&PyType_Type, 0)",
		"\"{classname}\",		/* tp_name */",
		"sizeof(papuga_python_ClassObject), /* tp_basicsize */",
		"1,				/* tp_itemsize */",
		"papuga_python_destroy_object,	/* tp_dealloc */",
		"0,				/* tp_print */",
		"0,				/* tp_getattr */",
		"0,				/* tp_setattr */",
		"0,				/* tp_reserved */",
		"0,				/* tp_repr */",
		"0,				/* tp_as_number */",
		"0,				/* tp_as_sequence */",
		"0,				/* tp_as_mapping */",
		"0,				/* tp_hash */",
		"0,				/* tp_call */",
		"0,				/* tp_str */",
		"0,				/* tp_getattro */",
		"0,				/* tp_setattro */",
		"0,				/* tp_as_buffer */",
		"Py_TPFLAGS_DEFAULT,		/* tp_flags */",
		"\"{doc}\",	/* tp_doc */",
		"0,				/* tp_traverse */",
		"0,				/* tp_clear */",
		"0,				/* tp_richcompare */",
		"0,				/* tp_weaklistoffset */",
		"0,				/* tp_iter */",
		"0,				/* tp_iternext */",
		"g_methods_{classname},		/* tp_methods */",
		"0,				/* tp_members */",
		"0,				/* tp_getset */",
		"0,				/* tp_base */",
		"0,				/* tp_dict */",
		"0,				/* tp_descr_get */",
		"0,				/* tp_descr_set */",
		"0,				/* tp_dictoffset */",
		"{constructor},			/* tp_init */",
		"PyType_GenericAlloc,		/* tp_alloc */",
		"PyType_GenericNew,		/* tp_new */",
		"0,				/* tp_free */",
		"0,				/* tp_is_gc */",
		"0,				/* tp_bases */",
		"0,				/* tp_mro */",
		"0,				/* tp_cache */",
		"0,				/* tp_subclasses */",
		"0,				/* tp_weaklist */",
		"0,				/* tp_del */",
		"0,				/* tp_version_tag */",
		"0				/* tp_finalize */",
		"};",
		0),
			fmt::arg("classname", classdef.name),
			fmt::arg("constructor", constructor_name),
			fmt::arg("destructor", classdef.funcname_destructor),
			fmt::arg("doc", getAnnotationText( classdef.doc, papuga_AnnotationType_Description))
		) << std::endl;
}

static void define_struct(
		std::ostream& out,
		const papuga_InterfaceDescription& descr,
		const papuga_StructInterfaceDescription& structdef)
{
	int nofmembers = 0;
	papuga_StructMemberDescription const* mi = structdef.members;
	for (int midx=0; mi->name; ++mi,++midx) {++nofmembers;}

	out << fmt::format( papuga::cppCodeSnippet( 0,
		"static PyMemberDef g_members_{structname}[ {memberarraysize}] = {",
		0),
		fmt::arg("structname", structdef.name),
		fmt::arg("memberarraysize", nofmembers+1)
	);
	mi = structdef.members;
	for (int midx=0; mi->name; ++mi,++midx)
	{
		std::string memberdoc = getAnnotationText( mi->doc, papuga_AnnotationType_Description);
		out << "\t" << "{\"" << mi->name << "\", T_OBJECT_EX, " << papuga_python_StructObjectElement_offset( midx) << ", 0, \"" << memberdoc << "\"}," << std::endl;
	}
	out << "\t" << "{NULL,0,0,0}" << std::endl << "};" << std::endl << std::endl;

	out << fmt::format( papuga::cppCodeSnippet( 0,
		"static PyTypeObject g_typestruct_{structname} =",
		"{",
		"PyVarObject_HEAD_INIT(&PyType_Type, 0)",
		"\"{structname}\",		/* tp_name */",
		"sizeof(papuga_python_StructObject) + {nofmembers} * sizeof(papuga_python_StructObjectElement), /* tp_basicsize */",
		"1,				/* tp_itemsize */",
		"papuga_python_destroy_struct,	/* tp_dealloc */",
		"0,				/* tp_print */",
		"0,				/* tp_getattr */",
		"0,				/* tp_setattr */",
		"0,				/* tp_reserved */",
		"0,				/* tp_repr */",
		"0,				/* tp_as_number */",
		"0,				/* tp_as_sequence */",
		"0,				/* tp_as_mapping */",
		"0,				/* tp_hash */",
		"0,				/* tp_call */",
		"0,				/* tp_str */",
		"0,				/* tp_getattro */",
		"0,				/* tp_setattro */",
		"0,				/* tp_as_buffer */",
		"Py_TPFLAGS_DEFAULT,		/* tp_flags */",
		"\"{doc}\",			/* tp_doc */",
		"0,				/* tp_traverse */",
		"0,				/* tp_clear */",
		"0,				/* tp_richcompare */",
		"0,				/* tp_weaklistoffset */",
		"0,				/* tp_iter */",
		"0,				/* tp_iternext */",
		"0,				/* tp_methods */",
		"g_members_{structname},	/* tp_members */",
		"0,				/* tp_getset */",
		"0,				/* tp_base */",
		"0,				/* tp_dict */",
		"0,				/* tp_descr_get */",
		"0,				/* tp_descr_set */",
		"0,				/* tp_dictoffset */",
		"0,				/* tp_init */",
		"PyType_GenericAlloc,		/* tp_alloc */",
		"PyType_GenericNew,		/* tp_new */",
		"0,				/* tp_free */",
		"0,				/* tp_is_gc */",
		"0,				/* tp_bases */",
		"0,				/* tp_mro */",
		"0,				/* tp_cache */",
		"0,				/* tp_subclasses */",
		"0,				/* tp_weaklist */",
		"0,				/* tp_del */",
		"0,				/* tp_version_tag */",
		"0				/* tp_finalize */",
		"};",
		0),
			fmt::arg("structname", structdef.name),
			fmt::arg("nofmembers", nofmembers),
			fmt::arg("doc", getAnnotationText( structdef.doc, papuga_AnnotationType_Description))
		) << std::endl;
}

static void define_main(
		std::ostream& out,
		const papuga_InterfaceDescription& descr)
{
	std::string modulename( descr.name);
	std::transform( modulename.begin(), modulename.end(), modulename.begin(), ::tolower);

	out << fmt::format( papuga::cppCodeSnippet( 0,
		"static PyModuleDef g_moduledef =",
		"{",
		"PyModuleDef_HEAD_INIT,",
		"\"{modulename}\",",
		"\"{description}\",	/* m_doc */",
		"-1,			/* m_size */",
		"NULL,			/* m_methods */",
		"NULL,			/* m_slots */",
		"NULL,			/* m_traverse */",
		"NULL,			/* m_clear */",
		"NULL			/* m_free */",
		"};",
		0),
			fmt::arg("modulename", modulename),
			fmt::arg("description", descr.description ? descr.description : "")
		) << std::endl << std::endl;

	out << "PyMODINIT_FUNC PyInit_" << modulename << "(void)" << std::endl
		<< "{" << std::endl
		<< "\t" << "PyObject* rt;" << std::endl
		<< "\t" << "if (0>papuga_python_init())" << std::endl
		<< "\t" << "{" << std::endl
		<< "\t" << "return NULL;" << std::endl
		<< "\t" << "}" << std::endl << std::endl;
	std::size_t ci;
	for (ci=0; descr.classes[ci].name; ++ci)
	{
		out << "\t" << "g_typeobjectar[ " << ci << "] = &g_typeobject_" << descr.classes[ci].name << ";" << std::endl;
		out << "\t" << "if (PyType_Ready(&g_typeobject_" << descr.classes[ci].name << ") < 0) return NULL;" << std::endl;
	}
	std::size_t mi;
	for (mi=0; descr.structs[mi].name; ++mi)
	{
		out << "\t" << "g_typestructar[ " << mi << "] = &g_typestruct_" << descr.structs[mi].name << ";" << std::endl;
		out << "\t" << "if (PyType_Ready(&g_typestruct_" << descr.structs[mi].name << ") < 0) return NULL;" << std::endl;
	}
	out << "\t" << "rt = PyModule_Create( &g_moduledef);" << std::endl;
	out << "\t" << "if (rt == NULL) return NULL;" << std::endl;

	for (ci=0; descr.classes[ci].name; ++ci)
	{
		out << "\t" << "Py_INCREF( &g_typeobject_" << descr.classes[ci].name << ");" << std::endl;
		out << "\t" << "PyModule_AddObject( rt, \"" << descr.classes[ci].name << "\", (PyObject *)&g_typeobject_" << descr.classes[ci].name << ");" << std::endl;
	}
	for (mi=0; descr.structs[mi].name; ++mi)
	{
		out << "\t" << "Py_INCREF( &g_typestruct_" << descr.structs[mi].name << ");" << std::endl;
		out << "\t" << "PyModule_AddObject( rt, \"" << descr.structs[mi].name << "\", (PyObject *)&g_typestruct_" << descr.structs[mi].name << ");" << std::endl;
	}
	out << "\t" << "return rt;" << std::endl;
	out << "}" << std::endl << std::endl;
}

static void define_class_entrymap(
		std::ostream& out,
		unsigned int nofClasses,
		unsigned int nofStructs)
{
	{
		unsigned int ci = 0;
		out << "static PyTypeObject* g_typeobjectar[ " << (nofClasses+1) << "] = {";
		for (; ci < nofClasses; ++ci) out << "0,";
		out << "0};" << std::endl;
	}{
		unsigned int mi = 0;
		out << "static PyTypeObject* g_typestructar[ " << (nofStructs+1) << "] = {";
		for (; mi < nofStructs; ++mi) out << "0,";
		out << "0};" << std::endl;
	}
	out << "static papuga_python_ClassEntryMap g_class_entry_map = { " << nofClasses << ", g_typeobjectar, " << nofStructs << ", g_typestructar};" << std::endl;
	out << std::endl;
}

void papuga::printPython3ModSource(
		std::ostream& out,
		const papuga_InterfaceDescription& descr,
		const std::vector<std::string>& includes)
{
	std::string modulename = descr.name;
	std::transform( modulename.begin(), modulename.end(), modulename.begin(), ::tolower);
	std::string MODULENAME = descr.name;
	std::transform( MODULENAME.begin(), MODULENAME.end(), MODULENAME.begin(), ::toupper);

	out << fmt::format( papuga::cppCodeSnippet( 0,
		"#define PYTHON_{MODULENAME}_EXTNAME \"{modulename}\"",
		"#define PYTHON_{MODULENAME}_VERSION \"{release}\"",
		"#include \"papuga/lib/python3_dev.h\"",
		"#include \"strus/bindingObjects.h\"",
		"#include \"papuga.h\"",
		"/* Python includes: */",
		"#include <Python.h>",
		"#include <structmember.h>",
		"",
		0),
			fmt::arg("MODULENAME", MODULENAME),
			fmt::arg("modulename", modulename),
			fmt::arg("release", descr.about && descr.about->version ? descr.about->version : "")
		) << std::endl;

	char const** fi = descr.includefiles;
	for (; *fi; ++fi)
	{
		out << "#include \"" << *fi << "\"" << std::endl;
	}
	std::vector<std::string>::const_iterator ai = includes.begin(), ae = includes.end();
	for (; ai != ae; ++ai)
	{
		out << "#include \"" << *ai << "\"" << std::endl;
	}
	out << "/* @remark GENERATED FILE (libpapuga_python3_gen) - DO NOT MODIFY */" << std::endl;
	out << std::endl << std::endl;

	std::size_t nofClasses = 0;
	for (; descr.classes[ nofClasses].name; ++nofClasses){}
	std::size_t nofStructs = 0;
	for (; descr.structs[ nofStructs].name; ++nofStructs){}
	define_class_entrymap( out, nofClasses, nofStructs);

	for (std::size_t ci=0; descr.classes[ci].name; ++ci)
	{
		const papuga_ClassDescription& classdef = descr.classes[ci];
		if (classdef.constructor)
		{
			define_constructor( out, ci+1, descr, classdef);
		}
		std::size_t mi = 0;
		for (; classdef.methodtable[mi].name; ++mi)
		{
			define_method( out, descr, classdef, classdef.methodtable[mi]);
		}
		define_methodtable( out, descr, classdef);
		define_class( out, descr, classdef);
	}
	for (std::size_t mi=0; descr.structs[mi].name; ++mi)
	{
		define_struct( out, descr, descr.structs[ mi]);
	}
	define_main( out, descr);
}

