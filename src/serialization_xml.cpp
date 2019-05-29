/*
 * Copyright (c) 2019 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/// \brief Serialize json structure
/// \file serialization_json.cpp
#include "papuga/serialization.h"
#include "papuga/valueVariant.h"
#include "papuga/allocator.h"
#include "papuga/serialization.h"
#include "papuga/constants.h"
#include "textwolf/xmlscanner.hpp"
#include "textwolf/charset.hpp"
#include "requestParser_utils.h"
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#undef PAPUGA_LOWLEVEL_DEBUG

typedef textwolf::XMLScanner<
		textwolf::SrcIterator,
		textwolf::charset::UTF8,
		textwolf::charset::UTF8,
		std::string
	> XMLScanner;

struct TagDef
{
	std::string name;
	int pos;

	TagDef() :name(),pos(0){}
	TagDef( const char* name_, int namelen_, int pos_) :name(name_,namelen_),pos(pos_){}
	TagDef( const TagDef& o) :name(o.name),pos(o.pos){}
	TagDef& operator=( const TagDef& o) {name=o.name;pos=o.pos; return *this;}
};

static bool getArrays( int* buf, int bufsize, int* len, const char* content, std::size_t contentlen, papuga_ErrorCode* errcode)
{
	textwolf::SrcIterator srciter;
	XMLScanner scanner;
	typename XMLScanner::iterator itr;
	typename XMLScanner::iterator end;

	*len = 0;
	int cnt = 0;

	std::vector<TagDef> tagstack;
	std::size_t tagstacksize = 0;

	typedef textwolf::XMLScannerBase tx;
	{
		jmp_buf eom;
		srciter.putInput( content, contentlen+1, &eom);
		scanner.setSource( srciter);
		itr = scanner.begin( false);
		end = scanner.end();

		if (setjmp(eom) != 0)
		{
			*errcode = papuga_UnexpectedEof;
			return false;
		}
		while (itr != end)
		{
			++itr;
			++cnt;
			switch (itr->type())
			{
				case tx::None:
				case tx::Exit:
					return true;
				case tx::ErrorOccurred:
					*errcode = papuga_SyntaxError;
					return false;
				case tx::HeaderStart:
				case tx::HeaderAttribName:
				case tx::HeaderAttribValue:
				case tx::HeaderEnd:
				case tx::DocAttribValue:
				case tx::DocAttribEnd:
				case tx::TagAttribName:
				case tx::TagAttribValue:
					break;
				case tx::OpenTag:
					try
					{
						if (tagstacksize > tagstack.size())
						{
							*errcode = papuga_LogicError;
							return false;
						}
						else if (tagstacksize == tagstack.size())
						{
							tagstack.push_back( TagDef( itr->content(), itr->size(), cnt));
							++tagstacksize;
						}
						else
						{
							if (tagstacksize+1 < tagstack.size())
							{
								tagstack.resize( tagstacksize+1);
							}
							if (itr->size() == tagstack.back().name.size()
								&& 0==std::memcmp( itr->content(), tagstack.back().name.c_str(), itr->size()))
							{
								if (*len > 0 &&  buf[*len-1] == tagstack.back().pos)
								{
									++tagstacksize;
								}
								else
								{
									if (*len > bufsize)
									{
										*errcode = papuga_BufferOverflowError;
										return false;
									}
									buf[ *len] = tagstack.back().pos;
									++*len;
									++tagstacksize;
								}
							}
							else
							{
								tagstack.back() = TagDef( itr->content(), itr->size(), cnt);
								++tagstacksize;
							}
						}
					}
					catch (const std::bad_alloc&)
					{
						*errcode = papuga_NoMemError;
						return false;
					}
					break;
				case tx::CloseTag:
				case tx::CloseTagIm:
					--tagstacksize;
					break;
				case tx::Content:
					break;
			}
		}
	}
	return true;
}

static bool isEmptyContent( const char* str, std::size_t size)
{
	std::size_t si = 0;
	for (; si < size && (unsigned char)str[si] <= 32; ++si){}
	return si == size;
}

extern "C" bool papuga_Serialization_append_xml( papuga_Serialization* self, const char* content, size_t contentlen, papuga_StringEncoding enc, bool withRoot, bool ignoreEmptyContent, papuga_ErrorCode* errcode)
{
	struct Structure
	{
		enum {MaxNofAttributes=32};
		const char* m_name;
		int m_namelen;
		struct {
			const char* name;
			int namelen;
			const char* value;
			int valuelen;
		} m_attributes[ MaxNofAttributes];
		int m_nofAttributes;
		const char* m_content;
		int m_contentlen;
		bool m_hasOpen;
		bool m_hasClose;
		papuga_ErrorCode* m_errcode;

		explicit Structure( papuga_ErrorCode* errcode_)
			:m_errcode(errcode_)
		{
			reset();
		}

		void resetAttribute( int idx)
		{
			m_attributes[ idx].name = 0;
			m_attributes[ idx].namelen = 0;
			m_attributes[ idx].value = 0;
			m_attributes[ idx].valuelen = 0;
		}
		void reset()
		{
			m_name = 0;
			m_namelen = 0;
			resetAttribute( 0);
			m_nofAttributes = 0;
			m_content = 0;
			m_contentlen = 0;
			m_hasOpen = false;
			m_hasClose = false;
		}
		bool addOpen( const char* str, int sz)
		{
			if (m_nofAttributes || m_content || m_hasOpen)
			{
				*m_errcode = papuga_SyntaxError;
				return false;
			}
			m_name = str;
			m_namelen = sz;
			m_hasOpen = true;
			return true;
		}
		bool addOpen()
		{
			if (m_nofAttributes || m_content || m_hasOpen)
			{
				*m_errcode = papuga_SyntaxError;
				return false;
			}
			m_hasOpen = true;
			return true;
		}
		bool addClose()
		{
			m_hasClose = true;
			return true;
		}
		bool addAttributeName( const char* str, int sz)
		{
			if (m_nofAttributes >= MaxNofAttributes-1)
			{
				*m_errcode = papuga_BufferOverflowError;
				return false;
			}
			if (m_attributes[ m_nofAttributes].name || m_content || sz == 0)
			{
				*m_errcode = papuga_SyntaxError;
				return false;
			}
			m_attributes[ m_nofAttributes].name = str;
			m_attributes[ m_nofAttributes].namelen = sz;
			return true;
		}
		bool addAttributeValue( const char* str, int sz)
		{
			if (m_nofAttributes >= MaxNofAttributes-1)
			{
				*m_errcode = papuga_BufferOverflowError;
				return false;
			}
			if (!m_attributes[ m_nofAttributes].name || m_content)
			{
				*m_errcode = papuga_SyntaxError;
				return false;
			}
			m_attributes[ m_nofAttributes].value = str;
			m_attributes[ m_nofAttributes].valuelen = sz;
			++m_nofAttributes;
			resetAttribute( m_nofAttributes);
			return true;
		}
		bool addContentValue( const char* str, int sz)
		{
			if (m_content)
			{
				*m_errcode = papuga_SyntaxError;
				return false;
			}
			m_content = str;
			m_contentlen = sz;
			return true;
		}
		bool flushStructure( papuga_Serialization* self)
		{
			bool rt = true;
			if (m_name)
			{
				rt &= papuga_Serialization_pushName_string( self, m_name, m_namelen);
			}
			if (m_nofAttributes)
			{
				if (m_hasOpen)
				{
					rt &= papuga_Serialization_pushOpen( self);
				}
				int ai = 0, ae = m_nofAttributes;
				for (; ai != ae; ++ai)
				{
					rt &= papuga_Serialization_pushName_string( self, m_attributes[ ai].name, m_attributes[ ai].namelen);
					rt &= papuga_Serialization_pushValue_string( self, m_attributes[ ai].value, m_attributes[ ai].valuelen);
				}
				if (m_content)
				{
					rt &= papuga_Serialization_pushName_string( self, "", 0);
					rt &= papuga_Serialization_pushValue_string( self, m_content, m_contentlen);
				}
				if (m_hasClose)
				{
					papuga_Serialization_pushClose( self);
				}
			}
			else
			{
				if (m_hasOpen && m_hasClose)
				{
					if (m_content)
					{
						rt &= papuga_Serialization_pushValue_string( self, m_content, m_contentlen);
					}
					else
					{
						rt &= papuga_Serialization_pushValue_void( self);
					}
				}
				else
				{
					if (m_hasOpen)
					{
						rt &= papuga_Serialization_pushOpen( self);
					}
					if (m_content)
					{
						rt &= papuga_Serialization_pushValue_string( self, m_content, m_contentlen);
					}
					if (m_hasClose)
					{
						papuga_Serialization_pushClose( self);
					}
				}
			}
			reset();
			return rt;
		}
	};

	struct TagStack
	{
		struct OpenFlags
		{
			bool isArray;
			bool isNew;
			bool isEndOfArray;
			bool isRoot;
		};
		struct CloseFlags
		{
			bool isEndOfArray;
			bool isRoot;
		};

		int m_arrays[ PAPUGA_MAX_RECURSION_DEPTH];
		int m_nofarrays;
		int m_arrayidx;
		int m_cnt;
		int m_depth;
		papuga_ErrorCode* m_errcode;

		struct {
			const char* name;
			int namelen;
			bool isArrayElem;
		} m_arrayStack[ PAPUGA_MAX_RECURSION_DEPTH]; //stack of last array element names
		
		explicit TagStack( papuga_ErrorCode* errcode_)
			:m_errcode(errcode_)
		{
			m_arrayStack[ 0].name = 0;
			m_arrayStack[ 0].namelen = -1;
			m_arrayStack[ 0].isArrayElem = false;
			m_nofarrays = 0;
			m_arrayidx = 0;
			m_cnt = 0;
			m_depth = 0;
		}
		bool init( const char* content, int contentlen)
		{
			return getArrays( m_arrays, PAPUGA_MAX_RECURSION_DEPTH/*bufsize*/, &m_nofarrays, content, contentlen, m_errcode);
		}
		void next()
		{
			++m_cnt;
		}
		bool push( const char* tagname, int tagnamelen, OpenFlags& flags)
		{
			if (m_depth >= PAPUGA_MAX_RECURSION_DEPTH-1)
			{
				*m_errcode = papuga_MaxRecursionDepthReached;
				return false;
			}
			++m_depth;
			flags.isRoot = m_depth == 1;
			if (m_arrayStack[ m_depth].namelen == tagnamelen && 0==std::memcmp( m_arrayStack[ m_depth].name, tagname, tagnamelen))
			{
				flags.isNew = false;
				flags.isArray = true;
				flags.isEndOfArray = false;
			}
			else
			{
				flags.isNew = true;
				flags.isEndOfArray = m_arrayStack[ m_depth].isArrayElem;
				while (m_arrayidx < m_nofarrays && m_arrays[ m_arrayidx] < m_cnt) ++m_arrayidx;
				flags.isArray = m_arrays[ m_arrayidx] == m_cnt;
				m_arrayStack[ m_depth].namelen = tagnamelen;
				m_arrayStack[ m_depth].name = tagname;
			}
			m_arrayStack[ m_depth].isArrayElem = flags.isArray;

			m_arrayStack[ m_depth+1].name = 0;
			m_arrayStack[ m_depth+1].namelen = -1;
			m_arrayStack[ m_depth+1].isArrayElem = false;
			return true;
		}
		bool pop( CloseFlags& flags)
		{
			flags.isEndOfArray = m_arrayStack[ m_depth+1].isArrayElem;
			flags.isRoot = m_depth == 1;
			if (m_depth == 0)
			{
				*m_errcode = papuga_SyntaxError;
				return false;
			}
			else
			{
				--m_depth;
				return true;
			}
		}
		bool end( CloseFlags& flags)
		{
			flags.isEndOfArray = m_arrayStack[ m_depth+1].isArrayElem;
			return true;
		}
	};

	Structure currentStruct( errcode);
	TagStack tagStack( errcode);

	textwolf::SrcIterator srciter;
	XMLScanner scanner;
	typename XMLScanner::iterator itr;
	typename XMLScanner::iterator end;
	bool rt = true;

	if (enc != papuga_UTF8)
	{
		// Convert input to UTF8 as cjson is only capable of parsing UTF8
		papuga_ValueVariant val;
		papuga_init_ValueVariant_string_enc( &val, enc, content, contentlen);
		content = papuga_ValueVariant_tostring( &val, self->allocator, &contentlen, errcode);
		if (!content) return false;
	}
	if (content[contentlen] != 0)
	{
		content = papuga_Allocator_copy_string( self->allocator, content, contentlen);
	}

	if (!tagStack.init( content, contentlen))
	{
		return false;
	}
	typedef textwolf::XMLScannerBase tx;
	{
		jmp_buf eom;
		srciter.putInput( content, contentlen+1, &eom);
		scanner.setSource( srciter);
		itr = scanner.begin( false);
		end = scanner.end();

		if (setjmp(eom) != 0)
		{
			*errcode = papuga_UnexpectedEof;
			return false;
		}
		while (rt && itr != end)
		{
			++itr;
			tagStack.next();

			int valsize = itr->size();
			const char* valstr = "";
			if (itr->size() > 0)
			{
				valstr = papuga_Allocator_copy_string( self->allocator, itr->content(), itr->size());
				if (!valstr)
				{
					*errcode = papuga_NoMemError;
					return false;
				}
			}
#ifdef PAPUGA_LOWLEVEL_DEBUG
			std::cerr << "ELEM " << tx::getElementTypeName( itr->type()) << " '" << valstr << "'" << std::endl;
#endif
			switch (itr->type())
			{
				case tx::None:
					*errcode = papuga_ValueUndefined;
					return false;
				case tx::Exit:
				{
					TagStack::CloseFlags flags;
					rt &= tagStack.end( flags);
					if (flags.isEndOfArray)
					{
						rt &= papuga_Serialization_pushClose( self);
					}
					rt &= currentStruct.flushStructure( self);
					return true;
				}
				case tx::ErrorOccurred:
					*errcode = papuga_SyntaxError;
					return false;
				case tx::HeaderStart:
				case tx::HeaderAttribName:
				case tx::HeaderAttribValue:
				case tx::HeaderEnd:
				case tx::DocAttribValue:
				case tx::DocAttribEnd:
					break;
				case tx::TagAttribName:
					rt &= currentStruct.addAttributeName( valstr, valsize);
					break;
				case tx::TagAttribValue:
					rt &= currentStruct.addAttributeValue( valstr, valsize);
					break;
				case tx::OpenTag:
				{
					TagStack::OpenFlags flags;
					rt &= currentStruct.flushStructure( self);
					rt &= tagStack.push( valstr, valsize, flags);
					if (flags.isEndOfArray)
					{
						rt &= papuga_Serialization_pushClose( self);
					}
					if (flags.isArray)
					{
						if (flags.isNew)
						{
							if (withRoot || !flags.isRoot)
							{
								rt &= papuga_Serialization_pushName_string( self, valstr, valsize);
								rt &= papuga_Serialization_pushOpen( self);
							}
						}
						rt &= currentStruct.addOpen();
					}
					else if (withRoot || !flags.isRoot)
					{
						rt &= currentStruct.addOpen( valstr, valsize);
					}
					break;
				}
				case tx::CloseTag:
				case tx::CloseTagIm:
				{
					TagStack::CloseFlags flags;
					rt &= tagStack.pop( flags);
					if (flags.isEndOfArray)
					{
						rt &= papuga_Serialization_pushClose( self);
					}
					if (withRoot || !flags.isRoot)
					{
						rt &= currentStruct.addClose();
					}
					rt &= currentStruct.flushStructure( self);
					break;
				}
				case tx::Content:
					if (ignoreEmptyContent && isEmptyContent( valstr, valsize))
					{}
					else
					{
						rt &= currentStruct.addContentValue( valstr, valsize);
					}
					break;
			}
		}
	}
	return rt;
}



