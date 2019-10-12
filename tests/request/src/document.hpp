/*
 * Copyright (c) 2017 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
#ifndef _PAPUGA_TEST_DOCUMENT_HPP_INCLUDED
#define _PAPUGA_TEST_DOCUMENT_HPP_INCLUDED
/// \brief Some classes and functions for building test documents in a convenient way
/// \file document.hpp

#include "papuga.h"
#include <string>

namespace papuga {
namespace test {

class DocumentNode
{
public:
	DocumentNode( const std::string& value_)
		:m_name(),m_value(value_),m_attr(0),m_next(0),m_child(0){}
	DocumentNode( const std::string& name_, const std::string& value_)
		:m_name(name_),m_value(value_),m_attr(0),m_next(0),m_child(0){}
	DocumentNode( const std::string& name_, const DocumentNode& content)
		:m_name(name_),m_value(content.m_value),m_attr(0),m_next(0),m_child( content.m_child ? new DocumentNode( *content.m_child) : 0){}
	DocumentNode( const DocumentNode& o)
		:m_name(o.m_name),m_value(o.m_value)
		,m_attr(o.m_attr ? new DocumentNode( *o.m_attr) : 0)
		,m_next(o.m_next ? new DocumentNode( *o.m_next) : 0)
		,m_child(o.m_child ? new DocumentNode( *o.m_child) : 0){}
#if __cplusplus >= 201103L
	DocumentNode( const std::string& name_, const std::initializer_list<std::pair<std::string,std::string> >& attributes, const std::initializer_list<DocumentNode>& content);
	DocumentNode( const std::string& name_, const std::initializer_list<DocumentNode>& content);
#endif
	~DocumentNode();

	void addChild( const DocumentNode& o);

	void addAttribute( const std::string& name_, const std::string& value_);

	std::string toxml( const papuga_StringEncoding& encoding, bool with_indent) const;

	std::string tojson( const papuga_StringEncoding& encoding) const;

	std::string totext() const;

private:
	static inline const char* indentTabJson() {return "\t";}
	static inline const char* indentTabText() {return "\t";}
	static inline const char* indentTabXml() {return "  ";}

	void printNodeText( std::ostream& out, const std::string& indent) const;

	void printRootNodeValueXml( std::ostream& out, const std::string& indent) const;
	void printRootNodeXml( std::ostream& out, const std::string& indent) const;
	void printNodeValueXml( std::ostream& out, const std::string& indent) const;
	void printNodeXml( std::ostream& out, const std::string& indent) const;

	void printNodeValueJson( std::ostream& out, const std::string& indent, int depth, bool tab) const;
	void printNodeListJson( std::ostream& out, const std::string& indent, int cnt, int depth) const;

	static const DocumentNode* getNodeNextDiffName( const DocumentNode* ci);

private:
	std::string m_name;
	std::string m_value;
	DocumentNode* m_attr;
	DocumentNode* m_next;
	DocumentNode* m_child;
};


class Document
{
public:
	Document()
		:m_root(0){}
	Document( const DocumentNode& o)
		:m_root( new DocumentNode(o)){}
	Document( const Document& o)
		:m_root( o.m_root ? new DocumentNode(*o.m_root) : 0){}
	~Document()
	{
		if (m_root) delete m_root;
	}

#if __cplusplus >= 201103L
	Document( const std::string& name_, const std::initializer_list<DocumentNode>& content)
		:m_root( new DocumentNode( name_, {}, content)){}
#endif
	std::string toxml( const papuga_StringEncoding& encoding, bool with_indent) const
	{
		return m_root ? m_root->toxml( encoding, with_indent) : std::string();
	}

	std::string tojson( const papuga_StringEncoding& encoding) const
	{
		return m_root ? m_root->tojson( encoding) : std::string();
	}

	std::string totext() const
	{
		return m_root ? m_root->totext() : std::string();
	}

private:
	DocumentNode* m_root;
};


std::string dumpRequest( papuga_ContentType contentType, papuga_StringEncoding encoding, const std::string& content);


}}//namespace
#endif

