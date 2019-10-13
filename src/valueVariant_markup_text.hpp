/*
 * Copyright (c) 2019 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
#ifndef _PAPUGA_REQUEST_VALUE_VARIANT_MARKUP_TEXT_HPP_INCLUDED
#define _PAPUGA_REQUEST_VALUE_VARIANT_MARKUP_TEXT_HPP_INCLUDED
/// \brief Print value variant contents as plain text
/// \file valueVariant_markup_text.hpp
#include "valueVariant_markup_base.hpp"
#include "valueVariant_markup_keydecl.hpp"

namespace papuga {
namespace markup {

class OutputContextTEXT
	:public KeyDeclOutputContext<OutputContextTEXT>
{
public:
	OutputContextTEXT( const papuga_StructInterfaceDescription* structs_, int maxDepth_, papuga_StringEncoding enc_, bool beautified_)
		:KeyDeclOutputContext<OutputContextTEXT>(structs_,maxDepth_,enc_),indent(),beautified(beautified_)
	{
		indent.push_back( '\n');
	}

	void reset()
	{
		KeyDeclOutputContext<OutputContextTEXT>::reset();
		indent.clear();
		indent.push_back( '\n');
	}

	void defHead( papuga_StringEncoding enc, const char* name)
	{
		// ... no root element printed for text
	}

	void openFirstElem( const char* name)
	{
		// ... no tag for first element after header printed for text
	}
	void closeFirstElem()
	{}
	bool firstElemTabulator()
	{
		return false;
	}

	void defTail()
	{
		out.push_back( '\n');
	}

	void defOpen()
	{
		if (!out.empty()) out.append( indent);
		if (beautified) indent.append( "  ");
		++depth;
	}

	void defClose()
	{
		if (depth <= 0) throw ErrorException( papuga_SyntaxError);
		if (beautified) indent.resize( indent.size()-2);
		--depth;
	}

	void defDone()
	{
		if (depth) throw ErrorException( papuga_SyntaxError);
	}

	void defName( const papuga_ValueVariant& name)
	{
		appendAtomicValueEncoded( name);
		out.append( ":");
	}

	void defName( const char* name)
	{
		appendStringEncoded( name, std::strlen(name));
		out.append( ":");
	}

	void openArray()
	{
	}

	void closeArray()
	{
	}

	void openStruct()
	{
	}

	void closeStruct()
	{
	}

	void openCloseStructImm()
	{
	}

	void appendSeparator()
	{
	}

	void appendTab()
	{
		out.push_back( ' ');
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
		appendLinkId( value);
	}

	void appendAtomicValueElem( const papuga_ValueVariant& value)
	{
		appendAtomicValueEncoded( value);
	}

	void appendNull()
	{
		out.push_back( '?');
	}

	void appendUnspecifiedStructure()
	{
		out.append( "...");
	}

protected:
	std::string indent;
	bool beautified;
};

}}//namespace
#endif

