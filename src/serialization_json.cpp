/*
 * Copyright (c) 2019 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/// \brief Serialize json structure
/// \file serialization_json.cpp
#include "papuga/serialization.h"
#include "papuga/valueVariant.h"
#include "papuga/allocator.h"
#include "papuga/serialization.h"
#include "papuga/constants.h"
#include "cjson/cJSON.h"
#include "requestParser_utils.h"
#include <cstdlib>
#include <cstring>
#include <new>

static const char* copyString( papuga_Serialization* self, const char* str)
{
	const char* valuestr = papuga_Allocator_copy_string( self->allocator, str, std::strlen(str));
	if (!valuestr) throw std::bad_alloc();
	return valuestr;
}

static bool Serialization_append_json( papuga_Serialization* self, const cJSON* nd, bool isDict, int depth, papuga_ErrorCode* errcode)
{
	bool rt = true;
	if (depth > PAPUGA_MAX_RECURSION_DEPTH)
	{
		*errcode = papuga_MaxRecursionDepthReached;
		return false;
	}
	if (isDict && !nd->string)
	{
		*errcode = papuga_SyntaxError;
		return false;
	}
	switch (nd->type & 0x7F)
	{
		case cJSON_False:
			if (nd->string) rt &= papuga_Serialization_pushName_charp( self, copyString( self, nd->string));
			rt &= papuga_Serialization_pushValue_bool( self, false);
			break;
		case cJSON_True:
			if (nd->string) rt &= papuga_Serialization_pushName_charp( self, copyString( self, nd->string));
			rt &= papuga_Serialization_pushValue_bool( self, true);
			break;
		case cJSON_NULL:
			if (nd->string && nd->string[0] != '-' && nd->string[0] != '#')
			{
				if (nd->string) rt &= papuga_Serialization_pushName_charp( self, copyString( self, nd->string));
				rt &= papuga_Serialization_pushValue_void( self);
			}
			break;
		case cJSON_String:
			if (nd->string) rt &= papuga_Serialization_pushName_charp( self, copyString( self, nd->string));
			rt &= papuga_Serialization_pushValue_charp( self, copyString( self, nd->valuestring));
			break;
		case cJSON_Number:
			if (!nd->valuestring)
			{
				*errcode = papuga_ValueUndefined;
				return false;
			}
			if (nd->string) rt &= papuga_Serialization_pushName_charp( self, copyString( self, nd->string));
			rt &= papuga_Serialization_pushValue_charp( self, copyString( self, nd->valuestring));
			break;
		case cJSON_Array:
		case cJSON_Object:
		{
			bool isDictParent = (nd->type & 0x7F) == cJSON_Object;
			cJSON const* chnd = nd->child;
			if (nd->string) rt &= papuga_Serialization_pushName_charp( self, copyString( self, nd->string));
			rt &= papuga_Serialization_pushOpen( self);
			for (;chnd; chnd = chnd->next)
			{
				rt &= Serialization_append_json( self, chnd, isDictParent, depth+1, errcode);
			}
			rt &= papuga_Serialization_pushClose( self);
			break;
		}
		default:
			*errcode = papuga_LogicError;
			return false;
	}
	return rt;
}

extern "C" bool papuga_Serialization_append_json( papuga_Serialization* self, const char* content, size_t contentlen, papuga_StringEncoding enc, papuga_ErrorCode* errcode)
{
	bool rt = false;
	if (enc != papuga_UTF8)
	{
		// Convert input to UTF8 as cjson is only capable of parsing UTF8
		papuga_ValueVariant val;
		papuga_init_ValueVariant_string_enc( &val, enc, content, contentlen);
		content = papuga_ValueVariant_tostring( &val, self->allocator, &contentlen, errcode);
		if (!content) return false;
	}
	if (content[contentlen] != 0)
	{
		content = papuga_Allocator_copy_string( self->allocator, content, contentlen);
	}
	cJSON_Context ctx;
	cJSON* tree = cJSON_Parse( content, &ctx);
	if (!tree)
	{
		if (ctx.position < 0)
		{
			*errcode = papuga_NoMemError;
		}
		else
		{
			*errcode = papuga_SyntaxError;
		}
		return false;
	}
	else if (tree->child)
	{
		try
		{
			rt = Serialization_append_json( self, tree->child, true/*isDict*/, 0/*depth*/, errcode);
		}
		catch (const std::bad_alloc&)
		{
			*errcode = papuga_NoMemError;
			rt = false;
		}
	}
	else if (tree->valuestring)
	{
		const char* valuestr = papuga_Allocator_copy_string( self->allocator, tree->valuestring, std::strlen(tree->valuestring));
		if (!valuestr)
		{
			*errcode = papuga_NoMemError;
			rt = false;
		}
		else
		{
			rt = papuga_Serialization_pushValue_charp( self, valuestr);
		}
	}
	else
	{
		rt = papuga_Serialization_pushValue_void( self);
	}
	cJSON_Delete( tree);
	return rt;
}



