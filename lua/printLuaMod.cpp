/*
* Copyright (c) 2017 Patrick P. Frey
*
* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/
/// \brief Module for printing the lua module C header and source
/// \file printLuaMod.cpp
#include "printLuaMod.hpp"
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
	return modulename + '_' + classname;
}

static void define_classentrymap(
		std::ostream& out,
		const papuga_InterfaceDescription& descr)
{
	std::string modulename = descr.name;
	std::transform( modulename.begin(), modulename.end(), modulename.begin(), ::tolower);
	papuga_ClassDescription const* ci = descr.classes;

	// Count number of classes:
	unsigned int cidx = 0;
	for (; ci->name; ++ci,++cidx){}

	out << "static const char* g_classnamear[" << (cidx+1) << "] = {" << std::endl;
	ci = descr.classes;
	for (; ci->name; ++ci)
	{
		out << "\"" << namespace_classname( modulename, ci->name) << "\", ";
	}
	out << "NULL };" << std::endl << std::endl;

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

	out << "static const papuga_lua_ClassEntryMap g_classentrymap = { " << cidx << ", g_classnamear, " << sidx << ", g_structmembers};"
		<< std::endl << std::endl;
}

static void define_method(
		std::ostream& out,
		const papuga_InterfaceDescription& descr,
		const papuga_ClassDescription& classdef,
		const papuga_MethodDescription& method)
{
	std::string modulename = descr.name;
	std::transform( modulename.begin(), modulename.end(), modulename.begin(), ::tolower);

	std::string selfname = (method.nonstatic) ? (std::string("\"") + namespace_classname( modulename, classdef.name) + "\""):std::string("NULL");

	out << fmt::format( papuga::cppCodeSnippet( 0,
		"static int l_{nsclassname}_{methodname}( lua_State *ls)",
		"{",
		"int rt;",
		"papuga_CallArgs arg;",
		"papuga_Allocator allocator;",
		"papuga_CallResult retval;",
		"char membuf_args[ 4096];",
		"char membuf_retv[ 4096];",
		"char membuf_err[ 256];",
		"papuga_init_CallArgs( &arg, membuf_args, sizeof(membuf_args));",
		"if (!papuga_lua_set_CallArgs( &arg, ls, lua_gettop(ls), {selfname}))",
		"{",
			"papuga_destroy_CallArgs( &arg);",
			"papuga_lua_error( ls, \"{nsclassname}.{methodname}\", arg.errcode);",
		"}",
		"papuga_init_Allocator( &allocator, membuf_retv, sizeof(membuf_retv));",
		"papuga_init_CallResult( &retval, &allocator, true/*allocator ownership*/, membuf_err, sizeof(membuf_err));",
		"if (!{funcname}( arg.self, &retval, arg.argc, arg.argv)) goto ERROR_CALL;",
		"papuga_destroy_CallArgs( &arg);",
		"rt = papuga_lua_move_CallResult( ls, &retval, &g_classentrymap, &arg.errcode);",
		"if (rt < 0) papuga_lua_error( ls, \"{nsclassname}.{methodname}\", arg.errcode);",
		"return rt;",
		"ERROR_CALL:",
		"papuga_destroy_CallResult( &retval);",
		"papuga_destroy_CallArgs( &arg);",
		"papuga_lua_error_str( ls, \"{nsclassname}.{methodname}\", papuga_CallResult_lastError( &retval));",
		"return 0; /*... never get here (papuga_lua_error_str exits) */",
		"}",
		0),
			fmt::arg("methodname", method.name),
			fmt::arg("nsclassname", namespace_classname( modulename, classdef.name)),
			fmt::arg("classname", classdef.name),
			fmt::arg("selfname", selfname),
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
	std::transform( modulename.begin(), modulename.end(), modulename.begin(), ::tolower);

	out << fmt::format( papuga::cppCodeSnippet( 0,
		"static int l_new_{nsclassname}( lua_State *ls)",
		"{",
		"void* objref;",
		"papuga_CallArgs arg;",
		"papuga_ErrorBuffer errbufstruct;",
		"papuga_lua_UserData* udata = papuga_lua_new_userdata( ls, \"{nsclassname}\");",
		"char membuf_args[ 4096];",
		"char membuf_err[ 256];",
		"papuga_init_CallArgs( &arg, membuf_args, sizeof(membuf_args));",
		"if (!papuga_lua_set_CallArgs( &arg, ls, lua_gettop(ls)-1, NULL))",
		"{",
			"papuga_destroy_CallArgs( &arg);",
			"papuga_lua_error( ls, \"{nsclassname}.new\", arg.errcode);",
		"}",
		"papuga_init_ErrorBuffer( &errbufstruct, membuf_err, sizeof(membuf_err));",
		"objref = {constructor}( &errbufstruct, arg.argc, arg.argv);",
		"if (!objref) goto ERROR_CALL;",
		"papuga_destroy_CallArgs( &arg);",
		"papuga_lua_init_UserData( udata, {classid}, objref, {destructor}, &g_classentrymap);",
		"return 1;",
		"ERROR_CALL:",
		"papuga_destroy_CallArgs( &arg);",
		"lua_pop(ls, 1);/*... pop udata */",
		"papuga_lua_error_str( ls, \"{nsclassname}.new\", membuf_err);",
		"return 0; /*... never get here (papuga_lua_error_str exits) */",
		"}",
		0),
			fmt::arg("classname", classdef.name),
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
	std::transform( modulename.begin(), modulename.end(), modulename.begin(), ::tolower);

	out << fmt::format( papuga::cppCodeSnippet( 0,
			"static const luaL_Reg mt_{nsclassname}[] =", "{", 0),
			fmt::arg("classname", classdef.name),
			fmt::arg("nsclassname", namespace_classname( modulename, classdef.name)));
	std::size_t mi = 0;
	if (classdef.constructor)
	{
		out << fmt::format( papuga::cppCodeSnippet( 1, "{{ \"new\", &l_new_{nsclassname} }},", 0),
				fmt::arg("classname", classdef.name),
				fmt::arg("nsclassname", namespace_classname( modulename, classdef.name)));
	}
	for (; classdef.methodtable[mi].name; ++mi)
	{
		out << fmt::format( papuga::cppCodeSnippet( 1, "{{ \"{methodname}\", &l_{nsclassname}_{methodname} }},", 0),
				fmt::arg("classname", classdef.name),
				fmt::arg("nsclassname", namespace_classname( modulename, classdef.name)),
				fmt::arg("methodname", classdef.methodtable[mi].name));
	}
	out << "\t" << "{0,0}" << "};" << std::endl << std::endl;
}

static void define_main(
		std::ostream& out,
		const papuga_InterfaceDescription& descr)
{
	std::string modulename = descr.name;
	std::transform( modulename.begin(), modulename.end(), modulename.begin(), ::tolower);

	out << fmt::format( papuga::cppCodeSnippet( 0,
		"int luaopen_{modulename}( lua_State* ls )",
		"{",
		"papuga_lua_init( ls);",
		0),
		fmt::arg("modulename", modulename)
	);
	std::size_t ci = 0;
	for (; descr.classes[ci].name; ++ci)
	{
		const papuga_ClassDescription& classdef = descr.classes[ci];
		out << fmt::format( papuga::cppCodeSnippet( 1,
			"papuga_lua_declare_class( ls, {classid}, \"{nsclassname}\", mt_{nsclassname});",
			0),
			fmt::arg("modulename", modulename),
			fmt::arg("classid", ci+1),
			fmt::arg("classname", classdef.name),
			fmt::arg("nsclassname", namespace_classname( modulename, classdef.name)),
			fmt::arg("destructor", classdef.funcname_destructor)
		);
	}
	out << "\t" << "return 0;" << std::endl << "}" << std::endl << std::endl;
}

void papuga::printLuaModHeader(
		std::ostream& out,
		const papuga_InterfaceDescription& descr)
{
	std::string modulename = descr.name;
	std::transform( modulename.begin(), modulename.end(), modulename.begin(), ::tolower);
	out << fmt::format( papuga::cppCodeSnippet( 0,
		"#ifndef _PAPUGA_{modulename}_LUA_INTERFACE__INCLUDED",
		"#define _PAPUGA_{modulename}_LUA_INTERFACE__INCLUDED",
		"/* @remark GENERATED FILE (libpapuga_lua_gen) - DO NOT MODIFY */",
		"",
		"#include \"lua.h\"",
		"#ifdef __cplusplus",
		"extern \"C\" {",
		"#endif",
		"int luaopen_{modulename}( lua_State* ls);",
		"",
		"#ifdef __cplusplus",
		"}",
		"#endif",
		"#endif",
		0),
		fmt::arg("modulename", modulename)
	) << std::endl;
}

void papuga::printLuaModSource(
		std::ostream& out,
		const papuga_InterfaceDescription& descr,
		const std::vector<std::string>& includes)
{
	out << papuga::cppCodeSnippet( 0,
		"#include \"lauxlib.h\"",
		"#include \"papuga.h\"",
		"#include \"papuga/lib/lua_dev.h\"", 0);

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
	out << "/* @remark GENERATED FILE (libpapuga_lua_gen) - DO NOT MODIFY */" << std::endl;
	out << std::endl;

	define_classentrymap( out, descr);

	std::size_t ci;
	for (ci=0; descr.classes[ci].name; ++ci)
	{
		const papuga_ClassDescription& classdef = descr.classes[ci];
		if (classdef.constructor)
		{
			define_constructor( out, ci+1/*classid*/, descr, classdef);
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

