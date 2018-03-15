/*
 * Copyright (c) 2017 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/* \brief Structure to build and map the Result of an XML/JSON request
 * @file requestResult.cpp
 */
#include "papuga/requestResult.h"
#include "papuga/serialization.hpp"
#include "papuga/valueVariant.hpp"
#include "papuga/valueVariant.h"
#include "papuga/allocator.h"
#include <string>
#include <cstring>
#include <cstdlib>
#include <iostream>
#include <sstream>

extern "C" char* papuga_RequestResult_tostring( const papuga_RequestResult* self, int maxdepth, size_t* len)
{
	papuga_ErrorCode errcode = papuga_Ok;
	try
	{
		std::ostringstream dump;
		dump << "ROOT " << self->name << "\n";
		papuga_RequestResultNode const* ni = self->nodes;
		for (; ni; ni = ni->next)
		{
			dump << "NODE " << ni->name << "\n";
			if (!papuga_ValueVariant_defined( &ni->value))
			{
				dump << "\tNULL\n";
			}
			else if (papuga_ValueVariant_isatomic( &ni->value))
			{
				dump << "\t" << papuga::ValueVariant_tostring( ni->value, errcode);
			}
			else if (ni->value.valuetype == papuga_TypeSerialization)
			{
				dump << papuga::Serialization_tostring( *ni->value.value.serialization, false/*linemode*/, maxdepth, errcode);
			}
			else
			{
				dump << "\t<" << papuga_Type_name(ni->value.valuetype) << ">\n";
			}
		}
		std::string res( dump.str());
		char* rt = (char*)std::malloc( res.size() +1);
		if (!rt) return NULL;
		papuga_Allocator_add_free_mem( self->allocator, rt);
		std::memcpy( rt, res.c_str(), res.size() +1);
		*len = res.size();
		return rt;
	}
	catch (const std::bad_alloc&)
	{
		return NULL;
	}
}

bool papuga_init_RequestResult_single( papuga_RequestResult* self, papuga_Allocator* allocator, const char* rootname, const char* elemname, const papuga_StructInterfaceDescription* structdefs, const papuga_ValueVariant* value)
{
	self->allocator = allocator;
	self->name = rootname ? papuga_Allocator_copy_charp( allocator, rootname) : 0;
	papuga_RequestResultNode* node = (papuga_RequestResultNode*)papuga_Allocator_alloc( allocator, sizeof(papuga_RequestResultNode), 0);
	if (!node || (rootname && !self->name)) return false;
	self->structdefs = structdefs;
	self->nodes = node;
	papuga_init_ValueVariant_value( &node->value, value);
	node->name = papuga_Allocator_copy_charp( allocator, elemname);
	node->name_optional = true;
	node->next = NULL;
	return true;
}


