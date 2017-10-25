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

#undef PAPUGA_LOWLEVEL_DEBUG

/// \brief Pseudo random generator 
enum {KnuthIntegerHashFactor=2654435761U};

static inline uint32_t uint32_hash( uint32_t a)
{
	a += ~(a << 15);
	a ^=  (a >> 10);
	a +=  (a << 3);
	a ^=  (a >> 6);
	a += ~(a << 11);
	a ^=  (a >> 16);
	return a;
}

class Random
{
public:
	Random()
	{
		time_t nowtime;
		struct tm* now;

		::time( &nowtime);
		now = ::localtime( &nowtime);

		m_value = uint32_hash( ((now->tm_year+1)
					* (now->tm_mon+100)
					* (now->tm_mday+1)));
		m_incr = m_value * KnuthIntegerHashFactor;
	}

	unsigned int get( unsigned int min_, unsigned int max_)
	{
		if (min_ >= max_) throw std::runtime_error("illegal range passed to pseudo random number generator");

		m_value = uint32_hash( m_value + 1 + m_incr++);
		unsigned int iv = max_ - min_;
		return (m_value % iv) + min_;
	}

private:
	unsigned int m_value;
	unsigned int m_incr;
};

static Random g_random;

class RandomValue
{
public:
	void init( const papuga_Tag& tag)
	{
		if (g_random.get( 0, 2) == 0)
		{
			papuga_init_ValueVariant_int( &m_val, g_random.get( 0, 0x7fFFffFFU));
		}
		else
		{
			unsigned int se = g_random.get( 0, 1 << g_random.get( 0, 4));
			unsigned int si = 0;
			for (; si != se; ++si)
			{
				m_str.push_back( 'a' + g_random.get( 0, 26));
			}
			papuga_init_ValueVariant_string( &m_val, m_str.c_str(), m_str.size());
		}
		m_val._tag = tag;
	}

	RandomValue()
		:m_str()
	{
		init( (papuga_Tag)g_random.get( 0, 4));
	}

	RandomValue( const papuga_Tag& tag)
		:m_str()
	{
		init( tag);
	}

	RandomValue( const RandomValue& o)
		:m_str(o.m_str)
	{
		std::memcpy( &m_val, &o.m_val, sizeof(m_val));
		if (m_val.valuetype == papuga_TypeString || m_val.valuetype == papuga_TypeLangString)
		{
			m_val.value.string = m_str.c_str();
		}
	}

	void push2ser( papuga_Serialization* ser) const
	{
		switch ((papuga_Tag)m_val._tag)
		{
			case papuga_TagValue:
				if (m_val.valuetype == papuga_TypeInt)
				{
					if (!papuga_Serialization_pushValue_int( ser, m_val.value.Int)) throw std::bad_alloc();
				}
				else if (m_val.valuetype == papuga_TypeString)
				{
					if (!papuga_Serialization_pushValue_string( ser, m_val.value.string, m_val.length)) throw std::bad_alloc();
				}
				else
				{
					if (!papuga_Serialization_pushValue( ser, &m_val)) throw std::bad_alloc();
				}
				break;
			case papuga_TagOpen:
				if (!papuga_Serialization_pushOpen( ser)) throw std::bad_alloc();
				break;
			case papuga_TagClose:
				if (!papuga_Serialization_pushClose( ser)) throw std::bad_alloc();
				break;
			case papuga_TagName:
				if (m_val.valuetype == papuga_TypeInt)
				{
					if (!papuga_Serialization_pushName_int( ser, m_val.value.Int)) throw std::bad_alloc();
				}
				else if (m_val.valuetype == papuga_TypeString)
				{
					if (!papuga_Serialization_pushName_string( ser, m_val.value.string, m_val.length)) throw std::bad_alloc();
				}
				else
				{
					if (!papuga_Serialization_pushName( ser, &m_val)) throw std::bad_alloc();
				}
				break;
		}
	}

	bool cmp( const papuga_SerializationIter& seritr) const
	{
		const papuga_ValueVariant* serval = papuga_SerializationIter_value( &seritr);
		if (m_val._tag != serval->_tag) return false;
		if (papuga_SerializationIter_tag( &seritr) == papuga_TagName
		||  papuga_SerializationIter_tag( &seritr) == papuga_TagValue)
		{
			if (m_val.encoding != serval->encoding) return false;
			if (m_val.length != serval->length) return false;
			if (m_val.valuetype != serval->valuetype) return false;
			if (m_val.valuetype == papuga_TypeInt)
			{
				if (m_val.value.Int != serval->value.Int) return false;
			}
			else if (m_val.valuetype == papuga_TypeString)
			{
				if (std::strcmp( m_val.value.string, serval->value.string) != 0) return false;
			}
			else
			{
				return false;
			}
		}
		return true;
	}

	const papuga_ValueVariant& value() const	{return m_val;}

private:
	papuga_ValueVariant m_val;
	std::string m_str;
};

std::vector<RandomValue> createRandomSerialization( unsigned int size)
{
	int bcnt = 0;
	std::vector<RandomValue> rt;
	papuga_Tag lasttag = papuga_TagValue;
	unsigned int si = 0, se = size;
	for (; si != se; ++si)
	{
		RandomValue val;
		if (lasttag == papuga_TagName && (val.value()._tag == papuga_TagName || val.value()._tag == papuga_TagClose))
		{
			--si;
			continue;
		}
		if (val.value()._tag == papuga_TagOpen)
		{
			if (bcnt > (1 << (int)sqrt( g_random.get( 1, 10))))
			{
				--si;
				continue;
			}
			++bcnt;
		}
		else if (val.value()._tag == papuga_TagClose)
		{
			if (bcnt <= 0)
			{
				--si;
				continue;
			}
			--bcnt;
		}
		rt.push_back( val);
		lasttag = (papuga_Tag)val.value()._tag;
	}
	for (;bcnt > 0; --bcnt)
	{
		rt.push_back( RandomValue( papuga_TagClose));
	}
	return rt;
}

std::vector<RandomValue> createRandomSerializationArray( unsigned int arsize, unsigned int maxelemsize)
{
	std::vector<RandomValue> rt;
	unsigned int ai = 0, ae = arsize;
	for (; ai != ae; ++ai)
	{
		if (g_random.get( 0, 10) < 2)
		{
			rt.push_back( RandomValue( papuga_TagValue));
		}
		else
		{
			std::vector<RandomValue> elem = createRandomSerialization( maxelemsize);
			rt.push_back( RandomValue( papuga_TagOpen));
			rt.insert( rt.end(), elem.begin(), elem.end());
			rt.push_back( RandomValue( papuga_TagClose));
		}
	}
	return rt;
}


int main( int argc, const char* argv[])
{
	if (argc <= 1 || std::strcmp( argv[1], "-h") == 0 || std::strcmp( argv[1], "--help") == 0)
	{
		std::cerr << "testSerialization <nodes> <arraysize>" << std::endl
				<< "\t<nodes>      :Number of nodes in the random serialization to test" << std::endl
				<< "\t<arraysize>  :Number of array elements in the random serialization to test" << std::endl;
		return 0;
	}
	try
	{
		unsigned int nodes = atoi( argv[1]);
		unsigned int arraysize = argv[2] ? atoi( argv[2]):3;
		{
			std::vector<RandomValue> ar = createRandomSerialization( nodes);
			papuga_Allocator allocator;
			papuga_Serialization ser;
			papuga_SerializationIter seritr;

			papuga_init_Allocator( &allocator, 0, 0);
			papuga_init_Serialization( &ser, &allocator);

			std::vector<RandomValue>::const_iterator ai = ar.begin(), ae = ar.end();
			for (; ai != ae; ++ai)
			{
				ai->push2ser( &ser);
			}
			papuga_init_SerializationIter( &seritr, &ser);
			int aidx = 1;
			for (ai = ar.begin(); ai != ae; ++ai,papuga_SerializationIter_skip(&seritr),++aidx)
			{
				if (papuga_SerializationIter_eof(&seritr))
				{
					throw std::runtime_error( std::string("unexpected end of random serialization"));
				}
				if (!ai->cmp( seritr))
				{
					char buf[ 64];
					std::snprintf( buf, sizeof( buf), "%d", aidx);
					throw std::runtime_error( std::string("diff in random serialization compared to source at index ") + buf);
				}
			}
			if (!papuga_SerializationIter_eof(&seritr))
			{
				throw std::runtime_error( std::string("unexpected elements in random serialization at end of source"));
			}
#ifdef PAPUGA_LOWLEVEL_DEBUG
			papuga_ErrorCode errcode = papuga_Ok;
			std::cout << papuga::Serialization_tostring( ser, errcode) << std::endl;
			if (errcode != papuga_Ok) throw std::runtime_error( papuga_ErrorCode_tostring( errcode));
#endif
			papuga_destroy_Allocator( &allocator);
			std::cerr << "1) random fill test" << std::endl;
		}{
			std::vector<RandomValue> ar = createRandomSerializationArray( arraysize, nodes);
			papuga_ErrorCode errcode = papuga_Ok;
			papuga_Allocator allocator;
			papuga_Serialization ser;
			papuga_SerializationIter seritr;

			papuga_init_Allocator( &allocator, 0, 0);
			papuga_init_Serialization( &ser, &allocator);
			std::vector<RandomValue>::const_iterator ai = ar.begin(), ae = ar.end();
			for (; ai != ae; ++ai)
			{
				ai->push2ser( &ser);
			}
			papuga_init_SerializationIter( &seritr, &ser);
#ifdef PAPUGA_LOWLEVEL_DEBUG
			std::cout << "enumerated array serialization:" << std::endl;
			std::cout << papuga::Serialization_tostring( ser, errcode) << std::endl;
			if (errcode != papuga_Ok) throw std::runtime_error( papuga_ErrorCode_tostring( errcode));
#endif
			papuga_Serialization_convert_array_assoc( &ser, &seritr, 0, &errcode);

#ifdef PAPUGA_LOWLEVEL_DEBUG
			std::cout << "associative array serialization:" << std::endl;
			std::cout << papuga::Serialization_tostring( ser, errcode) << std::endl;
			if (errcode != papuga_Ok) throw std::runtime_error( papuga_ErrorCode_tostring( errcode));
#endif
			int bcnt = 0, acnt = 0;
			papuga_init_SerializationIter( &seritr, &ser);
			int aidx = 1;
			for (ai = ar.begin(); ai != ae; ++ai,papuga_SerializationIter_skip(&seritr),++aidx)
			{
				if (bcnt == 0)
				{
					if (papuga_SerializationIter_tag(&seritr) != papuga_TagName) throw std::runtime_error( "missing array element name");
					const papuga_ValueVariant* nameval = papuga_SerializationIter_value( &seritr);
					if (nameval->valuetype != papuga_TypeInt) throw std::runtime_error( "array element name expected to of type INT");
					if ((int)nameval->value.Int != acnt) throw std::runtime_error( "array element name not strictly ascending from 0");
					papuga_SerializationIter_skip(&seritr);
					++acnt;
				}
				if (papuga_SerializationIter_tag(&seritr) == papuga_TagOpen)
				{
					bcnt++;
				}
				if (papuga_SerializationIter_tag(&seritr) == papuga_TagClose)
				{
					bcnt--;
				}
				if (papuga_SerializationIter_eof(&seritr))
				{
					throw std::runtime_error( std::string( "unexpected end of random serialization array"));
				}
				if (!ai->cmp( seritr))
				{
					char buf[ 64];
					std::snprintf( buf, sizeof( buf), "%d", aidx);
					throw std::runtime_error( std::string("diff in random serialization compared to source at index ") + buf);
				}
			}
			if (!papuga_SerializationIter_eof(&seritr))
			{
				throw std::runtime_error( std::string("unexpected elements in random serialization at end of source"));
			}
			papuga_destroy_Allocator( &allocator);
			std::cerr << "2) enumerated to associative array transformation test" << std::endl;
		}
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

