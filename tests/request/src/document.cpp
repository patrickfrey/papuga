/*
 * Copyright (c) 2017 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/// \brief Some classes and function for building test documents in a convenient way
/// \file document.cpp
#include "document.hpp"
#include "papuga.hpp"
#include <string>
#include <iostream>
#include <sstream>
#include <stdexcept>

using namespace papuga;
using namespace papuga::test;

std::string test::encodeString( const papuga_StringEncoding& encoding, const std::string& str)
{
	if (encoding == papuga_UTF8)
	{
		return str;
	}
	else
	{
		papuga_ValueVariant outvalue;
		papuga_init_ValueVariant_string( &outvalue, str.c_str(), str.size());
		std::size_t bufallocsize = str.size() * 6;
		std::size_t bufsize = 0;
		char* buf = (char*)std::malloc( bufallocsize);
		if (!buf) throw std::bad_alloc();
		papuga_ErrorCode errcode = papuga_Ok;
		const void* encbuf = papuga_ValueVariant_tostring_enc( &outvalue, encoding, buf, bufallocsize, &bufsize, &errcode);
		if (!encbuf)
		{
			std::free( buf);
			throw std::runtime_error( papuga_ErrorCode_tostring( errcode));
		}
		else try
		{
			std::string rt( buf, bufsize * papuga_StringEncoding_unit_size( encoding));
			std::free( buf);
			return rt;
		}
		catch (const std::bad_alloc&)
		{
			std::free( buf);
			throw std::bad_alloc();
		}
	}
}

#if __cplusplus >= 201103L
DocumentNode::DocumentNode( const std::string& name_, const std::initializer_list<std::pair<std::string,std::string> >& attributes, const std::initializer_list<DocumentNode>& content)
try	:m_name(name_),m_value(),m_attr(0),m_next(0),m_child(0)
{
	for (const auto& ai : attributes)
	{
		addAttribute( ai.first, ai.second);
	}
	for (const auto& ci : content)
	{
		addChild( ci);
	}
}
catch (...)
{
	std::cerr << "Initializer failed: " << name_ << std::endl;
}
#endif

DocumentNode::~DocumentNode()
{
	if (m_attr) delete m_attr;
	if (m_next) delete m_next;
	if (m_child) delete m_child;
}

void DocumentNode::addChild( const DocumentNode& o)
{
	if (!o.m_child && !o.m_attr && !o.m_next && o.m_name.empty() && m_value.empty())
	{
		m_value = o.m_value;
	}
	else if (m_child)
	{
		DocumentNode* nd = m_child;
		for (; nd->m_next; nd = nd->m_next){}
		nd->m_next = new DocumentNode( o);
	}
	else
	{
		m_child = new DocumentNode( o);
	}
}

void DocumentNode::addAttribute( const std::string& name_, const std::string& value_)
{
	if (name_.empty()) throw std::runtime_error( "adding attribute without name");
	if (m_attr)
	{
		DocumentNode* nd = m_attr;
		for (; nd->m_next; nd = nd->m_next){}
		nd->m_next = new DocumentNode( name_, value_);
	}
	else
	{
		m_attr = new DocumentNode( name_, value_);
	}
}

std::string DocumentNode::toxml( const papuga_StringEncoding& encoding, bool with_indent) const
{
	std::ostringstream out;

	out << "<?xml version=\"1.0\" encoding=\"" << papuga_StringEncoding_name( encoding) << "\" standalone=\"yes\"?>";
	if (m_next)
	{
		throw std::runtime_error("cannot print document with multiple roots as XML");
	}
	else if (m_name.empty())
	{
		throw std::runtime_error("cannot print document without root node name as XML");
	}
	else if (with_indent)
	{
		printNodeXml( out, "\n");
	}
	else
	{
		out << "\n";
		printNodeXml( out, "");
	}
	out << "\n";
	return encodeString( encoding, out.str());
}

std::string DocumentNode::tojson( const papuga_StringEncoding& encoding) const
{
	std::ostringstream out;
	printNodeListJson( out, "\n");
	out << "\n";
	return encodeString( encoding, out.str());
}

std::string DocumentNode::totext() const
{
	std::ostringstream out;
	printNodeText( out, "\n");
	out << "\n";
	return out.str();
}

void DocumentNode::printNodeText( std::ostream& out, const std::string& indent) const
{
	out << indent << "NAME [" << m_name << "]";
	out << indent << "VALUE [" << m_value << "]";
	if (m_attr)
	{
		out << indent << "ATTR [";
		m_attr->printNodeText( out, indent + indentTab());
		out << "]";
	}
	if (m_child)
	{
		out << indent << "CHILD [";
		m_child->printNodeText( out, indent + indentTab());
		out << "]";
	}
	if (m_next)
	{
		m_next->printNodeText( out, indent);
	}
}

void DocumentNode::printNodeValueXml( std::ostream& out, const std::string& indent) const
{
	out << m_value;
	DocumentNode const* ci = m_child;
	for (; ci; ci = ci->m_next)
	{
		ci->printNodeXml( out, indent.empty() ? indent: (indent + indentTab()));
	}
}

void DocumentNode::printNodeXml( std::ostream& out, const std::string& indent) const
{
	if (!m_name.empty())
	{
		out << indent << "<" << m_name;
		DocumentNode const* ai = m_attr;
		for (; ai; ai = ai->m_next)
		{
			out << " " << ai->m_name << "=\"" << ai->m_value << "\"";
		}
		out << ">";
		printNodeValueXml( out, indent);
		out << "</" << m_name << ">";
	}
	else
	{
		printNodeValueXml( out, indent);
	}
}

void DocumentNode::printNodeValueJson( std::ostream& out, const std::string& indent) const
{
	if (!m_attr && !m_child)
	{
		out << "\"" << m_value << "\"";
	}
	else
	{
		int cnt = 0;
		bool ismap = m_attr || (m_child && !m_child->m_name.empty());
		bool isarray = !ismap;
		out << (isarray ? "[":"{");
		DocumentNode const* ai = m_attr;
		for (; ai; ai = ai->m_next,++cnt)
		{
			if (cnt) out << ",";
			out << indent << indentTab() << "\"-" << ai->m_name << "\":\"" << ai->m_value << "\"";
		}
		DocumentNode const* ci = m_child;
		for (; ci; ci = ci->m_next,++cnt)
		{
			if (cnt) out << ",";
			if (isarray && !ci->m_name.empty()) throw std::runtime_error( "miximg array with dictonary in JSON");
			if (ismap && ci->m_name.empty()) throw std::runtime_error( "miximg array with dictonary in JSON");
			ci->printNodeJson( out, indent + indentTab());
		}
		if (!m_value.empty())
		{
			if (cnt) out << ",";
			out << indent << indentTab() << "\"#text\":\"" << m_value << "\"";
		}
		out << indent << (isarray ? "]":"}");
	}
}

void DocumentNode::printNodeJson( std::ostream& out, const std::string& indent) const
{
	if (!m_name.empty())
	{
		out << indent << "\"" << m_name << "\":";
		printNodeValueJson( out, indent);
	}
	else
	{
		printNodeValueJson( out, indent);
	}
}

void DocumentNode::printNodeListJson( std::ostream& out, const std::string& indent) const
{
	int cnt = 0;
	bool isarray = m_name.empty();
	bool ismap = !isarray;
	out << indent << (isarray ? "[":"{");
	DocumentNode const* ci = this;
	for (; ci; ci = ci->m_next,++cnt)
	{
		if (cnt) out << ",";
		if (isarray && !ci->m_name.empty()) throw std::runtime_error( "miximg array with dictonary in JSON");
		if (ismap && ci->m_name.empty()) throw std::runtime_error( "miximg array with dictonary in JSON");
		ci->printNodeJson( out, indent + indentTab());
	}
	out << indent << (isarray ? "]":"}");
}

std::string test::dumpRequest( papuga_ContentType contentType, papuga_StringEncoding encoding, const std::string& content)
{
	std::ostringstream out;
	papuga_ContentType contentType_guessed = papuga_guess_ContentType( content.c_str(), content.size());
	papuga_StringEncoding encoding_guessed = papuga_guess_StringEncoding( content.c_str(), content.size());
	papuga_RequestParser* parser = 0;
	papuga_ErrorCode errcode = papuga_Ok;
	papuga_ValueVariant elemval;
	papuga_RequestElementType elemtype;

	if (contentType_guessed != contentType) throw std::runtime_error("test document content type differs from guessed value");
	if (encoding_guessed != encoding) throw std::runtime_error("test document character set encoding differs from guessed value");

	switch (contentType)
	{
		case papuga_ContentType_Unknown: throw std::runtime_error("test document content type is unknown");
		case papuga_ContentType_XML: parser = papuga_create_RequestParser_xml( encoding, content.c_str(), content.size(), &errcode); break;
		case papuga_ContentType_JSON: parser = papuga_create_RequestParser_json( encoding, content.c_str(), content.size(), &errcode); break;
	}
	if (!parser) throw papuga::error_exception( errcode, "create request from content string");
	for (
		elemtype = papuga_RequestParser_next( parser, &elemval);
		elemtype != papuga_RequestElementType_None && errcode == papuga_Ok;
		elemtype = papuga_RequestParser_next( parser, &elemval))
	{
		switch (elemtype)
		{
			case papuga_RequestElementType_None: break;
			case papuga_RequestElementType_Open: out << "OPEN " << ValueVariant_tostring( elemval, errcode) << "\n"; break;
			case papuga_RequestElementType_Close: out << "CLOSE" << "\n"; break;
			case papuga_RequestElementType_AttributeName: out << "ATTRIBUTE NAME " << ValueVariant_tostring( elemval, errcode) << "\n"; break;
			case papuga_RequestElementType_AttributeValue: out << "ATTRIBUTE VALUE " << ValueVariant_tostring( elemval, errcode) << "\n"; break;
			case papuga_RequestElementType_Value: out << "CONTENT " << ValueVariant_tostring( elemval, errcode) << "\n"; break;
		}
	}
	if (errcode == papuga_Ok)
	{
		errcode = papuga_RequestParser_last_error( parser);
	}
	if (errcode != papuga_Ok)
	{
		char locbuf[ 1024];
		int errpos = papuga_RequestParser_get_position( parser, locbuf, sizeof(locbuf));
		if (errpos >= 0)
		{
			throw papuga::runtime_error( "error parsing request at position %d [%s]: %s", errpos, locbuf, papuga_ErrorCode_tostring( errcode));
		}
		else
		{
			throw papuga::runtime_error( "error parsing request: %s", papuga_ErrorCode_tostring( errcode));
		}
	}
	out << "END" << std::endl;
	return out.str();
}


