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
#include <iostream>
#include <cstdlib>
#include <cstring>
#include <inttypes.h>

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
	std::string out;
	std::string indent;
	papuga_ErrorCode errcode;
	int printlevel;
	int itemcnt;
	bool deterministic;

	PrintStruct( PrintMode mode_, const std::string& indent_, int printlevel_, bool deterministic_)
		:mode(mode_),out(),indent(indent_),errcode(papuga_Ok),printlevel(printlevel_),itemcnt(0),deterministic(deterministic_){}

	void appendInt( int64_t value)
	{
		char valuebuf[ 64];
		std::snprintf( valuebuf, sizeof(valuebuf), "%" PRIu64, value);
		out.append( valuebuf);
	}

	void printOpen( int structid)
	{
		--printlevel;
		if (printlevel >= 0)
		{
			switch (mode)
			{
				case LineMode:
					if (structid)
					{
						out.append( indent);
						out.append( "open #");
						appendInt( structid);
					}
					else
					{
						out.append( indent);
						out.append( "open");
					}
					indent.append( "  ");
					break;
				case BracketMode:
					if (itemcnt)
					{
						out.append( ", ");
						itemcnt = 0;
					}
					out.append( "{");
					break;
			}
			if (printlevel == 0)
			{
				switch (mode)
				{
					case LineMode:
						out.append( indent);
						out.append( "...");
						break;
					case BracketMode:
						out.append( "...");
						break;
				}
			}
		}
	}

	bool printClose()
	{
		++printlevel;
		if (printlevel >= 0)
		{
			switch (mode)
			{
				case LineMode:
					indent.resize( indent.size()-2);
					out.append( indent);
					out.append( "close");
					break;
				case BracketMode:
					out.append( "}");
					itemcnt++;
					break;
			}
		}
		return true;
	}

	void printName( const std::string& value)
	{
		if (printlevel >= 0)
		{
			switch (mode)
			{
				case LineMode:
					out.append( indent);
					out.append( "name ");
					out.append( value);
					break;
				case BracketMode:
					if (itemcnt)
					{
						out.append( ", ");
						itemcnt = 0;
					}
					out.append( value);
					out.append( ":");
					break;
			}
		}
	}

	void printNullValue()
	{
		if (printlevel >= 0)
		{
			switch (mode)
			{
				case LineMode:
					out.append( indent);
					out.append( "value NULL");
					break;
				case BracketMode:
					if (itemcnt++)
					{
						out.append( ", ");
					}
					out.append( "NULL");
					break;
			}
		}
	}

	void printObject( const char* typenam)
	{
		if (printlevel >= 0)
		{
			switch (mode)
			{
				case LineMode:
					out.append( indent);
					out.append( "value <");
					out.append( typenam);
					out.append( ">");
					break;
				case BracketMode:
					if (itemcnt++)
					{
						out.append( ", ");
					}
					out.append( "<");
					out.append( typenam);
					out.append( ">");
					break;
			}
		}
	}

	void printNumeric( const std::string& value)
	{
		if (printlevel >= 0)
		{
			switch (mode)
			{
				case LineMode:
					out.append( indent);
					out.append( "value ");
					out.append( value);
					break;
				case BracketMode:
					if (itemcnt++)
					{
						out.append( ", ");
					}
					out.append( value);
					break;
			}
		}
	}

	void printString( const std::string& value)
	{
		if (printlevel >= 0)
		{
			switch (mode)
			{
				case LineMode:
					out.append( indent);
					out.append( "value \"");
					out.append( value);
					out.append( "\"");
					break;
				case BracketMode:
					if (itemcnt++)
					{
						out.append( ", ");
					}
					out.append( "\"");
					out.append( value);
					out.append( "\"");
					break;
			}
		}
	}

	bool printValue( const papuga_ValueVariant& value)
	{
		if (printlevel >= 0)
		{
			if (!papuga_ValueVariant_defined( &value))
			{
				printNullValue();
			}
			else if (!papuga_ValueVariant_isatomic( &value))
			{
				if (value.valuetype == papuga_TypeSerialization)
				{
					if (!Serialization_print( *this, value.value.serialization)) return false;
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
		return true;
	}

	void printSubStruct( const std::multimap<std::string,std::string>& map)
	{
		if (printlevel >= 0)
		{
			std::multimap<std::string,std::string>::const_iterator mi = map.begin(), me = map.end();
			for (int midx=0; mi != me; ++mi,++midx)
			{
				if (mode == BracketMode && midx)
				{
					out.append( ", ");
				}
				if (!mi->first.empty())
				{
					printName( mi->first);
				}
				out.append( mi->second);
			}
		}
	}
};

static bool SerializationIter_print( PrintStruct& ctx, papuga_SerializationIter& seriter)
{
	int taglevel = 0;
	for (; !papuga_SerializationIter_eof(&seriter); papuga_SerializationIter_skip(&seriter))
	{
		switch (papuga_SerializationIter_tag(&seriter))
		{
			case papuga_TagOpen:
				++taglevel;
				if (papuga_SerializationIter_value(&seriter)->valuetype == papuga_TypeInt)
				{
					ctx.printOpen( papuga_SerializationIter_value(&seriter)->value.Int);
				}
				else
				{
					ctx.printOpen( 0);
				}
				break;
			case papuga_TagClose:
				--taglevel;
				if (taglevel < 0) return true;
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
	if (taglevel != 0)
	{
		ctx.errcode = papuga_UnexpectedEof;
		return false;
	}
	return true;
}

static bool SerializationIter_print_det( PrintStruct& ctx, papuga_SerializationIter& seriter)
{
	std::multimap<std::string,std::string> map;
	std::string name;
	for (; !papuga_SerializationIter_eof(&seriter); papuga_SerializationIter_skip(&seriter))
	{
		switch (papuga_SerializationIter_tag(&seriter))
		{
			case papuga_TagOpen:
			{
				PrintStruct subctx( ctx.mode, ctx.indent, ctx.printlevel, ctx.deterministic);
				if (papuga_SerializationIter_value(&seriter)->valuetype == papuga_TypeInt)
				{
					subctx.printOpen( papuga_SerializationIter_value(&seriter)->value.Int);
				}
				else
				{
					subctx.printOpen( 0);
				}
				papuga_SerializationIter_skip(&seriter);
				bool subrt = (ctx.printlevel >= 0) ? SerializationIter_print_det( subctx, seriter) : SerializationIter_print( subctx, seriter);
				if (!subrt)
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
				map.insert( std::pair<std::string,std::string>( name, subctx.out));
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
				PrintStruct subctx( ctx.mode, ctx.indent, ctx.printlevel, ctx.deterministic);
				if (!subctx.printValue( *papuga_SerializationIter_value( &seriter)))
				{
					ctx.errcode = subctx.errcode;
					return false;
				}
				map.insert( std::pair<std::string,std::string>( name, subctx.out));
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
	if (serialization->structid)
	{
		ctx.printOpen( serialization->structid);
	}
	else
	{
		ctx.printOpen( 0);
	}
	bool rt = ctx.deterministic ? SerializationIter_print_det( ctx, seriter) : SerializationIter_print( ctx, seriter);
	if (rt)
	{
		if (!papuga_SerializationIter_eof( &seriter))
		{
			ctx.errcode = papuga_SyntaxError;
			return false;
		}
		ctx.printClose();
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
		PrintStruct ctx( linemode?LineMode : BracketMode, linemode?"\n":"", maxdepth, false/*non deterministic*/);
		if (!Serialization_print( ctx, self))
		{
			*errcode = ctx.errcode;
			return NULL;
		}
		return stringCopyAsCString( ctx.out, allocator);
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
		PrintStruct ctx( linemode?LineMode : BracketMode, linemode?"\n":""/*indent*/, maxdepth, false/*non deterministic*/);
		if (!Serialization_print( ctx, &value))
		{
			errcode = ctx.errcode;
			return std::string();
		}
		return ctx.out;
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
		PrintStruct ctx( linemode?LineMode : BracketMode, linemode?"\n":""/*indent*/, maxdepth, true/*deterministic*/);
		if (!Serialization_print( ctx, &value))
		{
			errcode = ctx.errcode;
			return std::string();
		}
		return ctx.out;
	}
	catch (const std::bad_alloc&)
	{
		errcode = papuga_NoMemError;
		return std::string();
	}
}

