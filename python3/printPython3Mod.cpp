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

#define INDENT "\t"

static std::string namespace_classname( const std::string& modulename, const std::string& classname)
{
	return modulename + classname;
}

static void define_classdefmap(
		std::ostream& out,
		const papuga_InterfaceDescription& descr)
{
	std::string modulename = descr.name;

	// Count number of classes and define zend class object global:
	papuga_ClassDescription const* ci = descr.classes;
	unsigned int cidx = 0;
	for (; ci->name; ++ci,++cidx)
	{
		out << "static zend_class_entry* g_" << ci->name << "_ce = NULL;" << std::endl;
	}

	out << "static papuga_zend_class_entry* g_class_entry_list[ " << cidx << "];" << std::endl;
	out << "static const papuga_php_ClassEntryMap g_class_entry_map = { " << cidx << ", g_class_entry_list };" << std::endl << std::endl;
}

static void define_method(
		std::ostream& out,
		const papuga_InterfaceDescription& descr,
		const papuga_ClassDescription& classdef,
		const papuga_MethodDescription& method)
{
	std::string modulename = descr.name;

	out << fmt::format( papuga::cppCodeSnippet( 0,
		"static PyObject* {classname}__{methodname}(PyObject* self, PyObject* args)",
		"{",
		"papuga_python_CallArgs argstruct;",
		"papuga_CallResult retstruct;",
		"papuga_ErrorBuffer errbuf;",
		"char errstr[ 2048];",
		"const char* msg;",
		"",
		"if (!papuga_python_init_CallArgs( args, &argstruct))",
		"{",
			"papuga_python_error( papuga_ErrorCode_tostring( argstruct.errcode));",
			"return NULL;",
		"}",
		"papuga_init_CallResult( &retstruct, errstr, sizeof(errstr));",
		"if (!{funcname}( self, &retstruct, argstruct.argc, argstruct.argv))",
		"{",
			"msg = papuga_CallResult_lastError( &retstruct);",
			"papuga_python_destroy_CallArgs( &argstruct);",
			"papuga_destroy_CallResult( &retstruct);",
			"papuga_python_error( msg);",
			"return NULL;",
		"}",
		"papuga_php_destroy_CallArgs( &argstruct);",
		"papuga_init_ErrorBuffer( &errbuf, errstr, sizeof(errstr));",
		"papuga_python_move_CallResult( return_value, &retstruct, &g_class_entry_map, &errbuf);",
		"if (papuga_ErrorBuffer_hasError( &errbuf))",
		"{",
			"papuga_python_error( errbuf.ptr);",
			"return NULL;",
		"}",
		"}",
		0),
			fmt::arg("methodname", method.name),
			fmt::arg("nsclassname", namespace_classname( modulename, classdef.name)),
			fmt::arg("selfparam", selfparam),
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

	out << fmt::format( papuga::cppCodeSnippet( 0,
		"PHP_METHOD({nsclassname}, __construct)",
		"{",
		"papuga_HostObject thisHostObject;",
		"papuga_php_CallArgs argstruct;",
		"papuga_ErrorBuffer errbuf;",
		"void* self;",
		"char errstr[ 2048];",
		"const char* msg;",
		"int argc = ZEND_NUM_ARGS();",

		"if (!papuga_php_init_CallArgs( NULL/*self*/, argc, &argstruct))",
		"{",
			"PHP_FAIL( papuga_ErrorCode_tostring( argstruct.errcode));",
			"return;",
		"}",
		"papuga_init_ErrorBuffer( &errbuf, errstr, sizeof(errstr));",
		"self = {constructor}( &errbuf, argstruct.argc, argstruct.argv);",
		"if (!self)",
		"{",
			"msg = papuga_ErrorBuffer_lastError( &errbuf);",
			"papuga_php_destroy_CallArgs( &argstruct);",
			"PHP_FAIL( msg);",
			"return;",
		"}",
		"papuga_php_destroy_CallArgs( &argstruct);",
		"zval *thiszval = getThis();",
		"papuga_init_HostObject( &thisHostObject, {classid}, self, &{destructor});",
		"if (!papuga_php_init_object( thiszval, &thisHostObject))",
		"{",
			"PHP_FAIL( \"object initialization failed\");",
			"return;",
		"}",
		"}",
		0),
			fmt::arg("nsclassname", namespace_classname( modulename, classdef.name)),
			fmt::arg("classid", classid),
			fmt::arg("constructor", classdef.constructor->funcname),
			fmt::arg("destructor", classdef.funcname_destructor)
		) << std::endl;
}

static void define_methodtable(
		std::ostream& out,
		const papuga_InterfaceDescription& descr,
		const papuga_ClassDescription& classdef)
{
	std::string modulename = descr.name;
	std::string nsclassname = namespace_classname( modulename, classdef.name);

	out << fmt::format( papuga::cppCodeSnippet( 0,
		"static PyMethodDef g_{classname}_methods[] =",
		"{",
		0),
			fmt::arg("classname", classdef.name)
		) << std::endl;
	if (classdef.constructor)
	{
	}
	papuga_MethodDescription const* mi = classdef.methodtable;
	for (; mi->name; ++mi)
	{
		out << fmt::format( papuga::cppCodeSnippet( 1,
			"{\"{methodname}", &g_{classname}__{methodname}, METH_VARARGS, \"{description}\"},",
			0)
				fmt::arg("classname", classdef.name),
				fmt::arg("methodname", mi->name)
				
			) << std::endl;
	     {NULL, NULL, 0, NULL}
	};
	
	out << "static const zend_function_entry g_" << classdef.name << "_methods[] = {" << std::endl;
	if (classdef.constructor)
	{
		out << "\t" << "PHP_ME(" << nsclassname << ",  __construct, NULL, ZEND_ACC_PUBLIC | ZEND_ACC_CTOR)" << std::endl;
	}
	papuga_MethodDescription const* mi = classdef.methodtable;
	for (; mi->name; ++mi)
	{
		out << "\t" << "PHP_ME(" << nsclassname << ", " << mi->name << ", NULL, ZEND_ACC_PUBLIC)" << std::endl;
	}
	out << "\t" << "PHP_FE_END" << std::endl;
	out << "};" << std::endl;
}

static void define_main(
		std::ostream& out,
		const papuga_InterfaceDescription& descr)
{
	std::string ModuleName = descr.name;

	out << fmt::format( papuga::cppCodeSnippet( 0,
		"static struct PyModuleDef g_moduledef = {",
		"PyModuleDef_HEAD_INIT,",
		"\"{ModuleName}\",
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
		);
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
		"#define Py_RETURN(self)  {PyObject* rt=self; Py_INCREF(rt); return rt;}",
		"",
		0),
			fmt::arg("MODULENAME", MODULENAME),
			fmt::arg("modulename", modulename),
			fmt::arg("release", descr.about ? descr.about->version : "")
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
	out << "///\\remark GENERATED FILE (libpapuga_python3_gen) - DO NOT MODIFY" << std::endl;
	out << std::endl << std::endl;

	define_classdefmap( out, descr);

	std::size_t ci;
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
	}
	define_main( out, descr);
}

void papuga::printPython3ModSetup(
		std::ostream& out,
		const papuga_InterfaceDescription& descr,
		const std::string& c_includedir,
		const std::string& c_libdir)
{
	char const* vi = descr.about->version ? descr.about->version : "";
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
				fmt::arg("version", descr.about ? descr.about->version : "0.0"),
				fmt::arg("c_includedir", c_includedir),
				fmt::arg("c_libdir", c_libdir),
				fmt::arg("description", descr.description ? descr.description:""),
				fmt::arg("author", descr.author ? descr.author:""),
				fmt::arg("url", descr.url ? descr.url:"")
			) << std::endl;
}

