/*
 * Copyright (c) 2019 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
#ifndef _PAPUGA_REQUEST_VALUE_VARIANT_MARKUP_XML_HPP_INCLUDED
#define _PAPUGA_REQUEST_VALUE_VARIANT_MARKUP_XML_HPP_INCLUDED
/// \brief Print value variant contents as XML
/// \file valueVariant_markup_xml.hpp
#include "valueVariant_markup_base.hpp"
#include "valueVariant_markup_tagdecl.hpp"

namespace papuga {
namespace markup {

class OutputContextXML
	:public TagDeclOutputContext<OutputContextXML>
{
public:
	OutputContextXML( const papuga_StructInterfaceDescription* structs_, int maxDepth_, papuga_StringEncoding enc_, bool beautified_)
		:TagDeclOutputContext<OutputContextXML>(structs_,maxDepth_,enc_),indent(),beautified(beautified_)
	{
		if (beautified) indent.push_back( '\n');
	}

	void defHead( const char* name)
	{
		char hdrbuf[ 1024];
		std::snprintf( hdrbuf, sizeof(hdrbuf), "<?xml version=\"1.0\" encoding=\"%s\" standalone=\"yes\"?>\n<%s>", papuga_StringEncoding_name( encoding), name);
		out.append( hdrbuf);
	}

	void defTail( const char* name)
	{
		out.append( "</");
		out.append( name);
		out.append( ">\n");
	}

	void defDone()
	{
		if (depth) throw ErrorException( papuga_SyntaxError);
	}

	void defOpen()
	{
		if (beautified)
		{
			out.append( indent);
			indent.append( "  ");
		}
		++depth;
	}

	void defClose()
	{
		if (depth <= 0) throw ErrorException( papuga_SyntaxError);
		if (beautified) indent.resize( indent.size()-2);
		--depth;
	}

	void appendStringEncoded( const char* str, std::size_t len)
	{
		OutputContextBase::appendEncoded_Xml( str, len);
	}

	void appendAtomicValueEncoded( const papuga_ValueVariant& value)
	{
		appendAtomicValue_withEncoder( value, &OutputContextBase::appendEncoded_Xml);
	}

	void appendTagName( const papuga_ValueVariant& name)
	{
		appendAtomicValueEncoded( name);
	}

	void appendTagName( const char* name)
	{
		appendStringEncoded( name, std::strlen(name));
	}

	void appendAttributeName( const papuga_ValueVariant& name)
	{
		appendAtomicValue_withEncoder( name, &OutputContextBase::appendDecoded_AttributeName);
	}

	void appendAttributeName( const char* name)
	{
		appendTagName( name+1);
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

	void appendLinkDeclaration( const papuga_ValueVariant& value)
	{
		openTag( PAPUGA_HTML_LINK_ELEMENT);
		appendLinkId( value);
		closeTag( PAPUGA_HTML_LINK_ELEMENT);
	}

	void appendAtomicValueDeclaration( const char* name, const papuga_ValueVariant& value)
	{
		openTag( name);
		appendAtomicValueEncoded( value);
		closeTag( name);
	}

	void appendNullValueDeclaration( const char* name, const papuga_ValueVariant& value)
	{
		out.push_back( '<');
		appendTagName( name);
		out.append( " xsi:nil=\"true\"/>");
	}

	void appendUnspecifiedStructure()
	{
		out.append( "...");
	}

	void openTag( const papuga_ValueVariant& name)
	{
		defOpen(); out.push_back( '<'); appendTagName( name); out.push_back( '>');
	}

	void openTag( const char* name)
	{
		defOpen(); out.push_back( '<'); appendTagName( name); out.push_back( '>');
	}

	void closeTag( const papuga_ValueVariant& name)
	{
		defClose(); out.append( "</"); appendTagName( name); out.push_back( '>');
	}

	void closeTag( const char* name)
	{
		defClose(); out.append( "</"); appendTagName( name); out.push_back( '>');
	}

	void openCloseTagImm( const papuga_ValueVariant& name)
	{
		out.push_back( '<'); appendTagName( name); out.append( "/>");
	}

	void openCloseTagImm( const char* name)
	{
		out.push_back( '<'); out.append( name); out.append( "/>");
	}

	void reopenTagForAttribute()
	{
		if (out.size() < 2 || out[ out.size()-1] != '>' || out[ out.size()-2] == '/') throw ErrorException( papuga_SyntaxError);
		out.resize( out.size()-1);
		out.push_back( ' ');
	}

	void appendAttribute( const papuga_ValueVariant& name, const papuga_ValueVariant& value)
	{
		reopenTagForAttribute();
		appendAttributeName( name);
		out.append( "=\"");
		appendAtomicValueEncoded( value);
		out.append( "\">");
	}

	void appendAttribute( const char* name, const papuga_ValueVariant& value)
	{
		reopenTagForAttribute();
		appendAttributeName( name);
		out.append( "=\"");
		appendAtomicValueEncoded( value);
		out.append( "\">");
	}

protected:
	std::string indent;
	bool beautified;
};

}}//namespace
#endif

