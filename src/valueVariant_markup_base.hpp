/*
 * Copyright (c) 2019 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
#ifndef _PAPUGA_REQUEST_VALUE_VARIANT_MARKUP_BASE_HPP_INCLUDED
#define _PAPUGA_REQUEST_VALUE_VARIANT_MARKUP_BASE_HPP_INCLUDED
/// \brief Common base for the markup modules
/// \file valueVariant_markup_base.hpp
#include "papuga/serialization.h"
#include "papuga/serialization.hpp"
#include "papuga/valueVariant.hpp"
#include "papuga/valueVariant.h"
#include "papuga/allocator.h"
#include "papuga/typedefs.h"
#include "papuga/constants.h"
#include "papuga/callResult.h"
#include "papuga/interfaceDescription.h"
#include "papuga/uriEncode.h"
#include <string>
#include <cstring>
#include <cstdlib>
#include <stdexcept>

namespace papuga {
namespace markup {

class ErrorException
{
public:
	ErrorException( papuga_ErrorCode errcode_, const char* item_=0)
		:m_errcode(errcode_),m_item(item_){}
	ErrorException( const ErrorException& o)
		:m_errcode(o.m_errcode),m_item(o.m_item){}

	papuga_ErrorCode errcode() const	{return m_errcode;}
	const char* item() const		{return m_item;}

private:
	papuga_ErrorCode m_errcode;
	const char* m_item;
};


class OutputContextBase
{
public:
	OutputContextBase( const papuga_StructInterfaceDescription* structs_, int maxDepth_, papuga_StringEncoding encoding_);

	void reset();

	static bool isEqual( const char* val, const char* oth)
	{
		if (val == oth) return true;
		if (val[0] != oth[0]) return false;
		if (0==std::strcmp( val, oth)) return true;
		return false;
	}

	static bool isEqualAscii( const papuga_ValueVariant& val, const char* oth);

	static bool isAlpha( char ch)
	{
		return (ch|32) >= 'a' && (ch|32) <= 'z';
	}

	static bool hasProtocolPrefix( const papuga_ValueVariant& val);
	static bool isArray( const papuga_ValueVariant& val);

	static bool isAttributeName( const papuga_ValueVariant& name)
	{
		int pos = 0;
		papuga_ErrorCode errcode;
		return (papuga_ValueVariant_isstring( &name) && '-' == papuga_ValueVariant_nextchar( &name, &pos, &errcode));
	}

	static bool isAttributeName( const char* name)
	{
		return name[ 0] == '-';
	}

	void appendEncoded_Xml( const char* str, std::size_t len);
	void appendEncoded_AnsiC( const char* str, std::size_t len);
	void appendEncoded_Html5( const char* str, std::size_t len);
	void appendEncoded_Rfc3986( const char* str, std::size_t len);

	void appendDecoded_AttributeName( const char* str, std::size_t len);

	void appendAtomicValue_withEncoder( const papuga_ValueVariant& value, void (OutputContextBase::* encoder)( const char*, std::size_t));

	void appendAtomicValue( const papuga_ValueVariant& val);

	void consumeClose( papuga_SerializationIter& iter);

	static void* encodeRequestResultString( const std::string& out, papuga_Allocator* allocator, papuga_StringEncoding enc, size_t* len);

protected:
	std::string out;
	const papuga_StructInterfaceDescription* structs;
	int depth;
	int maxDepth;
	papuga_StringEncoding encoding;
};

}}//namespace
#endif
