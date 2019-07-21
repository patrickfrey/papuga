/*
 * Copyright (c) 2019 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
#ifndef _PAPUGA_REQUEST_RESULT_UTILS_HPP_INCLUDED
#define _PAPUGA_REQUEST_RESULT_UTILS_HPP_INCLUDED
/// \brief Private helper classes and structures to map request results
/// \file requestResult_utils.hpp
#include "papuga/typedefs.h"
#include "papuga/requestResult.h"
#include "request_utils.hpp"
#include <string>
#include <vector>
#include <utility>

namespace papuga {

/*
 * @brief Describes a value for a request result
 */
struct RequestResultItem
{
	papuga_RequestResultNodeType nodetype;
	const char* tagname;
	papuga_ValueVariant value;

	RequestResultItem( papuga_RequestResultNodeType nodetype_, const char* tagname_)
		:nodetype(nodetype_),tagname(tagname_)
	{
		papuga_init_ValueVariant( &value);
	}
	RequestResultItem( papuga_RequestResultNodeType nodetype_, const char* tagname_, const char* str_)
		:nodetype(nodetype_),tagname(tagname_)
	{
		papuga_init_ValueVariant_charp( &value, str_);
	}
	RequestResultItem( const RequestResultItem& o)
		:nodetype(o.nodetype),tagname(o.tagname)
	{
		papuga_init_ValueVariant_value( &value, &o.value);
	}
};

struct RequestResultInputElementRef
{
	Scope scope;
	int itemid;
	papuga_ResolveType resolvetype;
	int taglevel;
	papuga_ValueVariant* value;

	RequestResultInputElementRef( const Scope& scope_, int itemid_, papuga_ResolveType resolvetype_, int taglevel_, papuga_ValueVariant* value_)
		:scope(scope_),itemid(itemid_),resolvetype(resolvetype_),taglevel(taglevel_),value(value_){}
	RequestResultInputElementRef( const RequestResultInputElementRef& o)
		:scope(o.scope),itemid(o.itemid),resolvetype(o.resolvetype),taglevel(o.taglevel),value(o.value){}
};

/*
 * @brief Describes a seralization template for a request result
 */
class RequestResultTemplate
{
public:
	RequestResultTemplate()
		:m_name(0),m_schema(0),m_requestmethod(0),m_addressvar(0)
	{
		papuga_init_Allocator( &m_allocator, m_allocatormem, sizeof(m_allocatormem));
	}
	void addResultNodeInputReference( const Scope& scope_, const char* tagname_, int itemid_, papuga_ResolveType resolvetype_, int taglevel_)
	{
		m_inputrefs.push_back( InputRef( scope_, itemid_, resolvetype_, taglevel_, m_ar.size()));
		m_ar.push_back( RequestResultItem( papuga_ResultNodeInputReference, tagname_));
	}
	void addResultNodeResultReference( const Scope& scope_, const char* tagname_, const char* str_, papuga_ResolveType resolvetype_)
	{
		m_resultrefs.push_back( ResultRef( scope_, resolvetype_, str_, m_ar.size()));
		m_ar.push_back( RequestResultItem( papuga_ResultNodeResultReference, tagname_));
	}
	void addResultNodeOpenStructure( const char* tagname_, bool array)
	{
		m_ar.push_back( RequestResultItem( array ? papuga_ResultNodeOpenArray : papuga_ResultNodeOpenStructure, tagname_));
	}
	void addResultNodeCloseStructure( const char* tagname_, bool array)
	{
		m_ar.push_back( RequestResultItem( array ? papuga_ResultNodeCloseArray : papuga_ResultNodeCloseStructure, tagname_));
	}
	void addResultNodeConstant( const char* tagname_, const char* str_)
	{
		m_ar.push_back( RequestResultItem( papuga_ResultNodeConstant, tagname_, str_));
	}
	std::string tostring() const
	{
		std::string rt;
		std::vector<RequestResultItem>::const_iterator ai = m_ar.begin(), ae = m_ar.end();
		for (; ai != ae; ++ai)
		{
			rt.append( papuga_RequestResultNodeTypeName( ai->nodetype));
			rt.push_back( ' ');
			if (ai->tagname)
			{
				rt.append( ai->tagname);
				rt.push_back( ' ');
			}
			if (papuga_ValueVariant_isstring( &ai->value))
			{
				rt.push_back( '[');
				papuga_ErrorCode errcode;
				if (!ValueVariant_append_string( rt, ai->value, errcode))
				{
					throw std::runtime_error( papuga_ErrorCode_tostring( errcode));
				}
				rt.push_back( ']');
			}
			rt.push_back('\n');
		}
		return rt;
	}

	bool pushResult( const char* varname, const Scope& scope_, papuga_ValueVariant& value, papuga_ErrorCode& errcode)
	{
		std::vector<ResultRef>::iterator ri = m_resultrefs.begin(), re = m_resultrefs.end();
		int matches = 0;
		for (; ri != re; ++ri)
		{
			if (scope_.inside( ri->scope) && 0==std::strcmp( varname, ri->varname))
			{
				++matches;
				papuga_ValueVariant valuecopy;
				if (!papuga_Allocator_deepcopy_value( &m_allocator, &valuecopy, &value, false/*movehostobj*/, &errcode)) return false;

				switch (ri->resolvetype)
				{
					case papuga_ResolveTypeOptional:
					case papuga_ResolveTypeRequired:
						if (papuga_ValueVariant_defined( &m_ar[ ri->index].value))
						{
							errcode = papuga_AmbiguousReference;
							return false;
						}
						else
						{
							papuga_init_ValueVariant_value( &m_ar[ ri->index].value, &valuecopy);
						}
						break;
					case papuga_ResolveTypeInherited:
						errcode = papuga_NotImplemented;
						return false;
						break;
					case papuga_ResolveTypeArray:
					case papuga_ResolveTypeArrayNonEmpty:
					{
						papuga_Serialization* ser;

						if (papuga_ValueVariant_defined( &m_ar[ ri->index].value))
						{
							if (m_ar[ ri->index].value.valuetype == papuga_TypeSerialization)
							{
								ser = m_ar[ ri->index].value.value.serialization;
							}
							else
							{
								errcode = papuga_MixedConstruction;
								return false;
							}
						}
						else
						{
							ser = papuga_Allocator_alloc_Serialization( &m_allocator);
							if (!ser)
							{
								errcode = papuga_NoMemError;
								return false;
							}
							papuga_init_ValueVariant_serialization( &m_ar[ ri->index].value, ser);
						}
						if (!papuga_Serialization_pushValue( ser, &valuecopy))
						{
							errcode = papuga_NoMemError;
							return false;
						}
						break;
					}
				}
			}
		}
		return !!matches;
	}

	const char* findUnresolvedResultVariable()
	{
		std::vector<ResultRef>::iterator ri = m_resultrefs.begin(), re = m_resultrefs.end();
		for (; ri != re; ++ri)
		{
			if (ri->resolvetype == papuga_ResolveTypeRequired || ri->resolvetype == papuga_ResolveTypeArrayNonEmpty)
			{
				if (!papuga_ValueVariant_defined( &m_ar[ ri->index].value))
				{
					return ri->varname;
				}
			}
		}
		return NULL;
	}

	std::vector<RequestResultInputElementRef> inputElementRefs()
	{
		std::vector<RequestResultInputElementRef> rt;
		rt.reserve( m_inputrefs.size());
		std::vector<InputRef>::iterator ri = m_inputrefs.begin(), re = m_inputrefs.end();
		for (; ri != re; ++ri)
		{
			rt.push_back( RequestResultInputElementRef( ri->scope, ri->itemid, ri->resolvetype, ri->taglevel, &m_ar[ ri->index].value));
		}
		return rt;
	}
	const std::vector<RequestResultItem>& items() const
	{
		return m_ar;
	}

	void setName( const char* name_)
	{
		m_name = name_;
	}
	void setTarget( const char* schema_, const char* requestmethod_, const char* addressvar_)
	{
		m_schema = schema_;
		m_requestmethod = requestmethod_;
		m_addressvar = addressvar_;
	}
	const char* name() const
	{
		return m_name;
	}
	const char* schema() const
	{
		return m_schema;
	}
	const char* requestmethod() const
	{
		return m_requestmethod;
	}
	const char* addressvar() const
	{
		return m_addressvar;
	}

private:
	RequestResultTemplate( const RequestResultTemplate&){} //non copyable
	void operator=( const RequestResultTemplate&){} //non copyable

private:
	struct ResultRef
	{
		Scope scope;
		papuga_ResolveType resolvetype;
		const char* varname;
		std::size_t index;

		ResultRef( const Scope& scope_, papuga_ResolveType resolvetype_, const char* varname_, std::size_t index_)
			:scope(scope_),resolvetype(resolvetype_),varname(varname_),index(index_){}
		ResultRef( const ResultRef& o)
			:scope(o.scope),resolvetype(o.resolvetype),varname(o.varname),index(o.index){}
	};
	struct InputRef
	{
		Scope scope;
		int itemid;
		papuga_ResolveType resolvetype;
		int taglevel;
		std::size_t index;
	
		InputRef( const Scope& scope_, int itemid_, papuga_ResolveType resolvetype_, int taglevel_, std::size_t index_)
			:scope(scope_),itemid(itemid_),resolvetype(resolvetype_),taglevel(taglevel_),index(index_){}
		InputRef( const InputRef& o)
			:scope(o.scope),itemid(o.itemid),resolvetype(o.resolvetype),taglevel(o.taglevel),index(o.index){}
	};

private:
	const char* m_name;
	const char* m_schema;
	const char* m_requestmethod;
	const char* m_addressvar;
	std::vector<RequestResultItem> m_ar;
	std::vector<ResultRef> m_resultrefs;
	std::vector<InputRef> m_inputrefs;
	papuga_Allocator m_allocator;
	int m_allocatormem[ 2048];
};

}//namespace
#endif

