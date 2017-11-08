/*
* Copyright (c) 2017 Patrick P. Frey
*
* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/
/// \brief Module for printing the PHP (v7) module C source
/// \file printPhp7Mod.cpp
#include "printPhp7Mod.hpp"
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
	int cidx = 0;
	for (; ci->name; ++ci,++cidx)
	{
		out << "static zend_class_entry* g_classentry_" << ci->name << " = NULL;" << std::endl;
	}
	papuga_StructInterfaceDescription const* si = descr.structs;
	int sidx = 0;
	for (; si->name; ++si,++sidx)
	{
		out << "static const char* g_structmembers_" << si->name << "[] = {";
		papuga_StructMemberDescription const* mi = si->members;
		for (; mi->name; ++mi)
		{
			out << "\"" << mi->name << "\", ";
		}
		out << "NULL};" << std::endl;
	}
	out << "static const char** g_structmembers[ " << (sidx+1) << "] = {";
	si = descr.structs;
	sidx = 0;
	for (; si->name; ++si,++sidx)
	{
		out << "g_structmembers_" << si->name << ", ";
	}
	out << "NULL};" << std::endl;
	out << "static papuga_zend_class_entry* g_class_entry_list[ " << cidx << "];" << std::endl;
	out << "static const papuga_php_ClassEntryMap g_class_entry_map = { " << cidx << ", g_class_entry_list, " << sidx << ", g_structmembers};" << std::endl << std::endl;
}

static void define_method(
		std::ostream& out,
		const papuga_InterfaceDescription& descr,
		const papuga_ClassDescription& classdef,
		const papuga_MethodDescription& method)
{
	std::string modulename = descr.name;
	std::string selfparam = method.nonstatic ? "getThis()":"NULL";

	out << fmt::format( papuga::cppCodeSnippet( 0,
		"PHP_METHOD({nsclassname}, {methodname})",
		"{",
		"papuga_CallArgs argstruct;",
		"papuga_CallResult retstruct;",
		"papuga_ErrorCode errcode = papuga_Ok;",
		"char errstr[ 2048];"
		"const char* msg;",
		"int argc = ZEND_NUM_ARGS();",
		"",
		"zval *obj = {selfparam};",
		"if (!papuga_php_init_CallArgs( &argstruct, (void*)obj, argc, &g_class_entry_map))",
		"{",
			"PHP_FAIL( papuga_ErrorCode_tostring( argstruct.errcode));",
			"return;",
		"}",
		"papuga_init_CallResult( &retstruct, errstr, sizeof(errstr));",
		"if (!{funcname}( argstruct.self, &retstruct, argstruct.argc, argstruct.argv))",
		"{",
			"msg = papuga_CallResult_lastError( &retstruct);",
			"papuga_destroy_CallArgs( &argstruct);",
			"papuga_destroy_CallResult( &retstruct);",
			"PHP_FAIL( msg);",
			"return;",
		"}",
		"papuga_destroy_CallArgs( &argstruct);",
		"if (!papuga_php_move_CallResult( return_value, &retstruct, &g_class_entry_map, &errcode))",
		"{",
			"PHP_FAIL( papuga_ErrorCode_tostring( errcode));",
			"return;",
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
		"papuga_CallArgs argstruct;",
		"papuga_ErrorBuffer errbuf;",
		"void* self;",
		"zval *thiszval;",
		"char errstr[ 2048];",
		"const char* msg;",
		"int argc = ZEND_NUM_ARGS();",

		"if (!papuga_php_init_CallArgs( &argstruct,  NULL/*self*/, argc, &g_class_entry_map))",
		"{",
			"PHP_FAIL( papuga_ErrorCode_tostring( argstruct.errcode));",
			"return;",
		"}",
		"papuga_init_ErrorBuffer( &errbuf, errstr, sizeof(errstr));",
		"self = {constructor}( &errbuf, argstruct.argc, argstruct.argv);",
		"if (!self)",
		"{",
			"msg = papuga_ErrorBuffer_lastError( &errbuf);",
			"papuga_destroy_CallArgs( &argstruct);",
			"PHP_FAIL( msg);",
			"return;",
		"}",
		"papuga_destroy_CallArgs( &argstruct);",
		"thiszval = getThis();",
		"if (!papuga_php_init_object( thiszval, self, {classid}, &{destructor}))",
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
	std::string modulename = descr.name;
	std::transform( modulename.begin(), modulename.end(), modulename.begin(), ::tolower);
	std::string Modulename = descr.name;

	out << fmt::format( papuga::cppCodeSnippet( 0,
		"static zend_object* create_zend_object_wrapper( zend_class_entry* ce)",
		"{",
			"return (zend_object*)papuga_php_create_object( ce);",
		"}",
		"PHP_MINIT_FUNCTION({modulename})"
		"{",
		"zend_class_entry tmp_ce;",
		"papuga_php_init();",
		0),
			fmt::arg("modulename", modulename)
		);
	papuga_ClassDescription const* ci = descr.classes;
	int cidx = 0;
	for (; ci->name; ++ci,++cidx)
	{
		out << fmt::format( papuga::cppCodeSnippet( 1,
			"INIT_CLASS_ENTRY(tmp_ce, \"{nsclassname}\", g_{classname}_methods);",
			"g_classentry_{classname} = zend_register_internal_class( &tmp_ce);",
			"g_classentry_{classname}->create_object = &create_zend_object_wrapper;",
			"g_class_entry_list[ {cidx}] = g_classentry_{classname};",
			0),
				fmt::arg("cidx", cidx),
				fmt::arg("classname", ci->name),
				fmt::arg("nsclassname", namespace_classname( Modulename, ci->name))
			);
	}
	out << "\t" << "return SUCCESS;" << std::endl;
	out << "}";

	out << fmt::format( papuga::cppCodeSnippet( 0,
		"PHP_MSHUTDOWN_FUNCTION({modulename})",
		"{",
		"return SUCCESS;",
		"}",
		"PHP_MINFO_FUNCTION({modulename})",
		"{",
		"php_info_print_table_start();"
		"php_info_print_table_row(2, \"strus library support\", \"enabled\");",
		"php_info_print_table_end();",
		"}",
		"const zend_function_entry {modulename}_functions[] = {",
			"PHP_FE_END",
		"};",
		"zend_module_entry {modulename}_module_entry = {",
			"STANDARD_MODULE_HEADER,",
			"\"{modulename}\",",
			"{modulename}_functions,",
			"PHP_MINIT({modulename}),",
			"PHP_MSHUTDOWN({modulename}),",
			"NULL/*PHP_RINIT({modulename})*/,",
			"NULL/*PHP_RSHUTDOWN({modulename})*/,",
			"PHP_MINFO({modulename}),",
			"\"{release}\", /* Replace with version number for your extension */",
			"STANDARD_MODULE_PROPERTIES",
		"};",
		"ZEND_GET_MODULE({modulename})",
		0),
			fmt::arg("modulename", modulename),
			fmt::arg("release", descr.about->version)
		);
}

void papuga::printPhp7ModSource(
		std::ostream& out,
		const papuga_InterfaceDescription& descr,
		const std::vector<std::string>& includes)
{
	std::string modulename = descr.name;
	std::transform( modulename.begin(), modulename.end(), modulename.begin(), ::tolower);
	std::string MODULENAME = descr.name;
	std::transform( MODULENAME.begin(), MODULENAME.end(), MODULENAME.begin(), ::toupper);

	out << fmt::format( papuga::cppCodeSnippet( 0,
		"#define PHP_{MODULENAME}_EXTNAME \"{modulename}\"",
		"#define PHP_{MODULENAME}_VERSION \"{release}\"",
		"#include \"papuga/lib/php7_dev.h\"",
		"#include \"strus/bindingObjects.h\"",
		"#include \"papuga.h\"",
		"",
		"/* PHP & Zend includes: */",
		"#ifdef _MSC_VER",
		"#include <zend_config.w32.h>",
		"#else",
		"#include <zend_config.nw.h>",
		"#endif",
		"#define ZEND_SIGNAL_H // PH:HACK: Exclude compilation of stuff we don't need with system dependencies",
		"#include <php.h>",
		"#include <zend.h>",
		"#include <zend_API.h>",
		"#include <zend_exceptions.h>",
		"#include <ext/standard/info.h>",
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
	out << "/* @remark GENERATED FILE (libpapuga_php7_gen) - DO NOT MODIFY */" << std::endl;
	out << std::endl;
	out << "#define PHP_FAIL(msg) {TSRMLS_FETCH();zend_error( E_ERROR, \"%s\", msg);RETVAL_FALSE;return;}";
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


void papuga::printPhp7ModIni(
		std::ostream& out,
		const papuga_InterfaceDescription& descr,
		const std::string& php_ini,
		const std::string& dll_ext)
{
	std::string modulename = descr.name;
	std::transform( modulename.begin(), modulename.end(), modulename.begin(), ::tolower);

	out << php_ini << std::endl;
	out << "extension=" << modulename << dll_ext << std::endl << std::endl;
}

