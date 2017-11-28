/*
 * Copyright (c) 2017 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
#include "papuga.hpp"
#include <iostream>
#include <stdexcept>
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <cmath>
#include <vector>
#include <string>
#include <sstream>
#include <iostream>

#undef PAPUGA_LOWLEVEL_DEBUG

static std::string encodeString( const papuga_StringEncoding& encoding, const std::string& str)
{
	if (encoding == papuga_UTF8)
	{
		return str;
	}
	else
	{
		papuga_ValueVariant outvalue;
		papuga_init_ValueVariant_string( &outvalue, str.c_str(), str.size());
		std::size_t bufallocsize = str.size() * 5;
		std::size_t bufsize = 0;
		char* buf = (char*)std::malloc( bufsize);
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
			std::string rt( buf, bufsize);
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

	~DocumentNode()
	{
		if (m_next) delete m_next;
		if (m_child) delete m_child;
	}

	void addChild( const DocumentNode& o)
	{
		if (m_child)
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

	std::string toxml( const papuga_StringEncoding& encoding, bool with_indent) const
	{
		std::ostringstream out;

		out << "<?xml version=\"1.0\" encoding=\"" << encodingName( encoding) << "\" standalone=\"yes\"?>";
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

	std::string tojson( const papuga_StringEncoding& encoding) const
	{
		std::ostringstream out;
		printNodeListJson( out, "");
		out << "\n";
		return encodeString( encoding, out.str());
	}

private:
	static const char* encodingName( const papuga_StringEncoding& encoding)
	{
		switch (encoding)
		{
			case papuga_UTF8: return "UTF-8";
			case papuga_UTF16BE: return "UTF-16BE";
			case papuga_UTF16LE: return "UTF-16LE";
			case papuga_UTF16: return "UTF-16";
			case papuga_UTF32BE: return "UTF-32BE";
			case papuga_UTF32LE: return "UTF-32LE";
			case papuga_UTF32: return "UTF-32";
			case papuga_Binary:
			default:
				throw std::runtime_error("illegal encoding for XML");
		}
	}

	void printNodeValueXml( std::ostream& out, const std::string& indent) const
	{
		out << m_value;
		DocumentNode const* ci = m_child;
		for (; ci; ci = ci->m_next)
		{
			ci->printNodeXml( out, indent.empty() ? indent: (indent+"  "));
		}
	}
	void printNodeXml( std::ostream& out, const std::string& indent) const
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
			out << indent << "</" << m_name << ">";
		}
		else
		{
			printNodeValueXml( out, indent);
		}
	}

	void printNodeValueJson( std::ostream& out, const std::string& indent) const
	{
		if (!m_attr && !m_child)
		{
			out << "\"" << m_value << "\"";
		}
		else
		{
			int cnt = 0;
			bool isarray = (m_attr || (m_child && m_child->m_name.empty()));
			out << (isarray ? "[":"{");
			DocumentNode const* ai = m_attr;
			for (; ai; ai = ai->m_next,++cnt)
			{
				if (cnt) out << ",";
				out << indent << "-" << ai->m_name << "=\"" << ai->m_value << "\"";
			}
			DocumentNode const* ci = m_child;
			for (; ci; ci = ci->m_next,++cnt)
			{
				if (cnt) out << ",";
				if (isarray && !ci->m_name.empty()) throw std::runtime_error( "miximg array with dictonary in JSON");
				ci->printNodeJson( out, indent + "  ");
			}
			out << indent << (isarray ? "]":"}");
		}
	}

	void printNodeJson( std::ostream& out, const std::string& indent) const
	{
		if (!m_name.empty())
		{
			out << indent << m_name << "=";
			printNodeValueJson( out, indent);
		}
		else
		{
			printNodeValueJson( out, indent);
		}
	}

	void printNodeListJson( std::ostream& out, const std::string& indent) const
	{
		int cnt = 0;
		bool isarray = m_name.empty();
		out << indent << (isarray ? "[":"{");
		DocumentNode const* ci = this;
		for (; ci; ci = ci->m_next,++cnt)
		{
			if (cnt) out << ",";
			if (isarray && !ci->m_name.empty()) throw std::runtime_error( "miximg array with dictonary in JSON");
			ci->printNodeJson( out, indent + "  ");
		}
		out << indent << (isarray ? "]":"}");
	}

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
private:
};

int main( int argc, const char* argv[])
{
	if (argc <= 1 || std::strcmp( argv[1], "-h") == 0 || std::strcmp( argv[1], "--help") == 0)
	{
		std::cerr << "testRequest <testno>" << std::endl
				<< "\t<testno>     :Index of test to execute (default all)" << std::endl;
		return 0;
	}
	try
	{
		std::cerr << "OK" << std::endl;
		return 0;
	}
	catch (const std::runtime_error& err)
	{
		std::cerr << "ERROR " << err.what() << std::endl;
		return -1;
	}
	catch (const std::bad_alloc& )
	{
		std::cerr << "ERROR out of memory" << std::endl;
		return -2;
	}
	catch (...)
	{
		std::cerr << "EXCEPTION uncaught" << std::endl;
		return -3;
	}
}

