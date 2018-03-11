/*
 * Copyright (c) 2017 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/// \brief Expand a request result as JSON
/// \file requestResult_json.cpp
#include "papuga/requestResult.h"
#include "papuga/serialization.h"
#include "papuga/valueVariant.hpp"
#include "papuga/valueVariant.h"
#include "papuga/allocator.h"
#include "papuga/typedefs.h"
#include "papuga/constants.h"
#include "papuga/callResult.h"
#include "papuga/interfaceDescription.h"
#include "papuga/stack.h"
#include "papuga/errors.hpp"
#include "requestResult_utils.hpp"
#include <string>
#include <cstring>

static bool SerializationIter_tojson( std::string& out, papuga_SerializationIter* seritr, bool isdict, int structid, const papuga_StructInterfaceDescription* structs, const std::string& indent, papuga_ErrorCode& errcode);

static void append_attribute_name( std::string& out, const char* name)
{
	out.push_back( '"');
	out.append( name);
	out.append( "\": ");
}

static bool append_value( std::string& out, const papuga_ValueVariant& value, papuga_ErrorCode& errcode)
{
	if (!papuga_ValueVariant_defined( &value))
	{
		out.append( "null");
	}
	else if (value.valuetype == papuga_TypeBool)
	{
		out.append( value.value.Bool ? "true" : "false");
	}
	else if (papuga_ValueVariant_isstring( &value))
	{
		out.push_back( '"');
		if (!papuga::ValueVariant_append_string( out, value, errcode)) return false;
		out.push_back( '"');
	}
	else
	{
		if (!papuga::ValueVariant_append_string( out, value, errcode)) return false;
	}
	return true;
}

static void incindent( std::string& indent)
{
	indent.push_back( '\t');
}
static void decindent( std::string& indent)
{
	indent.pop_back();
}

static bool ValueVariant_tojson( std::string& out, const papuga_ValueVariant& value, const papuga_StructInterfaceDescription* structs, const std::string& indent, papuga_ErrorCode& errcode)
{
	bool rt = true;
	try
	{
		if (papuga_ValueVariant_isatomic(&value))
		{
			rt &= append_value( out, value, errcode);
		}
		else if (value.valuetype == papuga_TypeSerialization)
		{
			papuga_SerializationIter subitr;
			papuga_init_SerializationIter( &subitr, value.value.serialization);
			if (!papuga_SerializationIter_eof( &subitr))
			{
				bool isdict = value.value.serialization->structid || papuga_SerializationIter_tag( &subitr) == papuga_TagName;

				out.push_back( isdict?'{':'[');
				if (SerializationIter_tojson( out, &subitr, isdict, value.value.serialization->structid, structs, indent+'\t', errcode))
				{
					out.push_back( isdict?'}':']');
	
					if (!papuga_SerializationIter_eof( &subitr))
					{
						errcode = papuga_SyntaxError;
						rt = false;
					}
				}
				else
				{
					rt = false;
				}
			}
		}
		else if (value.valuetype == papuga_TypeIterator)
		{
			int itercnt = 0;
			papuga_Allocator allocator;
			papuga_CallResult result;
			int result_mem[ 1024];
			char error_mem[ 128];
			papuga_Iterator* iterator = value.value.iterator;

			papuga_init_Allocator( &allocator, result_mem, sizeof(result_mem));
			papuga_init_CallResult( &result, &allocator, true, error_mem, sizeof(error_mem));
			while (itercnt++ < PAPUGA_MAX_ITERATOR_EXPANSION_LENGTH && rt && iterator->getNext( iterator->data, &result))
			{
				out.push_back('[');
				int ri = 0, re = result.nofvalues;
				for (; ri != re; ++ri)
				{
					if (ri) out.push_back( ',');
					rt &= ValueVariant_tojson( out, result.valuear[ri], structs, indent+'\t', errcode);
				}
				out.push_back( ']');
				papuga_destroy_CallResult( &result);
				if (!rt)
				{
					errcode = papuga_NoMemError;
					break;
				}
				papuga_init_Allocator( &allocator, result_mem, sizeof(result_mem));
				papuga_init_CallResult( &result, &allocator, true, error_mem, sizeof(error_mem));
			}
			if (papuga_CallResult_hasError( &result))
			{
				errcode = papuga_IteratorFailed;
				rt = false;
			}
			papuga_destroy_CallResult( &result);
		}
		else if (!papuga_ValueVariant_defined( &value))
		{}
		else
		{
			errcode = papuga_TypeError;
			return false;
		}
		if (!rt && errcode == papuga_Ok)
		{
			errcode = papuga_NoMemError;
		}
	}
	catch (...)
	{
		errcode = papuga_NoMemError;
		rt = false;
	}
	return rt;
}

struct SerializationIterStackElem
{
	int parent_structid;
	int parent_elementcnt;
	bool parent_isdict;
	char name[ 128];
};

static bool SerializationIter_tojson( std::string& out, papuga_SerializationIter* seritr, bool isdict, int structid, const papuga_StructInterfaceDescription* structs, const std::string& indent_, papuga_ErrorCode& errcode)
{
	bool rt = true;
	papuga_Stack namestk;
	int namestk_mem[ 1024];
	const char* name = 0;
	int elementcnt = 0;
	SerializationIterStackElem namebuf;
	std::string indent( indent_);

	papuga_init_Stack( &namestk, sizeof(SerializationIterStackElem), 128, namestk_mem, sizeof(namestk_mem));
	try
	{
		for (; rt; papuga_SerializationIter_skip(seritr)) switch( papuga_SerializationIter_tag(seritr))
		{
			case papuga_TagClose:
			{
				if (name)
				{
					errcode = papuga_SyntaxError;
					goto ERROR;
				}
				SerializationIterStackElem* stkelem = (SerializationIterStackElem*)papuga_Stack_pop( &namestk);
				if (stkelem)
				{
					decindent( indent);
					if (isdict)
					{
						out.push_back( '}');
					}
					else
					{
						out.push_back( ']');
					}
					elementcnt = stkelem->parent_elementcnt+1;
					structid = stkelem->parent_structid;
					isdict = stkelem->parent_isdict;
				}
				else
				{
					papuga_destroy_Stack( &namestk);
					return true;
				}
				break;
			}
			case papuga_TagValue:
			{
				const papuga_ValueVariant* value = papuga_SerializationIter_value(seritr);
				if (elementcnt)
				{
					out.push_back( ',');
				}
				if (isdict)
				{
					if (!name)
					{
						if (structid)
						{
							name = structs[ structid-1].members[ elementcnt].name;
							if (!name)
							{
								errcode = papuga_SyntaxError;
								goto ERROR;
							}
						}
						else
						{
							errcode = papuga_SyntaxError;
							goto ERROR;
						}
					}
					out.push_back( '\n');
					out.append( indent);
					append_attribute_name( out, name);
				}
				++elementcnt;
				rt &= ValueVariant_tojson( out, *value, structs, indent, errcode);
				name = NULL;
				break;
			}
			case papuga_TagOpen:
			{
				const papuga_ValueVariant* value = papuga_SerializationIter_value(seritr);
				if (elementcnt)
				{
					out.push_back( ',');
				}
				if (isdict)
				{
					if (!name)
					{
						if (structid)
						{
							name = structs[ structid-1].members[ elementcnt].name;
							if (!name)
							{
								errcode = papuga_SyntaxError;
								goto ERROR;
							}
						}
						else
						{
							errcode = papuga_SyntaxError;
							goto ERROR;
						}
					}
					out.push_back( '\n');
					out.append( indent);
					append_attribute_name( out, name);
				}
				++elementcnt;
				SerializationIterStackElem* stkelem = (SerializationIterStackElem*)papuga_Stack_push( &namestk);
				size_t namelen = (isdict) ? std::snprintf( stkelem->name, sizeof(stkelem->name), "%s", name) : 0;
				stkelem->parent_structid = structid;
				stkelem->parent_elementcnt = elementcnt;
				stkelem->parent_isdict = isdict;
				if (sizeof(stkelem->name) <= namelen)
				{
					errcode = papuga_BufferOverflowError;
					goto ERROR;
				}
				stkelem->name[ namelen] = 0;
				if (papuga_ValueVariant_defined( value))
				{
					structid = papuga_ValueVariant_toint( value, &errcode);
					if (errcode != papuga_Ok) goto ERROR;
				}
				if (structid)
				{
					isdict = true;
				}
				else
				{
					papuga_SerializationIter seritr_follow;
					papuga_init_SerializationIter_copy( &seritr_follow, seritr);
					papuga_SerializationIter_skip( &seritr_follow);
					isdict = structid || papuga_SerializationIter_tag( &seritr_follow) == papuga_TagName;
				}
				if (isdict)
				{
					out.push_back( '{');
				}
				else
				{
					out.push_back( '[');
				}
				elementcnt = 0;
				incindent( indent);
				name = NULL;
				break;
			}
			case papuga_TagName:
			{
				size_t namelen;
				if (name || !isdict)
				{
					errcode = papuga_SyntaxError;
					goto ERROR;
				}
				if (!papuga_ValueVariant_tostring_enc( papuga_SerializationIter_value(seritr), papuga_UTF8, namebuf.name, sizeof(namebuf.name)-1, &namelen, &errcode))
				{
					goto ERROR;
				}
				namebuf.name[ namelen] = 0;
				name = namebuf.name;
			}
		}
		if (!rt && errcode == papuga_Ok)
		{
			errcode = papuga_NoMemError;
		}
	}
	catch (...)
	{
		errcode = papuga_NoMemError;
		rt = false;
	}
	papuga_destroy_Stack( &namestk);
	return rt;
ERROR:
	papuga_destroy_Stack( &namestk);
	return false;
}

extern "C" void* papuga_RequestResult_tojson( const papuga_RequestResult* self, papuga_StringEncoding enc, size_t* len, papuga_ErrorCode* err)
{
	try
	{
		std::string out;
		const char* rootelem = self->name;
		std::string indent;
		out.push_back( '{');
		if (rootelem)
		{
			out.push_back( '\n');
			incindent( indent);
			out.append( indent);
			append_attribute_name( out, rootelem);
			out.push_back( '{');
		}
		incindent( indent);
		if (self->nodes && !self->nodes->next && self->nodes->name_optional)
		{
			if (!ValueVariant_tojson( out, self->nodes->value, self->structdefs, indent, *err)) return NULL;
		}
		else
		{
			papuga_RequestResultNode const* nd = self->nodes;
			for (int ndcnt=0; nd; nd = nd->next, ++ndcnt)
			{
				if (ndcnt) out.push_back( ',');
				out.push_back( '\n');
				out.append( indent);
				append_attribute_name( out, nd->name);
				if (!ValueVariant_tojson( out, nd->value, self->structdefs, indent, *err)) return NULL;
			}
		}
		decindent( indent);
		if (rootelem)
		{
			out.push_back( '}');
		}
		out.append( "\n}\n");

		void* rt = papuga::encodeRequestResultString( out, enc, len, err);
		if (rt) papuga_Allocator_add_free_mem( self->allocator, rt);
		return rt;
	}
	catch (const std::bad_alloc&)
	{
		*err = papuga_NoMemError;
		return NULL;
	}
	catch (...)
	{
		*err = papuga_UncaughtException;
		return NULL;
	}
}


