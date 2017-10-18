/*
* Copyright (c) 2017 Patrick P. Frey
*
* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/
/// \brief Some utility functions for generating language binding sources
/// \file private/sourceDoc.hpp
#ifndef _PAPUGA_UTILS_SOURCE_DOC_HPP_INCLUDED
#define _PAPUGA_UTILS_SOURCE_DOC_HPP_INCLUDED
#include "papuga/interfaceDescription.h"
#include <string>
#include <map>
#include <stdexcept>

namespace papuga {

struct SourceDocExampleNode
{
	const char* proc;
	const char* name;
	const char* value;
	struct SourceDocExampleNode* next;
	struct SourceDocExampleNode* chld;

	SourceDocExampleNode()
		:proc(0),name(0),value(0),next(0),chld(0){}
	SourceDocExampleNode( const SourceDocExampleNode& o)
		:proc(o.proc),name(o.name),value(o.value),next(o.next),chld(o.chld){}
};


class SourceDocLanguageDescription
{
public:
	virtual const char* eolncomment() const=0;
	virtual std::string classStartDeclaration( const papuga_ClassDescription* classdef) const=0;
	virtual std::string classEndDeclaration( const papuga_ClassDescription* classdef) const=0;
	virtual std::string mapCodeExample( const SourceDocExampleNode* example) const=0;
	virtual std::string constructorDeclaration( const std::string& classname, const papuga_ConstructorDescription* cdef) const=0;
	virtual std::string methodDeclaration( const std::string& classname, const papuga_MethodDescription* mdef) const=0;
private:
};

void printSourceDoc( std::ostream& out, const SourceDocLanguageDescription* lang, const papuga_InterfaceDescription& descr);

}//namespace
#endif

