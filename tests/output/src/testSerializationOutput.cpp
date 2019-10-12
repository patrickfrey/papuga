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
#include <cerrno>
#include <vector>
#include <string>

static const char* HTML_HEAD()
{
	return "<style>div,span {\n\tfont-family: verdana,arial,courier;\n}"
		"\ndiv {\n\tpadding:5px 15px;\n\tposition:relative;\n}"
		"\ndiv.title {\n\tcolor: #09092a; font-size: larger; text-transform: capitalize;\n}"
		"\n.info {\n\tcolor: green;\n}"
		"\n.error {\n\tcolor: #e60000;\n}"
		"\n.table {\n\tdisplay: table;\n}"
		"\n.row {\n\tdisplay: table-row;\n}"
		"\n.col {\n\tdisplay: table-cell;\n}"
		"\n.list {\n\tdisplay: list;\n}"
		"\n.title {\n\tdisplay: block; text-transform: capitalize;\n}"
		"\n.elem {\n\tdisplay: list-item;\n\tlist-style: none;\n}"
		"\nspan.title {\n\tcolor: #222299;\ntext-decoration: underline; text-transform: capitalize;\n}"
		"\nspan.title:after {\n\tcontent: '\\A'\n}"
		"\nspan.name {\n\tpadding:5px 15px; display: inline;\n\tcolor: #669999; text-transform: capitalize;\n}"
		"\nspan.name:after {\n\tcontent: \": \";\n}"
		"\nspan.value {\n\tdisplay: inline;\n}"
		"\nspan.value:after {\n\tcontent: '\\A';\n\twhite-space: pre\n}"
		"\n</style>\n";
}

enum DocType
{
	JSON,
	XML,
	TEXT,
	HTML
};

static const char* docTypeName( DocType d)
{
	static const char* ar[] = {"json","xml","text","html",0};
	return ar[ d];
}

static std::string getTestFileName( const char* inputFileName, const char* fileType_, const char* docTypeName_, const char* outputdir=0)
{
	char const* ie = std::strchr( inputFileName, '\0');
	while (ie != inputFileName && *(ie-1) != '.') {--ie;}
	std::string rt;
	if (outputdir)
	{
		char const* id = ie;
		while (id != inputFileName && *(id-1) != '/') {--id;}
		rt.append( outputdir);
		while (!rt.empty() && rt[ rt.size()-1] == '/') rt.resize( rt.size()-1);
		rt.push_back( '/');
		rt.append( id, ie-id);
	}
	else
	{
		rt.append( inputFileName, ie-inputFileName);
	}
	char ext[ 32];
	std::snprintf( ext, sizeof(ext), "%s.%s", fileType_, docTypeName_);
	rt.append( ext);
	return rt;
}

static DocType docTypeFromName( const char* dt)
{
	if (0==std::strcmp( dt, "json")) return JSON;
	if (0==std::strcmp( dt, "xml")) return XML;
	if (0==std::strcmp( dt, "text")) return TEXT;
	if (0==std::strcmp( dt, "html")) return HTML;
	throw std::runtime_error( "unknown document type");
}

static std::string getTestHrefBase( const char* inputFileName)
{
	std::string rt;
	char const* ie = std::strchr( inputFileName, '\0');
	while (ie != inputFileName && *(ie-1) != '/') {--ie;}
	while (ie != inputFileName && *(ie-1) == '/') {--ie;}
	char const* ii = inputFileName;
	while (ii != ie && *ii == '/') ++ii;
	return std::string("file:///") + std::string( ii, ie-ii);
}

static std::string readFile( const char* filename)
{
	int c;
	std::string rt;
	FILE* fp = filename ? ::fopen( filename, "rb") : stdin;
	if (!fp) throw papuga::runtime_error( "failed to open file '%s' for reading: %s", filename, ::strerror( errno));
	while((c=fgetc(fp))!=EOF) rt.push_back( c);
	bool success = feof(fp);
	if (fp) ::fclose( fp);
	if (!success) throw papuga::runtime_error( "failed to read from file '%s': %s", filename, ::strerror( errno));
	return rt;
}

static void writeFile( const char* filename, const std::string& content)
{
	FILE* fp = filename ? ::fopen( filename, "wb") : stdout;
	if (!fp) throw papuga::runtime_error( "failed to open file '%s' for writing: %s", filename, ::strerror( errno));
	std::string::const_iterator ci = content.begin(), ce = content.end();
	for (; ci != ce && EOF!=fputc( *ci, fp); ++ci){}
	bool success = ci==ce;
	if (fp) ::fclose( fp);
	if (!success) throw papuga::runtime_error( "failed to write to file '%s': %s", filename, ::strerror( errno));
}

static void initValue_serialization_range( papuga_ValueVariant& dest, papuga_Allocator* allocator, const papuga_SerializationIter& start, const papuga_SerializationIter& end, bool withLast)
{
	papuga_Serialization* ser = papuga_Allocator_alloc_Serialization( allocator);
	if (!ser) throw std::bad_alloc();

	papuga_SerializationIter iter;
	papuga_init_SerializationIter_copy( &iter, &start);
	if (withLast)
	{
		while (!papuga_SerializationIter_isequal( &iter, &end))
		{
			if (!papuga_Serialization_push_node( ser, papuga_SerializationIter_node( &iter))) throw std::bad_alloc();
			papuga_SerializationIter_skip( &iter);
		}
	}
	else
	{
		if (papuga_SerializationIter_isequal( &iter, &end)) throw std::runtime_error("no last element");
		const papuga_Node* nd = papuga_SerializationIter_node( &iter);
		papuga_SerializationIter_skip( &iter);
		while (!papuga_SerializationIter_isequal( &iter, &end))
		{
			if (!papuga_Serialization_push_node( ser, nd)) throw std::bad_alloc();
			nd = papuga_SerializationIter_node( &iter);
			papuga_SerializationIter_skip( &iter);
		}
	}
	papuga_init_ValueVariant_serialization( &dest, ser);
}

static char parseChar( char const*& si)
{
	if (si[0] == '\r' && si[1] == '\n') {si+=2; return '\n';}
	else if (si[0] == '\r' && si[1] != '\n') {si+=1; return '\n';}
	else if (si[0] == '\n') {si+=1; return '\n';}
	else {char ch = *si; if (ch) {++si;} return ch;}
}

static bool checkExpected( const std::string& output, const std::string& expected)
{
	char const* oi = output.c_str();
	char const* ei = expected.c_str();
	while (*oi && *ei)
	{
		if (parseChar( oi) != parseChar( ei)) break;
	}
	return (!*oi && !*ei);
}

class Test
{
public:
	Test( const char* inputfilename_, const char* doctypestr, const char* outputdir, int verbosity)
		:m_doctype( docTypeFromName( doctypestr))
		,m_inputfilename(inputfilename_)
		,m_outputfilename(getTestFileName( inputfilename_, "out", doctypestr, outputdir))
		,m_expectfilename(getTestFileName( inputfilename_, "exp", doctypestr))
		,m_root(),m_elem(),m_href_base(getTestHrefBase(inputfilename_)),m_verbosity(verbosity)

	{
		papuga_init_Allocator( &m_allocator, m_allocator_mem, sizeof(m_allocator_mem));
		papuga_init_ValueVariant( &m_content);
	}

	~Test()
	{
		papuga_destroy_Allocator( &m_allocator);
	}

	void run()
	{
		parseInput();
		if (m_verbosity >= 1) printTestDescription();
		if (m_verbosity >= 2) printTestDump( "content", m_content);

		const char* head = HTML_HEAD();
		char* resptr = 0;
		size_t reslen = 0;
		bool beautyfied = true;
		papuga_ErrorCode errcode = papuga_Ok;
		papuga_StructInterfaceDescription structdefs = {NULL,NULL,NULL};
		//... structures not needed here as we have the serialization in plain without external definitions
		switch (m_doctype)
		{
			case JSON:
				resptr = (char*)papuga_ValueVariant_tojson(
						&m_content, &m_allocator, &structdefs, papuga_UTF8, beautyfied, 
						m_root.c_str(), m_elem.empty() ? 0 : m_elem.c_str(),
						&reslen, &errcode);
				
				break;
			case XML:
				resptr = (char*)papuga_ValueVariant_toxml(
						&m_content, &m_allocator, &structdefs, papuga_UTF8, beautyfied,
						m_root.c_str(), m_elem.empty() ? 0 : m_elem.c_str(),
						&reslen, &errcode);
				break;
			case TEXT:
				resptr = (char*)papuga_ValueVariant_totext(
						&m_content, &m_allocator, &structdefs, papuga_UTF8, beautyfied, 
						m_root.c_str(), m_elem.empty() ? 0 : m_elem.c_str(),
						&reslen, &errcode);
				break;
			case HTML:
				resptr = (char*)papuga_ValueVariant_tohtml5(
						&m_content, &m_allocator, &structdefs, papuga_UTF8, beautyfied,
						m_root.c_str(), m_elem.empty() ? 0 : m_elem.c_str(),
						head, m_href_base.c_str(), &reslen,
						&errcode);
				break;

		}
		if (!resptr) throw std::runtime_error( papuga_ErrorCode_tostring( errcode));
		std::string output( resptr, reslen);
		writeFile( m_outputfilename.c_str(), output);
		std::string expected = readFile( m_expectfilename.c_str());

		if (!checkExpected( output, expected))
		{
			std::cerr << "comparing output " << m_outputfilename << " with expected " << m_expectfilename << " failed" << std::endl;
			throw std::runtime_error( "output does not match expected");
		}
	}

private:
	void printTestDescription()
	{
		std::cerr << "Test " << m_inputfilename << " for " << docTypeName( m_doctype) << ":" << std::endl;
		std::cerr << "\tOutput file: " << m_outputfilename << std::endl;
		std::cerr << "\tExpect file: " << m_expectfilename << std::endl;
		std::cerr << "\tRoot: " << m_root << std::endl;
		std::cerr << "\tElem: " << m_elem << std::endl;
		if (m_doctype == HTML) std::cerr << "\tHref: " << m_href_base << std::endl;
	}

	void printTestDump( const char* title, const papuga_ValueVariant& dump)
	{
		std::size_t len;
		std::string dumpstr = papuga_ValueVariant_todump( &dump, &m_allocator, 0/*structdefs*/, &len);
		std::cerr << "\tDump " << title << ": " << dumpstr << std::endl << std::endl;
	}

	bool checkEnd( const papuga_SerializationIter& end, int nofClose)
	{
		papuga_SerializationIter iter;
		papuga_init_SerializationIter_copy( &iter, &end);
		while (nofClose > 0 && !papuga_SerializationIter_eof( &iter) && papuga_SerializationIter_tag( &iter) == papuga_TagClose)
		{
			papuga_SerializationIter_skip( &iter);
		}
		return nofClose == 0 && papuga_SerializationIter_eof( &iter);
	}

	void parseInput()
	{
		papuga_ValueVariant dump;
		papuga_ErrorCode errcode;
		std::string input = readFile( m_inputfilename.c_str());
		if (!papuga_init_ValueVariant_json( &dump, &m_allocator, papuga_UTF8, input.c_str(), input.size(), &errcode))
		{
			throw std::runtime_error( "failed to parse input");
		}
		if (m_verbosity >= 2) printTestDump( "input", dump);
		if (dump.valuetype != papuga_TypeSerialization)
		{
			throw std::runtime_error( "bad input");
		}
		papuga_SerializationIter iter;
		papuga_init_SerializationIter( &iter, dump.value.serialization);
		if (papuga_SerializationIter_tag( &iter) != papuga_TagName)
		{
			throw std::runtime_error( "bad structure");
		}
		m_root = papuga::ValueVariant_tostring( *papuga_SerializationIter_value( &iter), errcode);
		papuga_SerializationIter_skip( &iter);
		if (m_root.empty()) throw std::runtime_error( "bad root tag");

		if (papuga_SerializationIter_tag( &iter) == papuga_TagValue)
		{
			papuga_init_ValueVariant_value( &m_content, papuga_SerializationIter_value( &iter));
			papuga_SerializationIter_skip( &iter);
			if (!checkEnd( iter, 0)) throw std::runtime_error( "more than one root element");
			return;
		}
		else if (papuga_SerializationIter_tag( &iter) == papuga_TagOpen)
		{
			papuga_SerializationIter root_start, root_end;
			papuga_init_SerializationIter_copy( &root_start, &iter);
			papuga_init_SerializationIter_copy( &root_end, &iter);
			if (!papuga_SerializationIter_skip_structure( &root_end)) throw std::runtime_error( "bad inner structure");
			papuga_SerializationIter_skip( &root_start);

			papuga_SerializationIter_skip( &iter);
			if (papuga_SerializationIter_tag( &iter) == papuga_TagName)
			{
				m_elem = papuga::ValueVariant_tostring( *papuga_SerializationIter_value( &iter), errcode);
				papuga_SerializationIter_skip( &iter);
				if (m_elem.empty()) throw std::runtime_error( "bad element tag");

				if (papuga_SerializationIter_tag( &iter) == papuga_TagValue)
				{
					const papuga_ValueVariant* elem_val = papuga_SerializationIter_value( &iter);
					papuga_SerializationIter_skip( &iter);
					if (checkEnd( iter, 1))
					{
						papuga_init_ValueVariant_value( &m_content, elem_val);
						return;
					}
				}
				else if (papuga_SerializationIter_tag( &iter) == papuga_TagOpen)
				{
					papuga_SerializationIter elem_start, elem_end;
					papuga_init_SerializationIter_copy( &elem_start, &iter);
					papuga_init_SerializationIter_copy( &elem_end, &iter);
					if (!papuga_SerializationIter_skip_structure( &elem_end)) throw std::runtime_error( "bad inner structure");
					papuga_SerializationIter_skip( &elem_start);

					if (checkEnd( elem_end, 1))
					{
						initValue_serialization_range( m_content, &m_allocator, elem_start, elem_end, false);
						return;
					}
				}
			}
			m_elem.clear();
			// ... Fallback
			if (!checkEnd( root_end, 0)) throw std::runtime_error( "more than one root element");
			initValue_serialization_range( m_content, &m_allocator, root_start, root_end, false);
		}
		else
		{
			throw std::runtime_error( "bad structure for test");
		}
	}

private:
	DocType m_doctype;
	std::string m_inputfilename;
	std::string m_outputfilename;
	std::string m_expectfilename;
	std::string m_root;
	std::string m_elem;
	std::string m_href_base;
	papuga_ValueVariant m_content;
	papuga_Allocator m_allocator;
	int m_allocator_mem[ 4096];
	int m_verbosity;
};


int main( int argc, const char* argv[])
{
	if (argc <= 1 || std::strcmp( argv[1], "-h") == 0 || std::strcmp( argv[1], "--help") == 0)
	{
		std::cerr << "testSerializationOutput [-V,-VV] <inputfile> <doctype> <outputdir>" << std::endl
			<< "\t<inputfile>  :Path of JSON source file to process" << std::endl
			<< "\t<doctype>    :Document type for output" << std::endl
			<< "\t<outputdir>  :Directory for output" << std::endl
			<< "\tOption -V    :Verbose output" << std::endl
			<< "\tOption -VV   :Verbose output with dumping content" << std::endl;
		return 0;
	}
	try
	{
		int verbosity = 0;
		int argi = 1;
		if (std::strcmp( argv[argi], "-V") == 0)
		{
			++argi;
			if (!verbosity) verbosity = 1;
		}
		if (std::strcmp( argv[argi], "-VV") == 0)
		{
			++argi;
			if (!verbosity) verbosity = 2;
		}

		if (argi + 3 > argc) throw std::runtime_error( "too many arguments");
		if (argi + 3 < argc) throw std::runtime_error( "too few arguments");

		const char* inputfile = argv[argi+0];
		const char* doctype = argv[argi+1];
		const char* outputdir = argv[argi+2];

		Test test( inputfile, doctype, outputdir, verbosity);
		test.run();

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

