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

static bool Serialization_print( std::ostream& out, std::string indent, const papuga_Serialization* serialization, papuga_ErrorCode& errcode)
{
	papuga_SerializationIter seriter;
	papuga_init_SerializationIter( &seriter, serialization);

	for (; !papuga_SerializationIter_eof(&seriter); papuga_SerializationIter_skip(&seriter))
	{
		switch (papuga_SerializationIter_tag(&seriter))
		{
			case papuga_TagOpen:
				out << indent << "open" << std::endl;
				indent.append( "  ");
				break;
			case papuga_TagClose:
				if (indent.size() < 2)
				{
					errcode = papuga_TypeError;
					return false;
				}
				indent.resize( indent.size()-2);
				out << indent << "close" << std::endl;
				break;
			case papuga_TagName:
				out << indent << "name " << ValueVariant_tostring( *papuga_SerializationIter_value( &seriter), errcode) << std::endl;
				break;
			case papuga_TagValue:
			{
				const papuga_ValueVariant* val = papuga_SerializationIter_value( &seriter);
				if (!papuga_ValueVariant_defined( val))
				{
					out << indent << "value NULL" << std::endl;
				}
				else if (!papuga_ValueVariant_isatomic( val))
				{
					if (val->valuetype == papuga_TypeSerialization)
					{
						out << indent << "open (serialization)" << std::endl;
						indent.append( "  ");
						if (!Serialization_print( out, indent, val->value.serialization, errcode)) return false;
						indent.resize( indent.size()-2);
						out << indent << "close (serialization)" << std::endl;
					}
					else if (val->valuetype == papuga_TypeHostObject)
					{
						out << indent << "value HOSTOBJ" << std::endl;
					}
					else if (val->valuetype == papuga_TypeIterator)
					{
						out << indent << "value ITERATOR" << std::endl;
					}
					else
					{
						errcode = papuga_TypeError;
						return false;
					}
				}
				else if (papuga_ValueVariant_isstring( val))
				{
					out << indent << "value '" << ValueVariant_tostring( *val, errcode) << "'" << std::endl;
				}
				else
				{
					out << indent << "value " << ValueVariant_tostring( *val, errcode) << std::endl;
				}
				break;
			}
			default:
			{
				errcode = papuga_TypeError;
				return false;
			}
		}
	}
	return true;
}

class DetSerPrinter
{
public:
	DetSerPrinter()
		:dict(),name(),name_set(false),arraycontent(){}

	bool setName( const std::string& name_, papuga_ErrorCode& errcode)
	{
		if (name_set)
		{
			errcode = papuga_TypeError;
			return false; 
		}
		name = name_;
		name_set = true;
		return true;
	}

	bool setValue( const std::string& value, papuga_ErrorCode& errcode)
	{
		if (name_set)
		{
			dict[ name].append( value);
			name.clear();
			name_set = false;
		}
		else
		{
			arraycontent.append( value);
		}
		return true;
	}

	bool print( std::ostream& out, const std::string& indent, papuga_ErrorCode& errcode) const
	{
		if (!name.empty())
		{
			errcode = papuga_TypeError;
			return false;
		}
		std::map<std::string,std::string>::const_iterator di = dict.begin(), de = dict.end();
		for (; di != de; ++di)
		{
			out << indent << "name " << di->first << std::endl;
			out << di->second;
		}
		out << arraycontent;
		return true;
	}

private:
	std::map<std::string,std::string> dict;
	std::string name;
	bool name_set;
	std::string arraycontent;
};

static bool Serialization_print_deterministic( std::ostream& out, std::string indent, papuga_SerializationIter& seriter, papuga_ErrorCode& errcode)
{
	DetSerPrinter prn;

	for (; papuga_SerializationIter_tag(&seriter) != papuga_TagClose; papuga_SerializationIter_skip(&seriter))
	{
		switch (papuga_SerializationIter_tag(&seriter))
		{
			case papuga_TagOpen:
			{
				papuga_SerializationIter_skip(&seriter);
				std::ostringstream valueout;
				valueout << indent << "open" << std::endl;
				indent.append( "  ");
				if (!Serialization_print_deterministic( valueout, indent, seriter, errcode)) return false;
				if (papuga_SerializationIter_tag(&seriter) != papuga_TagClose)
				{
					errcode = papuga_LogicError;
					return false;
				}
				if (papuga_SerializationIter_eof(&seriter))
				{
					errcode = papuga_UnexpectedEof;
					return false;
				}
				indent.resize( indent.size()-2);
				valueout << indent << "close" << std::endl;

				if (!prn.setValue( valueout.str(), errcode)) return false;
				break;
			}
			case papuga_TagClose:
				break;
			case papuga_TagName:
				if (!prn.setName( ValueVariant_tostring( *papuga_SerializationIter_value( &seriter), errcode), errcode)) return false;
				break;
			case papuga_TagValue:
			{
				std::ostringstream valueout;
				const papuga_ValueVariant* val = papuga_SerializationIter_value( &seriter);
				if (!papuga_ValueVariant_defined( val))
				{
					valueout << indent << "value NULL" << std::endl;
				}
				else if (!papuga_ValueVariant_isatomic( val))
				{
					if (val->valuetype == papuga_TypeSerialization)
					{
						papuga_SerializationIter subiter;
						papuga_init_SerializationIter( &subiter, val->value.serialization);
						valueout << indent << "open (serialization)" << std::endl;
						indent.append( "  ");
						if (!Serialization_print_deterministic( valueout, indent, subiter, errcode)) return false;
						indent.resize( indent.size()-2);
						valueout << indent << "close (serialization)" << std::endl;
					}
					else if (val->valuetype == papuga_TypeHostObject)
					{
						valueout << indent << "value HOSTOBJ" << std::endl;
					}
					else if (val->valuetype == papuga_TypeIterator)
					{
						valueout << indent << "value ITERATOR" << std::endl;
					}
					else
					{
						errcode = papuga_TypeError;
						return false;
					}
				}
				else if (papuga_ValueVariant_isstring( val))
				{
					valueout << indent << "value '" << ValueVariant_tostring( *val, errcode) << "'" << std::endl;
				}
				else
				{
					valueout << indent << "value " << ValueVariant_tostring( *val, errcode) << std::endl;
				}
				if (!prn.setValue( valueout.str(), errcode)) return false;
				break;
			}
			default:
			{
				errcode = papuga_TypeError;
				return false;
			}
		}
	}
	if (!prn.print( out, indent, errcode)) return false;
	return (errcode == papuga_Ok);
}

extern "C" const char* papuga_Serialization_tostring( const papuga_Serialization* self, papuga_Allocator* allocator)
{
	try
	{
		if (!self) return 0;
		papuga_ErrorCode errcode = papuga_Ok;
		std::string indent;
		std::ostringstream out;
		if (!Serialization_print( out, indent, self, errcode)) return NULL;
		return stringCopyAsCString( out.str(), allocator);
	}
	catch (const std::bad_alloc&)
	{
		return 0;
	}
}

extern "C" const char* papuga_Serialization_print_node( const papuga_Node* nd, char* buf, size_t bufsize)
{
	try
	{
		std::ostringstream out;
		papuga_ErrorCode errcode = papuga_Ok;
		switch ((papuga_Tag)nd->content._tag)
		{
			case papuga_TagOpen:
				out << "open";
				break;
			case papuga_TagClose:
				out << "close";
				break;
			case papuga_TagName:
				if (papuga_ValueVariant_isatomic( &nd->content))
				{
					out << "name " << ValueVariant_tostring( nd->content, errcode);
				}
				else
				{
					out << "name <" << papuga_Type_name( (papuga_Type)nd->content.valuetype) << ">";
				}
				break;
			case papuga_TagValue:
				if (papuga_ValueVariant_isatomic( &nd->content))
				{
					out << "value " << ValueVariant_tostring( nd->content, errcode);
				}
				else
				{
					out << "value <" << papuga_Type_name( (papuga_Type)nd->content.valuetype) << ">";
				}
				break;
		}
		std::string str( out.str());
		std::size_t len = str.size() >= bufsize ? (bufsize - 1):str.size();
		std::memcpy( buf, str.c_str(), len);
		buf[ len] = 0;
		return buf;
	}
	catch (const std::bad_alloc&)
	{
		return 0;
	}
}

std::string papuga::Serialization_tostring( const papuga_Serialization& value, papuga_ErrorCode& errcode)
{
	try
	{
		std::ostringstream out;
		if (!Serialization_print( out, std::string(), &value, errcode))
		{
			return std::string();
		}
		return out.str();
	}
	catch (const std::bad_alloc&)
	{
		errcode = papuga_NoMemError;
		return std::string();
	}
}

std::string papuga::Serialization_tostring_deterministic( const papuga_Serialization& value, papuga_ErrorCode& errcode)
{
	try
	{
		std::ostringstream out;
		papuga_SerializationIter seriter;
		papuga_init_SerializationIter( &seriter, &value);

		if (!Serialization_print_deterministic( out, std::string(), seriter, errcode))
		{
			return std::string();
		}
		if (!papuga_SerializationIter_eof( &seriter))
		{
			errcode = papuga_TypeError;
			return std::string();
		}
		return out.str();
	}
	catch (const std::bad_alloc&)
	{
		errcode = papuga_NoMemError;
		return std::string();
	}
}

