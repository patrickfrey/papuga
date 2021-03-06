/*
 * Copyright (c) 2019 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/// \brief Structures and functions for describing a request result 
/// \file requestResult.cpp
#include "papuga/requestResult.h"
#include "papuga/allocator.h"
#include "papuga/serialization.h"
#include "papuga/serialization.hpp"
#include "papuga/valueVariant.h"
#include "papuga/valueVariant.hpp"
#include "papuga/errors.h"
#include "papuga/errors.hpp"
#include <stdexcept>
#include <sstream>
#include <iostream>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <algorithm>
#include <limits>

#undef PAPUGA_LOWLEVEL_DEBUG
#define INIT_RESULT_SIZE 128
#define MAX_RESULT_SIZE (1<<20)


using namespace papuga;

extern "C" const char* papuga_RequestResultNodeTypeName( papuga_RequestResultNodeType tp)
{
	static const char* ar[] = {"Constant", "OpenStructure", "CloseStructure", "OpenArray", "CloseArray", "InputReference", "ResultReference", 0};
	return ar[ tp];
}

extern "C" papuga_RequestResultDescription* papuga_create_RequestResultDescription( const char* name_, const char* schema_, const char* requestmethod_, const char* addressvar_, const char* path_)
{
	papuga_RequestResultDescription* rt = (papuga_RequestResultDescription*)std::malloc( sizeof( papuga_RequestResultDescription));
	if (!rt) return NULL;
	rt->name = name_;
	rt->schema = schema_;
	rt->requestmethod = requestmethod_;
	rt->addressvar = addressvar_;
	rt->path = path_;
	rt->nodear = 0;
	rt->nodearallocsize = 0;
	rt->nodearsize = 0;
	std::memset( rt->contentvar, 0, sizeof(rt->contentvar));
	rt->contentvarsize = 0;
	return rt;
}

void papuga_destroy_RequestResultDescription( papuga_RequestResultDescription* self)
{
	if (!self) return;
	if (self->nodear) std::free( self->nodear);
	std::free( self);
}

static papuga_RequestResultNodeDescription* allocNode( papuga_RequestResultDescription* descr)
{
	if (!descr->nodear)
	{
		descr->nodear = (papuga_RequestResultNodeDescription*)std::malloc( INIT_RESULT_SIZE * sizeof(papuga_RequestResultNodeDescription));
		if (!descr->nodear) return NULL;
	}
	else if (descr->nodearsize == descr->nodearallocsize)
	{
		if (descr->nodearsize <= MAX_RESULT_SIZE) return 0;
		int newallocsize = descr->nodearallocsize * 2;
		papuga_RequestResultNodeDescription* newar = (papuga_RequestResultNodeDescription*)std::realloc( descr->nodear, newallocsize * sizeof(papuga_RequestResultNodeDescription));
		if (!newar) return 0;
		descr->nodear = newar;
	}
	return &descr->nodear[ descr->nodearsize++];
}

extern "C" bool papuga_RequestResultDescription_push_constant( papuga_RequestResultDescription* descr, const char* inputselect, const char* tagname, const char* constant)
{
	papuga_RequestResultNodeDescription* nd = allocNode( descr);
	if (!nd) return false;
	nd->inputselect = inputselect;
	nd->type = papuga_ResultNodeConstant;
	nd->resolvetype = papuga_ResolveTypeRequired;
	nd->tagname = tagname;
	nd->value.str = constant;
	return true;
}

extern "C" bool papuga_RequestResultDescription_push_structure( papuga_RequestResultDescription* descr, const char* inputselect, const char* tagname, bool array)
{
	papuga_RequestResultNodeDescription* nd_open = allocNode( descr);
	papuga_RequestResultNodeDescription* nd_close = allocNode( descr);
	if (!nd_open || !nd_close) return false;

	nd_open->inputselect = inputselect;
	nd_open->type = array ? papuga_ResultNodeOpenArray : papuga_ResultNodeOpenStructure;
	nd_open->tagname = tagname;
	nd_open->resolvetype = papuga_ResolveTypeRequired;
	nd_open->value.str = NULL;

	nd_close->inputselect = inputselect;
	nd_close->type = array ? papuga_ResultNodeCloseArray : papuga_ResultNodeCloseStructure;
	nd_close->resolvetype = papuga_ResolveTypeRequired;
	nd_close->tagname = tagname;
	nd_close->value.str = NULL;
	return true;
}

extern "C" bool papuga_RequestResultDescription_push_input( papuga_RequestResultDescription* descr, const char* inputselect, const char* tagname, int itemid, papuga_ResolveType resolvetype)
{
	papuga_RequestResultNodeDescription* nd = allocNode( descr);
	if (!nd) return false;
	nd->inputselect = inputselect;
	nd->type = papuga_ResultNodeInputReference;
	nd->resolvetype = resolvetype;
	nd->tagname = tagname;
	nd->value.itemid = itemid;
	return true;
}

extern "C" bool papuga_RequestResultDescription_push_callresult( papuga_RequestResultDescription* descr, const char* inputselect, const char* tagname, const char* variable, papuga_ResolveType resolvetype)
{
	papuga_RequestResultNodeDescription* nd = allocNode( descr);
	if (!nd) return false;
	nd->inputselect = inputselect;
	nd->type = papuga_ResultNodeResultReference;
	nd->resolvetype = resolvetype;
	nd->tagname = tagname;
	nd->value.str = variable;
	return true;
}

extern "C" bool papuga_RequestResultDescription_push_content_variable( papuga_RequestResultDescription* descr, const char* variable)
{
	if (descr->contentvarsize >= papuga_RequestResultDescription_MaxNofContentVars) return false;
	descr->contentvar[ descr->contentvarsize++] = variable;
	descr->contentvar[ descr->contentvarsize] = 0;
	return true;
}

