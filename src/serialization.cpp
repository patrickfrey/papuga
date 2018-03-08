/*
 * Copyright (c) 2017 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/// \brief Some functions on serialization using C++ features like STL
/// \file serialization.cpp
#include "papuga/serialization.h"
#include "papuga/allocator.h"
#include "papuga/serialization.hpp"
#include "papuga/valueVariant.h"
#include "papuga/valueVariant.hpp"
#include <map>
#include <stdexcept>
#include <sstream>
#include <iostream>
#include <cstdlib>
#include <cstring>

using namespace papuga;

static const char* stringCopyAsCString( const std::string& str, papuga_Allocator* allocator)
{
	return papuga_Allocator_copy_string( allocator, str.c_str(), str.size());
}

struct PrintStruct;
static bool Serialization_print( PrintStruct& ctx, const papuga_Serialization* serialization);


enum PrintMode {
	LineMode,
	BracketMode
};

struct PrintStruct
{
	PrintMode mode;
	std::ostringstream out;
	std::string indent;
	papuga_ErrorCode errcode;
	int taglevel;
	int maxdepth;
	int itemcnt;

	PrintStruct( PrintMode mode_, const std::string& indent_, int maxdepth_)
		:mode(mode_),out(),indent(indent_),errcode(papuga_Ok),taglevel(0),maxdepth(maxdepth_),itemcnt(0){}

	void printOpen()
	{
		++taglevel;
		if (taglevel <= maxdepth)
		{
			switch (mode)
			{
				case LineMode:
					out << indent << "open" << std::endl;
					indent.append( "  ");
					break;
				case BracketMode:
					if (itemcnt)
					{
						out << ", ";
						itemcnt = 0;
					}
					out << "{";
					break;
			}
			if (taglevel == maxdepth)
			{
				switch (mode)
				{
					case LineMode:
						out << "..." << std::endl;
						break;
					case BracketMode:
						out << "...";
						break;
				}
			}
		}
	}

	bool printClose()
	{
		--taglevel;
		if (taglevel < 0)
		{
			errcode = papuga_TypeError;
			return false;
		}
		if (taglevel > maxdepth) return true;
		switch (mode)
		{
			case LineMode:
				indent.resize( indent.size()-2);
				out << indent << "close" << std::endl;
				break;
			case BracketMode:
				out << "}";
				itemcnt++;
				break;
		}
		return true;
	}

	void printName( const std::string& value)
	{
		if (taglevel > maxdepth) return;
		switch (mode)
		{
			case LineMode:
				out << indent << "name " << value << std::endl;
				break;
			case BracketMode:
				if (itemcnt)
				{
					out << ", ";
					itemcnt = 0;
				}
				out << value << ":";
				break;
		}
	}

	void printNullValue()
	{
		if (taglevel > maxdepth) return;
		switch (mode)
		{
			case LineMode:
				out << indent << "value NULL" << std::endl;
				break;
			case BracketMode:
				if (itemcnt++)
				{
					out << ", ";
				}
				out << "NULL";
				break;
		}
	}

	void printObject( const char* typenam)
	{
		if (taglevel > maxdepth) return;
		switch (mode)
		{
			case LineMode:
				out << indent << "value <" << typenam << ">" << std::endl;
				break;
			case BracketMode:
				if (itemcnt++)
				{
					out << ", ";
				}
				out << "<" << typenam << ">";
				break;
		}
	}

	void printNumeric( const std::string& value)
	{
		if (taglevel > maxdepth) return;
		switch (mode)
		{
			case LineMode:
				out << indent << "value " << value << std::endl;
				break;
			case BracketMode:
				if (itemcnt++)
				{
					out << ", ";
				}
				out << value;
				break;
		}
	}

	void printString( const std::string& value)
	{
		if (taglevel > maxdepth) return;
		switch (mode)
		{
			case LineMode:
				out << indent << "value \"" << value << "\"" << std::endl;
				break;
			case BracketMode:
				if (itemcnt++)
				{
					out << ", ";
				}
				out << "\"" << value << "\"";
				break;
		}
	}

	bool printValue( const papuga_ValueVariant& value)
	{
		if (taglevel > maxdepth) return true;
		if (!papuga_ValueVariant_defined( &value))
		{
			printNullValue();
		}
		else if (!papuga_ValueVariant_isatomic( &value))
		{
			if (value.valuetype == papuga_TypeSerialization)
			{
				printOpen();
				Serialization_print( *this, value.value.serialization);
				printClose();
			}
			else if (value.valuetype == papuga_TypeHostObject)
			{
				printObject( papuga_Type_name( value.valuetype));
			}
			else
			{
				errcode = papuga_TypeError;
				return false;
			}
		}
		else if (papuga_ValueVariant_isstring( &value))
		{
			printString( ValueVariant_tostring( value, errcode));
		}
		else
		{
			printNumeric( ValueVariant_tostring( value, errcode));
		}
		return errcode == papuga_Ok;
	}

	void printSubStruct( const std::multimap<std::string,std::string>& map)
	{
		std::multimap<std::string,std::string>::const_iterator mi = map.begin(), me = map.end();
		for (int midx=0; mi != me; ++mi,++midx)
		{
			if (mode == BracketMode && midx)
			{
				out << ", ";
			}
			if (!mi->first.empty())
			{
				printName( mi->first);
			}
			out << mi->second;
		}
	}
};

static bool Serialization_print( PrintStruct& ctx, papuga_SerializationIter& seriter)
{
	for (; !papuga_SerializationIter_eof(&seriter); papuga_SerializationIter_skip(&seriter))
	{
		switch (papuga_SerializationIter_tag(&seriter))
		{
			case papuga_TagOpen:
				ctx.printOpen();
				break;
			case papuga_TagClose:
				if (ctx.taglevel == 0) return true;
				if (!ctx.printClose()) return false;
				break;
			case papuga_TagName:
				ctx.printName( ValueVariant_tostring( *papuga_SerializationIter_value( &seriter), ctx.errcode));
				break;
			case papuga_TagValue:
				if (!ctx.printValue( *papuga_SerializationIter_value( &seriter))) return false;
				break;
			default:
			{
				ctx.errcode = papuga_TypeError;
				return false;
			}
		}
	}
	if (ctx.taglevel != 0)
	{
		ctx.errcode = papuga_UnexpectedEof;
		return false;
	}
	return true;
}

static bool Serialization_print_det( PrintStruct& ctx, papuga_SerializationIter& seriter)
{
	std::multimap<std::string,std::string> map;
	std::string name;
	for (; !papuga_SerializationIter_eof(&seriter); papuga_SerializationIter_skip(&seriter))
	{
		switch (papuga_SerializationIter_tag(&seriter))
		{
			case papuga_TagOpen:
			{
				PrintStruct subctx( ctx.mode, ctx.indent, ctx.maxdepth - ctx.taglevel);
				subctx.printOpen();
				papuga_SerializationIter_skip(&seriter);
				if (!Serialization_print_det( subctx, seriter))
				{
					ctx.errcode = subctx.errcode;
					return false;
				}
				subctx.printClose();
				if (papuga_SerializationIter_eof(&seriter))
				{
					ctx.errcode = papuga_UnexpectedEof;
					return false;
				}
				map.insert( std::pair<std::string,std::string>( name, subctx.out.str()));
				name.clear();
				break;
			}
			case papuga_TagClose:
				ctx.printSubStruct( map);
				return true;
			case papuga_TagName:
				name = ValueVariant_tostring( *papuga_SerializationIter_value( &seriter), ctx.errcode);
				break;
			case papuga_TagValue:
			{
				PrintStruct subctx( ctx.mode, ctx.indent, ctx.maxdepth - ctx.taglevel);
				if (!subctx.printValue( *papuga_SerializationIter_value( &seriter)))
				{
					ctx.errcode = subctx.errcode;
					return false;
				}
				map.insert( std::pair<std::string,std::string>( name, subctx.out.str()));
				name.clear();
				break;
			}
			default:
			{
				ctx.errcode = papuga_TypeError;
				return false;
			}
		}
	}
	ctx.printSubStruct( map);
	return true;
}

static bool Serialization_print( PrintStruct& ctx, const papuga_Serialization* serialization)
{
	papuga_SerializationIter seriter;
	papuga_init_SerializationIter( &seriter, const_cast<papuga_Serialization*>( serialization));
	if (Serialization_print( ctx, seriter))
	{
		if (!papuga_SerializationIter_eof( &seriter))
		{
			ctx.errcode = papuga_SyntaxError;
			return false;
		}
		return true;
	}
	else
	{
		return false;
	}
}

static bool Serialization_print_det( PrintStruct& ctx, const papuga_Serialization* serialization)
{
	papuga_SerializationIter seriter;
	papuga_init_SerializationIter( &seriter, const_cast<papuga_Serialization*>( serialization));
	if (Serialization_print_det( ctx, seriter))
	{
		if (!papuga_SerializationIter_eof( &seriter))
		{
			ctx.errcode = papuga_SyntaxError;
			return false;
		}
		return true;
	}
	else
	{
		return false;
	}
}

extern "C" const char* papuga_Serialization_tostring( const papuga_Serialization* self, papuga_Allocator* allocator, bool linemode, int maxdepth, papuga_ErrorCode* errcode)
{
	try
	{
		if (!self) return 0;
		PrintStruct ctx( linemode?LineMode : BracketMode, linemode?"\n":"", maxdepth);
		if (!Serialization_print( ctx, self))
		{
			*errcode = ctx.errcode;
			return NULL;
		}
		return stringCopyAsCString( ctx.out.str(), allocator);
	}
	catch (const std::bad_alloc&)
	{
		*errcode = papuga_NoMemError;
		return NULL;
	}
}

std::string papuga::Serialization_tostring( const papuga_Serialization& value, bool linemode, int maxdepth, papuga_ErrorCode& errcode)
{
	try
	{
		PrintStruct ctx( linemode?LineMode : BracketMode, linemode?"\n":"", maxdepth);
		if (!Serialization_print( ctx, &value))
		{
			errcode = ctx.errcode;
			return std::string();
		}
		return ctx.out.str();
	}
	catch (const std::bad_alloc&)
	{
		errcode = papuga_NoMemError;
		return std::string();
	}
}

std::string papuga::Serialization_tostring_deterministic( const papuga_Serialization& value, bool linemode, int maxdepth, papuga_ErrorCode& errcode)
{
	try
	{
		PrintStruct ctx( linemode?LineMode : BracketMode, linemode?"\n":"", maxdepth);
		if (!Serialization_print_det( ctx, &value))
		{
			errcode = ctx.errcode;
			return std::string();
		}
		return ctx.out.str();
	}
	catch (const std::bad_alloc&)
	{
		errcode = papuga_NoMemError;
		return std::string();
	}
}

