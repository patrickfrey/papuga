/*
 * Copyright (c) 2019 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
#ifndef _PAPUGA_REQUEST_VALUE_VARIANT_MARKUP_JSON_HPP_INCLUDED
#define _PAPUGA_REQUEST_VALUE_VARIANT_MARKUP_JSON_HPP_INCLUDED
/// \brief Print value variant contents as JSON
/// \file valueVariant_markup_json.hpp
#include "valueVariant_markup_base.hpp"
#include "valueVariant_markup_keydecl.hpp"

namespace papuga {
namespace markup {

class OutputContextJSON
	:public KeyDeclOutputContext<OutputContextJSON>
{
public:	
	OutputContextJSON( const papuga_StructInterfaceDescription* structs_, int maxDepth_, papuga_StringEncoding enc_, bool beautyfied_)
		:KeyDeclOutputContext<OutputContextJSON>(structs_,maxDepth_,enc_),indent(),beautyfied(beautyfied_)
	{
		if (beautyfied) indent.push_back( '\n');
	}

	static bool isUnquotedValue( const papuga_ValueVariant& value)
	{
		return (value.valuetype == papuga_TypeInt && value.value.Int >= 0);
	}

	void defHead( papuga_StringEncoding enc, const char* name)
	{
		out.append( "{\n");
		defName( name);
	}

	void defTail()
	{
		out.append( "}\n");
	}

	void defOpen()
	{
		out.append( indent);
		if (beautyfied) indent.push_back( '\t');
		++depth;
	}

	void defClose()
	{
		if (depth <= 0) throw ErrorException( papuga_SyntaxError);
		if (beautyfied) indent.resize( indent.size()-1);
		--depth;
	}

	void defDone()
	{
		if (depth) throw ErrorException( papuga_SyntaxError);
	}

	void defName( const papuga_ValueVariant& name)
	{
		out.push_back( '\"');
		appendAtomicValueEncoded( name);
		out.append( "\":");
	}

	void defName( const char* name)
	{
		out.push_back( '\"');
		appendStringEncoded( name, std::strlen(name));
		out.append( "\":");
	}

	void openArray()
	{
		out.push_back( '[');
	}

	void closeArray()
	{
		out.push_back( ']');
	}

	void openStruct()
	{
		out.push_back( '{');
	}

	void closeStruct()
	{
		out.push_back( '}');
	}

	void openCloseStructImm()
	{
		out.append( "{}");
	}

	void appendTab()
	{
		out.push_back( ' ');
	}

	void appendSeparator()
	{
		out.push_back( ',');
	}

	void appendStringEncoded( const char* str, std::size_t len)
	{
		OutputContextBase::appendEncoded_AnsiC( str, len);
	}

	void appendAtomicValueEncoded( const papuga_ValueVariant& value)
	{
		appendAtomicValue_withEncoder( value, &OutputContextBase::appendEncoded_AnsiC);
	}

	void appendLinkId( const papuga_ValueVariant& value)
	{
		if (hasProtocolPrefix( value))
		{
			appendAtomicValue( value);
		}
		else
		{
			appendAtomicValue_withEncoder( value, &OutputContextBase::appendEncoded_Rfc3986);
		}
	}

	void appendLinkIdElem( const papuga_ValueVariant& value)
	{
		out.push_back( '"');
		appendLinkId( value);
		out.push_back( '"');
	}

	void appendAtomicValueElem( const papuga_ValueVariant& value)
	{
		if (isUnquotedValue( value))
		{
			appendAtomicValue( value);
		}
		else
		{
			out.push_back( '\"');
			appendAtomicValueEncoded( value);
			out.push_back( '\"');
		}
	}

	void appendNull()
	{
		out.append( "null");
	}

	void appendUnspecifiedStructure()
	{
		out.append( "...");
	}

protected:
	std::string indent;
	bool beautyfied;
};

}}//namespace
#endif

