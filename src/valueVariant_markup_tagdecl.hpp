/*
 * Copyright (c) 2019 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
#ifndef _PAPUGA_REQUEST_VALUE_VARIANT_MARKUP_TAGDECL_HPP_INCLUDED
#define _PAPUGA_REQUEST_VALUE_VARIANT_MARKUP_TAGDECL_HPP_INCLUDED
#include "valueVariant_markup_base.hpp"

namespace papuga {
namespace markup {

/// \brief Template for output of tagging markup languages like XML and HTML
/// \note Using curiously recurring template pattern to inject the higher level methods that implement the concept of a tagged declaration markup
template <class OutputContextClass>
class TagDeclOutputContext
	:public OutputContextBase
{
public:
	TagDeclOutputContext( const papuga_StructInterfaceDescription* structs_, int maxDepth_, papuga_StringEncoding enc_)
		:OutputContextBase(structs_,maxDepth_,enc_){}

	void defTopValue( const papuga_ValueVariant& value, bool isLink)
	{
		if (isLink)
		{
			((OutputContextClass*)this)->appendLinkId( value);
		}
		else if (!papuga_ValueVariant_isvalid(&value))
		{}
		else if (papuga_ValueVariant_isatomic(&value))
		{
			((OutputContextClass*)this)->appendAtomicValueEncoded( value);
		}
		else if (value.valuetype == papuga_TypeSerialization)
		{
			appendSerialization( *value.value.serialization, 0/*name*/);
		}
		else if (value.valuetype == papuga_TypeIterator)
		{
			throw ErrorException( papuga_SyntaxError);
		}
		else
		{
			throw ErrorException( papuga_TypeError);
		}
	}

	void defValue( const papuga_ValueVariant& value, const char* name)
	{
		if (isEqual( name, PAPUGA_HTML_LINK_ELEMENT))
		{
			((OutputContextClass*)this)->appendLinkDeclaration( value);
		}
		else if (!papuga_ValueVariant_isvalid(&value))
		{
			((OutputContextClass*)this)->appendNullValueDeclaration( name, value);
		}
		else if (papuga_ValueVariant_isatomic(&value))
		{
			((OutputContextClass*)this)->appendAtomicValueDeclaration( name, value);
		}
		else if (value.valuetype == papuga_TypeSerialization)
		{
			appendSerialization( *value.value.serialization, name);
		}
		else if (value.valuetype == papuga_TypeIterator)
		{
			appendIterator( *value.value.iterator, name);
		}
		else
		{
			throw ErrorException( papuga_TypeError);
		}
	}

	void appendSerialization( const papuga_Serialization& ser, const char* name)
	{
		int structid = papuga_Serialization_structid( &ser);
		papuga_SerializationIter iter;
		papuga_init_SerializationIter( &iter, &ser);
		appendSerializationIter( iter, name, structid);
		if (!papuga_SerializationIter_eof(&iter))
		{
			throw ErrorException( papuga_SyntaxError);
		}
	}

	void appendSerializationIterElement( papuga_SerializationIter& iter, const char* name)
	{
		if (papuga_SerializationIter_tag( &iter) == papuga_TagValue)
		{
			defValue( *papuga_SerializationIter_value( &iter), name);
			papuga_SerializationIter_skip( &iter);
		}
		else if (papuga_SerializationIter_tag( &iter) == papuga_TagOpen)
		{
			int structid = papuga_SerializationIter_value( &iter)->value.Int;
			papuga_SerializationIter_skip( &iter);
			appendSerializationIter( iter, name, structid);
			consumeClose( iter);
		}
		else
		{
			throw ErrorException( papuga_SyntaxError);
		}
	}

	void appendSerializationIterElement( papuga_SerializationIter& iter, const papuga_ValueVariant& nameval)
	{
		char namebuf[ 128];
		size_t namelen;
		papuga_Allocator allocator;
		papuga_ErrorCode errcode;
		
		papuga_init_Allocator( &allocator, namebuf, sizeof(namebuf));
		const char* namestr = papuga_ValueVariant_tostring( &nameval, &allocator, &namelen, &errcode);
		if (!namestr)
		{
			papuga_destroy_Allocator( &allocator);
			throw ErrorException( errcode);
		}
		try
		{
			appendSerializationIterElement( iter, namestr);
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
		papuga_destroy_Allocator( &allocator);
	}

	void appendSerializationIter( papuga_SerializationIter& iter, const char* name, int structid)
	{
		if (structid)
		{
			if (name) ((OutputContextClass*)this)->openTag( name);
			if (depth >= maxDepth)
			{
				if (!papuga_SerializationIter_skip_structure( &iter)) throw ErrorException( papuga_SyntaxError);
				((OutputContextClass*)this)->appendUnspecifiedStructure();
			}
			else
			{
				int ecnt = 0;
				for (; papuga_SerializationIter_tag(&iter) != papuga_TagClose; papuga_SerializationIter_skip(&iter))
				{
					const char* membname = structs[ structid-1].members[ ecnt].name;
					if (!membname) throw ErrorException( papuga_SyntaxError);

					if (papuga_SerializationIter_tag(&iter) == papuga_TagValue
					&& !papuga_ValueVariant_isvalid( papuga_SerializationIter_value(&iter)))
					{
						// ... skip NULL value definition
						papuga_SerializationIter_skip(&iter);
					}
					else if (isAttributeName( membname))
					{
						if (papuga_SerializationIter_tag(&iter) == papuga_TagValue
						&&  papuga_ValueVariant_isatomic( papuga_SerializationIter_value(&iter)))
						{
							((OutputContextClass*)this)->appendAttribute( membname, *papuga_SerializationIter_value(&iter));
							papuga_SerializationIter_skip(&iter);
						}
						else
						{
							throw ErrorException( papuga_SyntaxError);
						}
					}
					else
					{
						appendSerializationIterElement( iter, membname);
					}
				}
			}
			if (name) ((OutputContextClass*)this)->closeTag( name);
		}
		else if (papuga_SerializationIter_tag( &iter) == papuga_TagName)
		{
			if (name) ((OutputContextClass*)this)->openTag( name);
			if (depth >= maxDepth)
			{
				if (!papuga_SerializationIter_skip_structure( &iter)) throw ErrorException( papuga_SyntaxError);
				((OutputContextClass*)this)->appendUnspecifiedStructure();
			}
			else
			{
				while (papuga_SerializationIter_tag( &iter) == papuga_TagName)
				{
					const papuga_ValueVariant* nameval = papuga_SerializationIter_value( &iter);
					papuga_SerializationIter_skip( &iter);

					if (papuga_SerializationIter_tag(&iter) == papuga_TagValue
					&& !papuga_ValueVariant_isvalid( papuga_SerializationIter_value(&iter)))
					{
						// ... skip NULL value definition
						papuga_SerializationIter_skip( &iter);
					}
					else
					{
						appendSerializationIterElement( iter, *nameval);
					}
				}
			}
			if (name) ((OutputContextClass*)this)->closeTag( name);
		}
		else if (papuga_SerializationIter_tag( &iter) == papuga_TagClose)
		{
			if (name) ((OutputContextClass*)this)->openCloseTagImm( name);
		}
		else
		{
			if (!name) throw ErrorException( papuga_SyntaxError);
			if (depth >= maxDepth)
			{
				if (!papuga_SerializationIter_skip_structure( &iter)) throw ErrorException( papuga_SyntaxError);
				((OutputContextClass*)this)->appendUnspecifiedStructure();
			}
			else
			{
				while (papuga_SerializationIter_tag( &iter) != papuga_TagClose)
				{
					appendSerializationIterElement( iter, name);
				}
			}
		}
	}

	void appendCallResult( const papuga_CallResult& result, const char* name)
	{
		if (result.nofvalues > 1)
		{
			((OutputContextClass*)this)->openTag( name);
			int ri = 0, re = result.nofvalues;
			for (; ri != re; ++ri)
			{
				char buf[ 32];
				std::snprintf( buf, sizeof(buf), "%d", ri+1);
				defValue( result.valuear[ri], buf);
			}
			((OutputContextClass*)this)->closeTag( name);
		}
		else if (result.nofvalues == 1)
		{
			defValue( result.valuear[0], name);
		}
	}

	void appendIterator( const papuga_Iterator& iterator, const char* name)
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
	
			for (; itercnt < PAPUGA_MAX_ITERATOR_EXPANSION_LENGTH && rt && iterator.getNext( iterator.data, &result); ++itercnt)
			{
				appendCallResult( result, name);

				papuga_destroy_CallResult( &result);
				papuga_destroy_Allocator( &allocator);
				papuga_init_Allocator( &allocator, result_mem, sizeof(result_mem));
				papuga_init_CallResult( &result, &allocator, false/*allocator ownership*/, error_mem, sizeof(error_mem));
			}
			if (papuga_CallResult_hasError( &result))
			{
				throw ErrorException( papuga_IteratorFailed);
			}
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
		if (elem)
		{
			((OutputContextClass*)this)->defHead( root);
			((OutputContextClass*)this)->defValue( val, elem);
			((OutputContextClass*)this)->defTail( root);
			((OutputContextClass*)this)->defDone();
		}
		else
		{
			((OutputContextClass*)this)->defHead( root);
			((OutputContextClass*)this)->defTopValue( val, isEqual( root, PAPUGA_HTML_LINK_ELEMENT));
			((OutputContextClass*)this)->defTail( root);
			((OutputContextClass*)this)->defDone();
		}
		return out;
	}
};

}}//namespace
#endif

