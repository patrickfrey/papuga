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

static papuga_RequestResultNodeDescription* allocNode( papuga_RequestResultDescription* descr)
{
	if (!descr->nodear)
	{
		descr->nodear = (papuga_RequestResultNodeDescription*)papuga_Allocator_alloc( descr->allocator, INIT_RESULT_SIZE * sizeof(papuga_RequestResultNodeDescription), 0/*default alignment*/);
		if (!descr->nodear) return 0;
	}
	else if (descr->nodearsize == descr->nodearallocsize)
	{
		if (descr->nodearsize <= MAX_RESULT_SIZE) return 0;
		int newallocsize = descr->nodearallocsize * 2;
		papuga_RequestResultNodeDescription* newar = (papuga_RequestResultNodeDescription*)papuga_Allocator_alloc( descr->allocator, newallocsize * sizeof(papuga_RequestResultNodeDescription), 0/*default alignment*/);
		if (!newar) return 0;
		std::memcpy( newar, descr->nodear, descr->nodearsize * sizeof(papuga_RequestResultNodeDescription));
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
	nd->tagname = tagname;
	nd->value.constant = constant;
	return true;
}

extern "C" bool papuga_RequestResultDescription_push_structure( papuga_RequestResultDescription* descr, const char* inputselect, const char* tagname)
{
	papuga_RequestResultNodeDescription* nd_open = allocNode( descr);
	papuga_RequestResultNodeDescription* nd_close = allocNode( descr);
	if (!nd_open || !nd_close) return false;

	std::size_t inputselectlen = std::strlen( inputselect);
	char* closeselect = (char*)papuga_Allocator_alloc( descr->allocator, inputselectlen+1, 1);
	if (!closeselect) return false;
	std::memcpy( closeselect, inputselect, inputselectlen);
	closeselect[ inputselectlen+0] = '~';
	closeselect[ inputselectlen+1] = 0;

	nd_open->inputselect = inputselect;
	nd_open->type = papuga_ResultNodeOpenStructure;
	nd_open->tagname = tagname;
	nd_open->value.constant = NULL;

	nd_close->inputselect = closeselect;
	nd_close->type = papuga_ResultNodeCloseStructure;
	nd_close->tagname = tagname;
	nd_close->value.constant = NULL;
	return true;
}

extern "C" bool papuga_RequestResultDescription_push_input( papuga_RequestResultDescription* descr, const char* inputselect, const char* tagname, int nodeid)
{
	papuga_RequestResultNodeDescription* nd = allocNode( descr);
	if (!nd) return false;
	nd->inputselect = inputselect;
	nd->type = papuga_ResultNodeInputReference;
	nd->tagname = tagname;
	nd->value.nodeid = nodeid;
	return true;
}

extern "C" bool papuga_RequestResultDescription_push_callresult( papuga_RequestResultDescription* descr, const char* inputselect, const char* tagname, const char* variable)
{
	papuga_RequestResultNodeDescription* nd = allocNode( descr);
	if (!nd) return false;
	nd->inputselect = inputselect;
	nd->type = papuga_ResultNodeResultReference;
	nd->tagname = tagname;
	nd->value.variable = variable;
	return true;
}



