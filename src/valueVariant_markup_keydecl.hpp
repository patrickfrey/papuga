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
#include "papuga/serialization.hpp"
#include <algorithm>

namespace papuga {
namespace markup {

/// \brief Template for output of name value pair declaration markup languages like JSON and TEXT
/// \note Using curiously recurring template pattern to inject the higher level methods that implement the concept of a key value declaration markup
template <class OutputContextClass>
class KeyDeclOutputContext
	:public OutputContextBase
{
public:
	KeyDeclOutputContext( const papuga_StructInterfaceDescription* structs_, int maxDepth_, papuga_StringEncoding enc_)
		:OutputContextBase(structs_,maxDepth_,enc_){}

	void reset()
	{
		OutputContextBase::reset();
	}
	inline OutputContextClass* THIS()
	{
		return (OutputContextClass*)this;
	}

	void defValue( const papuga_ValueVariant& value, bool valueIsLink, bool tabulator)
	{
		if (!papuga_ValueVariant_defined(&value))
		{
			if (tabulator) THIS()->appendTab();
			THIS()->appendNull();
		}
		else if (papuga_ValueVariant_isatomic(&value))
		{
			if (tabulator) THIS()->appendTab();
			if (valueIsLink)
			{
				THIS()->appendLinkIdElem( value);
			}
			else
			{
				THIS()->appendAtomicValueElem( value);
			}
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

	void defValueRoot( const papuga_ValueVariant& value, bool valueIsLink, bool tabulator)
	{
		defValue( value, valueIsLink, tabulator);
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

	void appendSerializationIterElement( papuga_SerializationIter& iter, bool valueIsLink, bool tabulator)
	{
		if (papuga_SerializationIter_tag( &iter) == papuga_TagValue)
		{
			defValue( *papuga_SerializationIter_value( &iter), valueIsLink, tabulator);
			papuga_SerializationIter_skip( &iter);
		}
		else if (papuga_SerializationIter_tag( &iter) == papuga_TagOpen)
		{
			int structid = papuga_SerializationIter_value( &iter)->value.Int;
			papuga_SerializationIter_skip( &iter);
			appendSerializationIter( iter, valueIsLink, structid);
			consumeClose( iter);
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
			THIS()->openStruct();
			if (depth >= maxDepth)
			{
				if (!papuga_SerializationIter_skip_structure( &iter)) throw ErrorException( papuga_SyntaxError);
				THIS()->appendUnspecifiedStructure();
			}
			else
			{
				int ecnt = 0;
				for (;papuga_SerializationIter_tag(&iter) != papuga_TagClose; ++ecnt)
				{
					if (ecnt) THIS()->appendSeparator();
					const char* name = structs[ structid-1].members[ ecnt].name;
					if (!name) throw ErrorException( papuga_SyntaxError);
					valueIsLink = isEqual( name, PAPUGA_HTML_LINK_ELEMENT);
					THIS()->defOpen();
					THIS()->defName( name);
					appendSerializationIterElement( iter, valueIsLink, true/*tabulator*/);
					THIS()->defClose();
				}
			}
			THIS()->closeStruct();
		}
		else if (papuga_SerializationIter_tag( &iter) == papuga_TagName)
		{
			THIS()->openStruct();
			if (depth >= maxDepth)
			{
				if (!papuga_SerializationIter_skip_structure( &iter)) throw ErrorException( papuga_SyntaxError);
				THIS()->appendUnspecifiedStructure();
			}
			else
			{
				int ecnt = 0;
				for (;papuga_SerializationIter_tag( &iter) == papuga_TagName; ++ecnt)
				{
					if (ecnt) THIS()->appendSeparator();
					const papuga_ValueVariant* nameval = papuga_SerializationIter_value( &iter);
					valueIsLink = isEqualAscii( *nameval, PAPUGA_HTML_LINK_ELEMENT);
					THIS()->defOpen();
					THIS()->defName( *nameval);
					papuga_SerializationIter_skip( &iter);
					appendSerializationIterElement( iter, valueIsLink, true/*tabulator*/);
					THIS()->defClose();
				}
			}
			THIS()->closeStruct();
		}
		else if (papuga_SerializationIter_tag( &iter) == papuga_TagClose)
		{
			THIS()->openCloseStructImm();
		}
		else
		{
			THIS()->openArray();
			if (depth >= maxDepth)
			{
				if (!papuga_SerializationIter_skip_structure( &iter)) throw ErrorException( papuga_SyntaxError);
				THIS()->appendUnspecifiedStructure();
			}
			else
			{
				int ecnt = 0;
				for (; papuga_SerializationIter_tag( &iter) != papuga_TagClose; ++ecnt)
				{
					if (ecnt) THIS()->appendSeparator();
					THIS()->defOpen();
					appendSerializationIterElement( iter, valueIsLink, false/*tabulator*/);
					THIS()->defClose();
				}
			}
			THIS()->closeArray();
		}
	}

	void appendCallResult( const papuga_CallResult& result, bool valueIsLink)
	{
		if (result.nofvalues > 1)
		{
			THIS()->openArray();
			int ri = 0, re = result.nofvalues;
			for (; ri != re; ++ri)
			{
				if (ri) THIS()->appendSeparator();
				THIS()->defOpen();
				defValue( result.valuear[ri], valueIsLink, false/*tabulator*/);
				THIS()->defClose();
			}
			THIS()->closeArray();
		}
		else if (result.nofvalues == 1)
		{
			defValue( result.valuear[0], valueIsLink, false/*tabulator*/);
		}
		else
		{
			
			THIS()->appendNull();
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
	
			THIS()->openArray();
			for (; itercnt < PAPUGA_MAX_ITERATOR_EXPANSION_LENGTH && rt && iterator.getNext( iterator.data, &result); ++itercnt)
			{
				if (itercnt) THIS()->appendSeparator();
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
			THIS()->closeArray();
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

	std::string build( const char* root, const char* elem, const papuga_ValueVariant& val)
	{
		std::string rt;
		THIS()->reset();
		if (root)
		{
			THIS()->defHead( encoding, root);
			if (elem)
			{
				THIS()->openFirstElem( elem);
				THIS()->defValueRoot( val, isEqual( elem, PAPUGA_HTML_LINK_ELEMENT), THIS()->firstElemTabulator());
				THIS()->closeFirstElem();
			}
			else
			{
				THIS()->defValueRoot( val, isEqual( root, PAPUGA_HTML_LINK_ELEMENT), false/*tabulator*/);
			}
			THIS()->defTail();
		}
		else
		{
			THIS()->defValueRoot( val, false/*is link element*/, false/*tabulator*/);
		}
		THIS()->defDone();
		std::swap( out, rt);
		return rt;
	}
};

}}//namespace
#endif

