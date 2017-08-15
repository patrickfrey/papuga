/*
* Copyright (c) 2017 Patrick P. Frey
*
* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/
/// \brief Function to print source documentation out of a language description
/// \file sourceDoc.cpp
#include "private/sourceDoc.hpp"
#include <cstdarg>
#include <iostream>
#include <sstream>
#include <cstdio>
#include <cstring>
#include <stdint.h>
#include <string>
#include <vector>
#include <stdexcept>
#include <algorithm>

using namespace papuga;

static void printDocumentationHdr(
		std::ostream& out,
		const SourceDocLanguageDescription* lang,
		const char* tag,
		const std::string& value)
{
	if (tag)
	{
		out << lang->eolncomment() << " @" << tag << " " << value << std::endl;
	}
	else
	{
		out << lang->eolncomment() << " " << value << std::endl;
	}
}

static void printDocumentationTag(
		std::ostream& out,
		const SourceDocLanguageDescription* lang,
		const char* tag,
		const char* value)
{
	if (!value || !value[0]) return;
	char const* si = value;
	char const* sn = std::strchr( si,'\n');
	if (sn)
	{
		printDocumentationHdr( out, lang, tag, std::string( si, sn-si));
		for (sn = std::strchr( si=sn+1,'\n'); sn; sn = std::strchr( si=sn+1,'\n'))
		{
			out << lang->eolncomment() << " \t" << std::string( si, sn-si) << std::endl;
		}
		out << lang->eolncomment() << " \t" << std::string( si) << std::endl;
	}
	else
	{
		printDocumentationHdr( out, lang, tag, si);
	}
}

static void printAnnotations(
		std::ostream& out,
		const SourceDocLanguageDescription* lang,
		const papuga_Annotation* ann)
{
	papuga_Annotation const* di = ann;
	for (; di->text; ++di)
	{
		switch (di->type)
		{
			case papuga_AnnotationType_Description:
				printDocumentationTag( out, lang, "brief", di->text);
				break;
			case papuga_AnnotationType_Example:
			{
				std::string examplecode = lang->mapCodeExample( di->text);
				printDocumentationTag( out, lang, "usage", examplecode.c_str());
				break;
			}
			case papuga_AnnotationType_Note:
				printDocumentationTag( out, lang, "note", di->text);
				break;
			case papuga_AnnotationType_Remark:
				printDocumentationTag( out, lang, "remark", di->text);
				break;
		}
	}
}

static const char* getAnnotationText(
		const papuga_Annotation* ann,
		const papuga_AnnotationType type)
{
	papuga_Annotation const* di = ann;
	for (; di->text; ++di)
	{
		if (di->type == type)
		{
			return di->text;
		}
	}
	return 0;
}

static void printParameterDescription(
		std::ostream& out,
		const SourceDocLanguageDescription* lang,
		const papuga_ParameterDescription* parameter)
{
	if (!parameter) return;
	papuga_ParameterDescription const* pi = parameter;
	for (; pi->name; ++pi)
	{
		const char* description = getAnnotationText( pi->doc, papuga_AnnotationType_Description);
		char buf[ 4096];
		std::snprintf( buf, sizeof(buf), "%s %s%s",
				pi->name,
				pi->mandatory?"":"(optional) ",
				description?description:"");
		buf[ sizeof(buf)-1] = '\0';
		printDocumentationTag( out, lang, "param", buf);
		printAnnotations( out, lang, pi->doc);
	}
}

static void printConstructor(
		std::ostream& out,
		const SourceDocLanguageDescription* lang,
		const std::string& classname,
		const papuga_ConstructorDescription* cdef)
{
	if (!cdef) return;
	printDocumentationTag( out, lang, "constructor", "new");

	printAnnotations( out, lang, cdef->doc);

	printParameterDescription( out, lang, cdef->parameter);
	out << lang->constructorDeclaration( classname, cdef) << std::endl;
}

static void printMethod(
		std::ostream& out,
		const SourceDocLanguageDescription* lang,
		const std::string& classname,
		const papuga_MethodDescription* mdef)
{
	if (!mdef) return;
	printDocumentationTag( out, lang, "method", mdef->name);

	printAnnotations( out, lang, mdef->doc);

	printParameterDescription( out, lang, mdef->parameter);
	out << lang->methodDeclaration( classname, mdef) << std::endl;
}

void papuga::printSourceDoc(
		std::ostream& out,
		const SourceDocLanguageDescription* lang,
		const papuga_InterfaceDescription& descr)
{
	printDocumentationTag( out, lang, "project", descr.name);
	if (descr.about)
	{
		printDocumentationTag( out, lang, "author ", descr.about->author);
		printDocumentationTag( out, lang, "contributors ", descr.about->contributors);
		printDocumentationTag( out, lang, "copyright", descr.about->copyright);
		printDocumentationTag( out, lang, "license", descr.about->license);
		printDocumentationTag( out, lang, "release", descr.about->version);
		printDocumentationTag( out, lang, "url", descr.about->url);
	}
	std::size_t ci;
	for (ci=0; descr.classes[ci].name; ++ci)
	{
		const papuga_ClassDescription& cdef = descr.classes[ci];
		printDocumentationTag( out, lang, "class", cdef.name);

		printAnnotations( out, lang, cdef.doc);
		out << lang->classStartDeclaration( &cdef);

		printConstructor( out, lang, cdef.name, cdef.constructor);
		std::size_t mi = 0;
		for (; cdef.methodtable[mi].name; ++mi)
		{
			printMethod( out, lang, cdef.name, &cdef.methodtable[mi]);
		}
		out << lang->classEndDeclaration( &cdef);
		out << std::endl;
	}
}

