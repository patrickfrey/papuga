/*
 * Copyright (c) 2019 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
#ifndef _PAPUGA_REQUEST_VALUE_VARIANT_MARKUP_HTML_HPP_INCLUDED
#define _PAPUGA_REQUEST_VALUE_VARIANT_MARKUP_HTML_HPP_INCLUDED
/// \brief Print value variant contents as HTML
/// \file valueVariant_markup_html.hpp
#include "valueVariant_markup_base.hpp"

namespace papuga {
namespace markup {

class OutputContextHTML
	:public TagDeclOutputContext<OutputContextHTML>
{
public:
	OutputContextHTML( const papuga_StructInterfaceDescription* structs_, int maxDepth_, papuga_StringEncoding enc_, bool beautified_, const char* head_, const char* href_base_)
		:TagDeclOutputContext<OutputContextHTML>(structs_,maxDepth_,enc_),indent(),beautified(beautified_),head(head_),href_base(href_base_)
	{
		if (beautified) indent.push_back( '\n');
	}

	void reset()
	{
		TagDeclOutputContext<OutputContextHTML>::reset();
		indent.clear();
		if (beautified) indent.push_back( '\n');
	}

	void defHead( const char* name)
	{
		char hdrbuf[ 1024];
		if (href_base)
		{
			std::snprintf( hdrbuf, sizeof(hdrbuf), "<!DOCTYPE html>\n<html>\n<head>\n<meta charset=\"%s\"></meta>\n<base href=\"%s\"></base>\n", papuga_StringEncoding_name( encoding), href_base);
		}
		else
		{
			std::snprintf( hdrbuf, sizeof(hdrbuf), "<!DOCTYPE html>\n<html>\n<head>\n<meta charset=\"%s\"></meta>\n", papuga_StringEncoding_name( encoding));
		}
		out.append( hdrbuf);
		if (head)
		{
			out.append( head);
		}
		out.append( "</head>\n<body>\n");
		out.append( "<div class=\"title\">");
		appendTagName( name);
		out.append( "</div>");
	}

	void defTail( const char* name)
	{
		out.append( "\n</body>\n</html>\n");
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
			appendAtomicValue_withEncoder( value, &OutputContextBase::appendEncoded_Xml);
		}
		else
		{
			appendAtomicValue_withEncoder( value, &OutputContextBase::appendEncoded_Rfc3986);
		}
	}

	void appendLinkDeclaration( const papuga_ValueVariant& value)
	{
		if (beautified) out.append( indent);
		out.append( "<div class=\"link\"><a href=\"");
		appendLinkId( value);
		out.append( "\"><span class=\"value\">");
		appendAtomicValue_withEncoder( value, &OutputContextBase::appendEncoded_Xml);
		out.append( "</span></a></div>");
	}

	void appendAtomicValueDeclaration( const char* name, const papuga_ValueVariant& value)
	{
		if (beautified) out.append( indent);
		out.append( "<span class=\"name\">"); appendTagName( name); out.append( "</span>");
		out.append( "<span class=\"value\">"); appendAtomicValueEncoded( value); out.append( "</span>");
	}

	void appendNullValueDeclaration( const char* name, const papuga_ValueVariant& value)
	{
		if (beautified) out.append( indent);
		out.append( "<span class=\"null\"></span>");
	}

	void appendUnspecifiedStructure()
	{
		if (beautified) out.append( indent);
		out.append( "<div class=\"folded\"></div>");
	}

	void openTag( const papuga_ValueVariant& name)
	{
		defOpen(); out.append( "<div class=\""); appendTagName( name); out.append( "\">");
		out.append( "<span class=\"title\">"); appendTagName( name); out.append( "</span>"); 
	}

	void openTag( const char* name)
	{
		defOpen(); out.append( "<div class=\""); appendTagName( name); out.append( "\">");
		out.append( "<span class=\"title\">"); appendTagName( name); out.append( "</span>"); 
	}

	void closeTag( const papuga_ValueVariant& name)
	{
		defClose(); out.append( "</div>"); 
	}

	void closeTag( const char* name)
	{
		defClose(); out.append( "</div>"); 
	}

	void openCloseTagImm( const papuga_ValueVariant& name)
	{
		out.append( "<div class=\""); appendTagName( name); out.append( "\"></div>");
	}

	void openCloseTagImm( const char* name)
	{
		out.append( "<div class=\""); appendTagName( name); out.append( "\"></div>");
	}

	void appendAttribute( const papuga_ValueVariant& name, const papuga_ValueVariant& value)
	{
		out.append( "<div class=\"attribute\">"); 
		out.append( "<span class=\"name\">"); appendTagName( name); out.append( "</span>");
		out.append( "<span class=\"value\">"); appendAtomicValueEncoded( value); out.append( "</span>");
		out.append( "</div>");
	}

	void appendAttribute( const char* name, const papuga_ValueVariant& value)
	{
		out.append( "<div class=\"attribute\">"); 
		out.append( "<span class=\"name\">"); appendTagName( name); out.append( "</span>");
		out.append( "<span class=\"value\">"); appendAtomicValueEncoded( value); out.append( "</span>");
		out.append( "</div>");
	}

protected:
	std::string indent;
	bool beautified;
	const char* head;
	const char* href_base;
};

}}//namespace
#endif

