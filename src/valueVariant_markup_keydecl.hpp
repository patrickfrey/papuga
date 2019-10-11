/*
 * Copyright (c) 2019 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
#ifndef _PAPUGA_REQUEST_VALUE_VARIANT_MARKUP_KEYDECL_HPP_INCLUDED
#define _PAPUGA_REQUEST_VALUE_VARIANT_MARKUP_KEYDECL_HPP_INCLUDED
#include "valueVariant_markup_base.hpp"

namespace papuga {
namespace markup {

/// \brief Template for output of name value pair declaration markup languages like JSON and TEXT
/// \note Using curiously recurring template pattern to inject the higher level methods that implement the concept of a key value declaration markup
template <class OutputContextClass>
class KeyDeclOutputContext
	:public OutputContextBase
{
public:
	KeyDeclOutputContext( const papuga_StructInterfaceDescription* structs_, int maxDepth_)
		:OutputContextBase(structs_,maxDepth_){}

	void defValue( const papuga_ValueVariant& value, bool valueIsLink)
	{
		if (valueIsLink)
		{
			OutputContextClass::appendLinkIdElem( value);
		}
		else if (!papuga_ValueVariant_defined(&value))
		{
			OutputContextClass::appendNull();
		}
		else if (papuga_ValueVariant_isatomic(&value))
		{
			OutputContextClass::appendAtomicValueElem( value);
		}
		else if (value.valuetype == papuga_TypeSerialization)
		{
			appendSerialization( *value.value.serialization, valueIsLink);
		}
		else if (value.valuetype == papuga_TypeIterator)
		{
			appendIterator( *value.value.iterator, valueIsLink);
		}
		else
		{
			throw ErrorException( papuga_TypeError);
		}
	}

	void appendSerialization( const papuga_Serialization& ser, bool valueIsLink)
	{
		int structid = papuga_Serialization_structid( &ser);
		papuga_SerializationIter iter;
		papuga_init_SerializationIter( &iter, &ser);
		appendSerializationIter( iter, valueIsLink, structid);
		if (!papuga_SerializationIter_eof(&iter))
		{
			throw ErrorException( papuga_SyntaxError);
		}
	}

	void appendSerializationIterElement( papuga_SerializationIter& iter, bool valueIsLink)
	{
		if (papuga_SerializationIter_tag( &iter) == papuga_TagValue)
		{
			defValue( *papuga_SerializationIter_value( &iter), valueIsLink);
			papuga_SerializationIter_skip( &iter);
		}
		else if (papuga_SerializationIter_tag( &iter) == papuga_TagOpen)
		{
			int structid = papuga_SerializationIter_value( &iter)->value.Int;
			OutputContextClass::defOpen();
			papuga_SerializationIter_skip( &iter);
			appendSerializationIter( iter, valueIsLink, structid);
			consumeClose( iter);
			OutputContextClass::defClose();
		}
		else
		{
			throw ErrorException( papuga_SyntaxError);
		}
	}

	void appendSerializationIter( papuga_SerializationIter& iter, bool valueIsLink, int structid)
	{
		if (structid)
		{
			OutputContextClass::openStruct();
			if (depth >= maxDepth)
			{
				if (!papuga_SerializationIter_skip_structure( &iter)) throw ErrorException( papuga_SyntaxError);
				OutputContextClass::appendUnspecifiedStructure();
			}
			else
			{
				int ecnt = 0;
				for (; papuga_SerializationIter_tag(&iter) != papuga_TagClose; papuga_SerializationIter_skip(&iter))
				{
					if (++ecnt) OutputContextClass::appendSeparator();
					const char* name = structs[ structid-1].members[ ecnt].name;
					if (!name) throw ErrorException( papuga_SyntaxError);
					valueIsLink = isEqual( name, PAPUGA_HTML_LINK_ELEMENT);
					OutputContextClass::defOpen();
					OutputContextClass::defName( name);
					appendSerializationIterElement( iter, valueIsLink);
					OutputContextClass::defClose();
				}
			}
			OutputContextClass::closeStruct();
		}
		else if (papuga_SerializationIter_tag( &iter) == papuga_TagName)
		{
			OutputContextClass::openStruct();
			if (depth >= maxDepth)
			{
				if (!papuga_SerializationIter_skip_structure( &iter)) throw ErrorException( papuga_SyntaxError);
				OutputContextClass::appendUnspecifiedStructure();
			}
			else
			{
				int ecnt = 0;
				while (papuga_SerializationIter_tag( &iter) == papuga_TagName)
				{
					if (ecnt++) OutputContextClass::appendSeparator();
					const papuga_ValueVariant* nameval = papuga_SerializationIter_value( &iter);
					valueIsLink = isEqualAscii( *nameval, PAPUGA_HTML_LINK_ELEMENT);
					OutputContextClass::defOpen();
					OutputContextClass::defName( *nameval);
					papuga_SerializationIter_skip( &iter);
					appendSerializationIterElement( iter, valueIsLink);
				}
			}
			OutputContextClass::closeStruct();
		}
		else if (papuga_SerializationIter_tag( &iter) != papuga_TagClose)
		{
			OutputContextClass::openArray();
			if (depth >= maxDepth)
			{
				if (!papuga_SerializationIter_skip_structure( &iter)) throw ErrorException( papuga_SyntaxError);
				OutputContextClass::appendUnspecifiedStructure();
			}
			else
			{
				int ecnt = 0;
				while (papuga_SerializationIter_tag( &iter) != papuga_TagClose)
				{
					if (ecnt++) OutputContextClass::appendSeparator();
					appendSerializationIterElement( iter, valueIsLink);
				}
			}
			OutputContextClass::closeArray();
		}
	}

	void appendCallResult( const papuga_CallResult& result, bool valueIsLink)
	{
		if (result.nofvalues > 1)
		{
			OutputContextClass::openArray();
			int ri = 0, re = result.nofvalues;
			for (; ri != re; ++ri)
			{
				if (ri) OutputContextClass::appendSeparator();
				defValue( result.valuear[ri], valueIsLink);
			}
			OutputContextClass::closeArray();
		}
		else if (result.nofvalues == 1)
		{
			defValue( result.valuear[0], valueIsLink);
		}
		else
		{
			
			OutputContextClass::appendNull();
		}
	}

	void appendIterator( const papuga_Iterator& iterator, bool valueIsLink)
	{
		int itercnt = 0;
		papuga_Allocator allocator;
		papuga_CallResult result;
		int result_mem[ 4096];
		char error_mem[ 128];
		bool rt = true;

		try
		{
			papuga_init_Allocator( &allocator, result_mem, sizeof(result_mem));
			papuga_init_CallResult( &result, &allocator, false/*allocator ownership*/, error_mem, sizeof(error_mem));
	
			OutputContextClass::openArray();
			for (; itercnt < PAPUGA_MAX_ITERATOR_EXPANSION_LENGTH && rt && iterator.getNext( iterator.data, &result); ++itercnt)
			{
				if (itercnt) OutputContextClass::appendSeparator();
				appendCallResult( result, valueIsLink);
	
				papuga_destroy_CallResult( &result);
				papuga_destroy_Allocator( &allocator);
				papuga_init_Allocator( &allocator, result_mem, sizeof(result_mem));
				papuga_init_CallResult( &result, &allocator, false/*allocator ownership*/, error_mem, sizeof(error_mem));
			}
			if (papuga_CallResult_hasError( &result))
			{
				throw ErrorException( papuga_IteratorFailed);
			}
			OutputContextClass::closeArray();
			papuga_destroy_Allocator( &allocator);
		}
		catch (const ErrorException& err)
		{
			papuga_destroy_Allocator( &allocator);
			throw err;
		}
		catch (const std::bad_alloc& err)
		{
			papuga_destroy_Allocator( &allocator);
			throw err;
		}
		catch (const std::runtime_error& err)
		{
			papuga_destroy_Allocator( &allocator);
			throw err;
		}
		catch (...)
		{
			papuga_destroy_Allocator( &allocator);
			throw ErrorException( papuga_IteratorFailed);
		}
	}

	static std::string tostring( const char* root, const char* elem, const papuga_ValueVariant& val, const papuga_StructInterfaceDescription* structs, int maxDepth)
	{
		OutputContextClass ctx( structs, maxDepth);
		ctx.defHead( root);
		if (elem)
		{
			ctx.defOpen();
			ctx.defName( elem);
			ctx.defValue( val, ctx.isEqual( elem, PAPUGA_HTML_LINK_ELEMENT));
			ctx.defClose();
		}
		else
		{
			ctx.defValue( val, ctx.isEqual( elem, PAPUGA_HTML_LINK_ELEMENT));
		}
		ctx.defTail();
		ctx.done();
		return ctx.out;
	}
};

}}//namespace
#endif

