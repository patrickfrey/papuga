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
	std::string modulename = descr.name;
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
			"void* self = ((papuga_python_ClassObject*)selfobj)->self;"
			"papuga_python_CallArgs argstruct;",
			"papuga_CallResult retstruct;",
			"papuga_ErrorBuffer errbuf;",
			"char errstr[ 2048];",
			"const char* msg;",
			"",
			"if (!papuga_python_init_CallArgs( &argstruct, args, g_paramname_{classname}__{methodname}))",
			"{",
				"papuga_python_error( \"error in '%s': %s\", \"{classname}->{methodname}\", papuga_ErrorCode_tostring( argstruct.errcode));",
				"return NULL;",
			"}",
			"papuga_init_CallResult( &retstruct, errstr, sizeof(errstr));",
			"if (!{funcname}( self, &retstruct, argstruct.argc, argstruct.argv))",
			"{",
				"msg = papuga_CallResult_lastError( &retstruct);",
				"papuga_python_destroy_CallArgs( &argstruct);",
				"papuga_destroy_CallResult( &retstruct);",
				"papuga_python_error( \"error in '%s': %s\", \"{classname}->{methodname}\", msg);",
				"return NULL;",
			"}",
			"papuga_python_destroy_CallArgs( &argstruct);",
			"papuga_init_ErrorBuffer( &errbuf, errstr, sizeof(errstr));",
			"rt = papuga_python_move_CallResult( &retstruct, &g_class_entry_map, &errbuf);",
			"if (papuga_ErrorBuffer_hasError( &errbuf))",
			"{",
				"papuga_python_error( \"error in '%s': %s\", \"{classname}->{methodname}\", errbuf.ptr);",
				"return NULL;",
			"}",
			"Py_RETURN( rt);",
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
	std::string modulename = descr.name;
	out << "static const char* g_paramname_constructor__" << classdef.name << "[] = {";
	const papuga_ParameterDescription* pi = classdef.constructor->parameter;
	for (; pi->name; ++pi)
	{
		out << "\"" << pi->name << "\",";
	}
	out << "NULL};" << std::endl << std::endl;

	out << fmt::format( papuga::cppCodeSnippet( 0,
		"static PyObject* constructor__{classname}( PyObject* selfobj, PyObject* args)",
		"{",
			"void* self;",
			"papuga_python_CallArgs argstruct;",
			"papuga_ErrorBuffer errbuf;",
			"char errstr[ 2048];",
			"const char* msg;",
			"",
			"if (!papuga_python_init_CallArgs( &argstruct, args, g_paramname_constructor__{classname}))",
			"{",
				"papuga_python_error( \"error in constructor of '%s': %s\", \"{classname}\", papuga_ErrorCode_tostring( argstruct.errcode));",
				"return NULL;",
			"}",
			"papuga_init_ErrorBuffer( &errbuf, errstr, sizeof(errstr));",
			"self = {constructor}( &errbuf, argstruct.argc, argstruct.argv);",
			"if (!self)",
			"{",
				"msg = papuga_ErrorBuffer_lastError( &errbuf);",
				"papuga_python_destroy_CallArgs( &argstruct);",
				"papuga_python_error( \"error in constructor of '%s': %s\", \"{classname}\", msg);",
				"return NULL;",
			"}",
			"((papuga_python_ClassObject*)selfobj)->self = self;"
			"papuga_python_destroy_CallArgs( &argstruct);",
			"Py_RETURN( selfobj);",
		"}",
		0),
			fmt::arg("classname", classdef.name),
			fmt::arg("classid", classid),
			fmt::arg("constructor", classdef.constructor->funcname),
			fmt::arg("destructor", classdef.funcname_destructor)
		) << std::endl;
}

static const char* getAnnotationText(
		const papuga_Annotation* ann,
		const papuga_AnnotationType type)
{
	papuga_Annotation const* di = ann;
	if (di) for (; di->text; ++di)
	{
		if (di->type == type)
		{
			return di->text;
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
	if (classdef.constructor)
	{
		const char* description = getAnnotationText( classdef.doc, papuga_AnnotationType_Description);
		out << fmt::format( papuga::cppCodeSnippet( 1,
			"{{\"_ _init_ _\", &constructor__{classname}, METH_VARARGS|METH_KEYWORDS, \"{description}\"}},",
			0),
				fmt::arg("classname", classdef.name),
				fmt::arg("description", description)
			);
	}
	papuga_MethodDescription const* mi = classdef.methodtable;
	for (; mi->name; ++mi)
	{
		const char* description = getAnnotationText( mi->doc, papuga_AnnotationType_Description);
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
	out << fmt::format( papuga::cppCodeSnippet( 0,
		"static void dealloc__{classname}( PyObject* selfobj)",
		"{",
			"void* self = ((papuga_python_ClassObject*)selfobj)->self;",
			"{destructor}( self);",
			"PyObject_Del(self);",
		"}",
		"static PyObject* init__{classname}( PyTypeObject *subtype, PyObject *args, PyObject *kwargs)",
		"{",
		    "return args ? constructor__{classname}( NULL, args) : constructor__{classname}( NULL, kwargs);",
		"}",
		"",
		"static PyTypeObject g_typeobject_{classname} =",
		"{",
		"PyVarObject_HEAD_INIT(&PyType_Type, 0)",
		"\"{classname}\",                /* tp_name */",
		"sizeof(papuga_python_ClassObject), /* tp_basicsize */",
		"0,                              /* tp_itemsize */",
		"(destructor)dealloc__{classname},/* tp_dealloc */",
		"0,                              /* tp_print */",
		"0,                              /* tp_getattr */",
		"0,                              /* tp_setattr */",
		"0,                              /* tp_reserved */",
		"0,                              /* tp_repr */",
		"0,                              /* tp_as_number */",
		"0,                              /* tp_as_sequence */",
		"0,                              /* tp_as_mapping */",
		"0,                              /* tp_hash */",
		"0,                              /* tp_call */",
		"0,                              /* tp_str */",
		"0,                              /* tp_getattro */",
		"0,                              /* tp_setattro */",
		"0,                              /* tp_as_buffer */",
		"Py_TPFLAGS_DEFAULT,             /* tp_flags */",
		"0,                              /* tp_doc */",
		"0,                              /* tp_traverse */",
		"0,                              /* tp_clear */",
		"0,                              /* tp_richcompare */",
		"0,                              /* tp_weaklistoffset */",
		"0,                              /* tp_iter */",
		"0,                              /* tp_iternext */",
		"g_methods_{classname},          /* tp_methods */",
		"0,                              /* tp_members */",
		"0,                              /* tp_getset */",
		"0,                              /* tp_base */",
		"0,                              /* tp_dict */",
		"0,                              /* tp_descr_get */",
		"0,                              /* tp_descr_set */",
		"0,                              /* tp_dictoffset */",
		"init__{classname},              /* tp_init */",
		"0,                              /* tp_alloc */",
		"0,                              /* tp_new */",
		"};",
		0),
			fmt::arg("classname", classdef.name),
			fmt::arg("destructor", classdef.funcname_destructor)
		) << std::endl;
}

static void define_main(
		std::ostream& out,
		const papuga_InterfaceDescription& descr)
{
	std::string ModuleName = descr.name;

	out << fmt::format( papuga::cppCodeSnippet( 0,
		"static PyMethodDef g_module_functions[] = {{ {{0, 0}} }};",
		"",
		"static struct PyModuleDef g_moduledef =",
		"{",
		"PyModuleDef_HEAD_INIT,",
		"\"{ModuleName}\",",
		"\"{description}\",     /* m_doc */",
		"-1,                    /* m_size */",
		"g_module_functions,    /* m_methods */",
		"NULL,                  /* m_reload */",
		"NULL,                  /* m_traverse */",
		"NULL,                  /* m_clear */",
		"NULL,                  /* m_free */",
		"};",
		0),
			fmt::arg("ModuleName", ModuleName),
			fmt::arg("description", descr.description ? descr.description : "")
		) << std::endl << std::endl;

	out << "PyMODINIT_FUNC PyInit_" << ModuleName << "(void)" << std::endl
		<< "{" << std::endl
		<< "\t" << "PyObject* rt;" << std::endl;
	std::size_t ci;
	for (ci=0; descr.classes[ci].name; ++ci)
	{
		out << "\t" << "g_typeobjectar[ " << ci << "] = &g_typeobject_" << descr.classes[ci].name << ";" << std::endl;
	}
	out << "\t" << "rt = PyModule_Create( &g_moduledef);" << std::endl;
	out << "\t" << "return rt;" << std::endl;
	out << "}" << std::endl << std::endl;
}

static void define_class_entrymap(
		std::ostream& out,
		unsigned int nofClasses)
{
	unsigned int ci = 0;
	out << "static PyTypeObject* g_typeobjectar[ " << (nofClasses+1) << "] = {";
	for (; ci < nofClasses; ++ci) out << "0,";
	out << "0};" << std::endl;
	out << "static papuga_python_ClassEntryMap g_class_entry_map = { " << nofClasses << ", g_typeobjectar };" << std::endl;
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
		"",
		"#include <Python.h>",
		"",
		"#define Py_RETURN(self)  {{PyObject* rt_=self; if (rt_) Py_INCREF(rt_); return rt_;}}",
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

	std::size_t ci;
	for (ci=0; descr.classes[ci].name; ++ci){}
	define_class_entrymap( out, ci);

	for (ci=0; descr.classes[ci].name; ++ci)
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
	define_main( out, descr);
}

void papuga::printPython3ModSetup(
		std::ostream& out,
		const papuga_InterfaceDescription& descr,
		const std::string& c_includedir,
		const std::string& c_libdir)
{
	char const* vi = descr.about && descr.about->version ? descr.about->version : "";
	std::string version_major;
	std::string version_minor;
	for (; *vi && *vi <= 32; ++vi){}
	for (; *vi && *vi >= '0' && *vi <= '9'; ++vi) {version_major.push_back(*vi);}
	if (*vi == '.' || *vi == '-')
	{
		++vi;
		for (; *vi && *vi >= '0' && *vi <= '9'; ++vi) {version_minor.push_back(*vi);}
	}
	if (version_major.empty()) version_major = "0";
	if (version_minor.empty()) version_minor = "0";
	std::string ModuleName = descr.name;
	std::string modulename = descr.name;
	std::transform( modulename.begin(), modulename.end(), modulename.begin(), ::tolower);
	std::string MODULENAME = descr.name;
	std::transform( MODULENAME.begin(), MODULENAME.end(), MODULENAME.begin(), ::toupper);

	out << fmt::format( papuga::cppCodeSnippet( 0,
		"from distutils.core import setup, Extension",
		"module1 = Extension('demo',"
		"define_macros = [('MAJOR_VERSION', '{MAJOR_VERSION}'),('MINOR_VERSION', '{MINOR_VERSION}')],",
		"include_dirs = ['{c_includedir}'],",
		"libraries = ['tcl83'],",
		"library_dirs = ['{c_libdir}','{c_libdir}/{modulename}'],",
		"sources = ['{modulename}.c'])",
		"",
		"setup (name = '{ModuleName}',",
		"version = '{version}',",
		"description = '{description}',",
		"author = '{author}',",
		"url = '{url}',",
		"ext_modules = [{modulename}])",
		0),
			fmt::arg("MODULENAME", MODULENAME),
			fmt::arg("ModuleName", ModuleName),
			fmt::arg("modulename", modulename),
			fmt::arg("MAJOR_VERSION", version_major),
			fmt::arg("MINOR_VERSION", version_minor),
			fmt::arg("version", descr.about && descr.about->version ? descr.about->version : "0.0"),
			fmt::arg("c_includedir", c_includedir),
			fmt::arg("c_libdir", c_libdir),
			fmt::arg("description", descr.description ? descr.description:""),
			fmt::arg("author", descr.about && descr.about->author ? descr.about->author:""),
			fmt::arg("url", descr.about && descr.about->url ? descr.about->url:"")
		) << std::endl;
}

