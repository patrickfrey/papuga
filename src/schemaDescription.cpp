/*
 * Copyright (c) 2019 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/*
* \brief Automaton to describe and build papuga XML and JSON requests
* \file schemaDescription.cpp
*/
#include "papuga/schemaDescription.h"
#include "papuga/allocator.h"
#include "papuga/valueVariant.h"
#include <vector>
#include <utility>
#include <new>
#include <stdexcept>
#include <cstring>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <map>
#include <algorithm>

class ErrorException
{
public:
	explicit ErrorException( papuga_ErrorCode err_) :m_err(err_),m_path(){}
	ErrorException( papuga_ErrorCode err_, const std::string& path_) :m_err(err_),m_path(path_){}

	papuga_ErrorCode err() const	{return m_err;}
	const std::string& path() const	{return m_path;}

private:
	papuga_ErrorCode m_err;
	std::string m_path;
};

static bool isDelimiter( char ch)
{
	static const char* brk = "/@[](){}#=";
	if (ch == '\0') return true;
	if ((unsigned char)ch < 32) return true;
	return 0!=std::strchr(brk,ch);
}

static char const* skipBrackets( char const* src)
{
	char sb = *src++;
	char eb = '\0';
	if (sb == '(') eb = ')';
	else if (sb == '[') eb = ']';
	else if (sb == '{') eb = '}';
	else throw ErrorException( papuga_SyntaxError);

	for (; *src && *src != eb; ++src)
	{
		if (*src == sb) throw ErrorException( papuga_SyntaxError);
	}
	if (*src != eb) throw ErrorException( papuga_SyntaxError);
	return ++src;
}

std::string parseToken( char const*& src)
{
	const char* start = src;
	for (;*src && !isDelimiter(*src); ++src){};
	return std::string( start, src-start);
}

std::string parseString( char const*& src)
{
	std::string rt;
	char eb = *src++;
	for (; *src && *src != eb; ++src)
	{
		if (*src == '\\')
		{
			++src;
			switch (*src)
			{
				case 'n': rt.push_back( '\n'); break;
				case 't': rt.push_back( '\t'); break;
				case 'b': rt.push_back( '\b'); break;
				case 'r': rt.push_back( '\r'); break;
				case '\0': throw ErrorException( papuga_SyntaxError);
				default: rt.push_back( *src); break;
			}
		}
		else
		{
			rt.push_back( *src);
		}
	}
	if (*src != eb) throw ErrorException( papuga_SyntaxError);
	return rt;
}

static char const* skipElement( char const* src)
{
	for (; !isDelimiter( *src); ++src){}
	return src;
}


class TreeNode
{
public:
	enum {NullId=-1};
	enum ElementType {NullType,StructType,ValueType,AttributeType,ReferenceType,UnionType};
	static const char* elementTypeName( ElementType et)
	{
		static const char* ar[] = {"Null","Struct","Value","Attribute","Reference","UnionType",0};
		return ar[ et];
	}
	const char* elementTypeName() const
	{
		return elementTypeName( elementType);
	}

	enum FollowType {FollowNull, FollowImmediate, FollowDeep};
	static const char* followTypeName( FollowType ft)
	{
		static const char* ar[] = {"Null","Immediate","Deep", 0};
		return ar[ ft];
	}
	const char* followTypeName() const
	{
		return followTypeName( followType);
	}

	struct Scope
	{
		int start;
		int end;

		Scope()					:start(-1),end(-1){}
		Scope( int start_, int end_)		:start(start_),end(end_){}
		Scope( const Scope& o)			:start(o.start),end(o.end){}
		Scope& operator=(const Scope& o)	{start=o.start; end=o.end; return *this;}

		bool defined() const			{return start >= 0;}
		bool inside( const Scope& o) const	{return start >= o.start && end <= o.end;}
		bool covers( const Scope& o) const	{return start <= o.start && end >= o.end;}
	};

	struct Related
	{
		int id;
		papuga_ResolveType resolveType;

		Related()						:id(NullId),resolveType(papuga_ResolveTypeRequired){}
		Related( int id_, papuga_ResolveType resolveType_)	:id(id_),resolveType(resolveType_){}
		Related( const Related& o)				:id(o.id),resolveType(o.resolveType){}

		bool defined() const					{return id != NullId;}
	};

public:
	std::string name;
	int id;
	ElementType elementType;
	papuga_Type valueType;
	FollowType followType;
	papuga_ResolveType resolveType;
	Scope scope;
	std::vector<Related> related;
	std::vector<std::string> examples;
	std::vector<TreeNode> chld;

	TreeNode()
		:name(),id(NullId),elementType(NullType),valueType(papuga_TypeVoid),followType(FollowNull),resolveType(papuga_ResolveTypeRequired),scope(),related(),examples(),chld(){}
	TreeNode( const TreeNode& o)
		:name(o.name),id(o.id),elementType(o.elementType),valueType(o.valueType),followType(o.followType),resolveType(o.resolveType),scope(o.scope),related(o.related),examples(o.examples),chld(o.chld){}
	TreeNode& operator = (const TreeNode& o)
		{name=o.name; id=o.id; elementType=o.elementType; valueType=o.valueType; followType=o.followType; resolveType=o.resolveType; scope=o.scope; related=o.related; examples=o.examples; chld=o.chld; return *this;}
#if __cplusplus >= 201103L
	TreeNode( TreeNode&& o)
		:name(std::move(o.name)),id(o.id),elementType(o.elementType),valueType(o.valueType),followType(o.followType),resolveType(o.resolveType),scope(o.scope),related(std::move(o.related)),examples(std::move(o.examples)),chld(std::move(o.chld)){}
	TreeNode& operator = (TreeNode&& o)
		{name=std::move(o.name); id=o.id; elementType=o.elementType; valueType=o.valueType; followType=o.followType; resolveType=o.resolveType; scope=o.scope; related=std::move(o.related); examples=std::move(o.examples); chld=std::move(o.chld); return *this;}
#endif
	TreeNode( const std::string& name_, int id_, ElementType elementType_, papuga_Type valueType_, FollowType followType_, const std::vector<std::string>& examples_=std::vector<std::string>())
		:name(name_),id(id_),elementType(elementType_),valueType(valueType_),followType(followType_),resolveType(papuga_ResolveTypeRequired),scope(),related(),examples(examples_),chld()
	{}
	TreeNode( const std::string& name_, int id_, ElementType elementType_, papuga_Type valueType_, FollowType followType_, const char* examples_)
		:name(name_),id(id_),elementType(elementType_),valueType(valueType_),followType(followType_),resolveType(papuga_ResolveTypeRequired),scope(),examples(),chld()
	{
		addExamples( examples_);
	}
	int assignScope( int start)
	{
		int scopecnt = start;
		scope.start = scopecnt++;
		std::vector<TreeNode>::iterator ci = chld.begin(), ce = chld.end();
		for (; ci != ce; ++ci)
		{
			scopecnt = ci->assignScope( scopecnt);
		}
		return scope.end = scopecnt;
	}

	void addRelated( const Related& related_)
	{
		related.push_back( related_);
	}

	void addExamples( const char* examples_)
	{
		if (examples_)
		{
			std::string examplebuf;
			if (examples_)
			{
				examplebuf.append( examples_);
				char* ei = const_cast<char*>( examplebuf.c_str());
				char* ni = std::strchr( ei, ';');
				for (; ni; ei=ni+1,ni = std::strchr( ei, ';'))
				{
					*ni = '\0';
					examples.push_back( std::string(ei,ni-ei));
				}
				examples.push_back( std::string(ei));
			}
		}
	}

	TreeNode& addChild( const TreeNode& o)
	{
		if (elementType == AttributeType || elementType == ValueType) throw ErrorException( papuga_SyntaxError);
		chld.push_back( o);
		return chld.back();
	}
	std::vector<TreeNode>::iterator findNode( const std::string& name_)
	{
		std::vector<TreeNode>::iterator ci = chld.begin(), ce = chld.end();
		for (; ci != ce; ++ci)
		{
			if (name_ == ci->name) return ci;
		}
		return chld.end();
	}

	void addConditionElements( const char* src)
	{
		while (*src == '[')
		{
			++src;
			for (;;)
			{
				for (;*src && (unsigned char)*src <= 32; ++src){}
				ElementType condElementType = ValueType;
				if (*src == '@') {++src; condElementType = AttributeType;}
				char const* start = src;
				src = skipElement( src);
				if (start == src) throw ErrorException( papuga_SyntaxError);
				std::string condAttrName( start, src-start);
				std::vector<std::string> condExamples;
				for (;*src && (unsigned char)*src <= 32; ++src){}
				if (*src == '=')
				{
					++src;
					for (;*src && (unsigned char)*src <= 32; ++src){}
					if (*src == '\'' || *src == '\"')
					{
						condExamples.push_back( parseString( src));
					}
					else
					{
						condExamples.push_back( parseToken( src));
					}
					for (;*src && (unsigned char)*src <= 32; ++src){}
				}
				std::vector<TreeNode>::iterator ni = findNode( condAttrName);
				if (ni == chld.end())
				{
					chld.push_back( TreeNode( condAttrName, NullId, condElementType, papuga_TypeVoid, TreeNode::FollowImmediate, condExamples));
				}
				else
				{
					if (ni->elementType != condElementType) throw ErrorException( papuga_SyntaxError);
					ni->examples.insert( ni->examples.end(), condExamples.begin(), condExamples.end());
				}
				if (*src == ',')
				{
					++src;
				}
				else
				{
					break;
				}
			}
			if (*src == ']')
			{
				++src;
				for (;*src && (unsigned char)*src <= 32; ++src){}
			}
			else
			{
				throw std::runtime_error( "syntax error");
			}
		}
	}

	void setValueTypeUnique( papuga_Type valueType_)
	{
		if (valueType == papuga_TypeVoid) valueType = valueType_;
		else if (valueType_ != papuga_TypeVoid)
		{
			if (valueType != valueType_) throw ErrorException( papuga_AmbiguousReference);
		}
	}
	void setElementTypeUnique( ElementType elementType_)
	{
		if (elementType == NullType) elementType = elementType_;
		else if (elementType != elementType_) throw ErrorException( papuga_AmbiguousReference);
	}
	void setIdUnique( int id_)
	{
		if (id == NullId) id = id_;
		else if (id != id_) throw ErrorException( papuga_AmbiguousReference);
	}
	void setFollowTypeUnique( FollowType followType_)
	{
		if (followType != followType_)
		{
			switch (followType)
			{
				case FollowNull:
					followType = followType_;
					break;
				case FollowImmediate:
					if (followType_ != FollowImmediate) throw ErrorException( papuga_AmbiguousReference);
					break;
				case FollowDeep:
					if (followType_ != FollowDeep) throw ErrorException( papuga_AmbiguousReference);
					break;
			}
		}
	}

	void transformValueToSimpleContent()
	{
		if (elementType != ValueType) throw ErrorException( papuga_LogicError);
		chld.push_back( TreeNode( ""/*name*/, NullId, elementType, valueType, FollowImmediate, examples));
		elementType = StructType;
		valueType = papuga_TypeVoid;
		examples.clear();
	}
	bool isAttributeOnlyStruct()
	{
		if (elementType != StructType) return false;
		std::vector<TreeNode>::const_iterator ci = chld.begin(), ce = chld.end();
		for (; ci != ce && ci->elementType == AttributeType; ++ci){}
		return ci == ce;
	}

	void addFollow( int id_, const char* expression_, papuga_Type valueType_, const char* examples_, const Related& related_)
	{
		char const* ei = expression_;
		FollowType followType_ = FollowImmediate;
		if (*ei == '[')
		{
			if (elementType == ValueType) transformValueToSimpleContent();
			setElementTypeUnique( StructType);
			addConditionElements( ei);
			ei = skipBrackets( ei);
		}
		if (*ei == '/')
		{
			if (elementType == ValueType) transformValueToSimpleContent();
			setElementTypeUnique( StructType);
			++ei;
			if (*ei == '/')
			{
				followType_ = FollowDeep;
				++ei;
			}
			addElement( id_, ei, valueType_, followType_, examples_, related_);
		}
		else if (*ei == '@')
		{
			if (elementType == ValueType) transformValueToSimpleContent();
			setElementTypeUnique( StructType);
			addElement( id_, ei, valueType_, followType_, examples_, related_);
		}
		else if (*ei == '(')
		{
			if (isAttributeOnlyStruct())
			{
				chld.push_back( TreeNode( ""/*name*/, NullId, ValueType, valueType_, FollowImmediate, examples_));
			}
			else
			{
				setElementTypeUnique( ValueType);
				setValueTypeUnique( valueType_);
				setIdUnique( id_);
				addExamples( examples_);
			}
		}
		else if (*ei == '\0')
		{
			setIdUnique( id_);
			if (elementType == AttributeType)
			{
				setValueTypeUnique( valueType_);
				addExamples( examples_);
			}
			else if (examples_ || valueType_ != papuga_TypeVoid)
			{
				throw ErrorException( papuga_SyntaxError);
			}
			else if (related_.defined())
			{
				setElementTypeUnique( StructType);
				addRelated( related_);
			}
		}
		else
		{
			throw ErrorException( papuga_SyntaxError);
		}
	}

	static std::string trim( char const* str, std::size_t strsize)
	{
		for (; strsize && (unsigned char)*str <= 32; ++str,--strsize){}
		for (; strsize && (unsigned char)str[ strsize-1] <= 32; --strsize){}
		return std::string( str, strsize);
	}

	static std::vector<std::string> getAltNodeList( const std::string& nodeliststr)
	{
		std::vector<std::string> rt;
		char const* ei = nodeliststr.c_str();
		char const* ni = std::strchr( ei, ',');
		for (; ni; ei=ni+1,ni = std::strchr( ei, ','))
		{
			rt.push_back( trim( ei, ni-ei));
			if (rt.back().empty() || rt.back()[0] == '@')
			{
				throw ErrorException( papuga_SyntaxError);
			}
		}
		ni = std::strchr( ei, '\0');
		rt.push_back( trim( ei, ni-ei));
		return rt;
	}

	std::vector<TreeNode>::iterator getOrCreateChildNode( const std::string& nodename)
	{
		std::vector<TreeNode>::iterator ni = findNode( nodename);
		if (ni == chld.end())
		{
			chld.push_back( TreeNode( nodename, NullId, NullType, papuga_TypeVoid, FollowNull));
			ni = chld.end();
			--ni;
		}
		return ni;
	}

	void addElement( int id_, const char* expression_, papuga_Type valueType_, FollowType followType_, const char* examples_, const Related& related_)
	{
		char const* ei = expression_;
		if (*ei == '{')
		{
			char const* start = ei;
			ei = skipBrackets( ei);
			std::vector<std::string> nodelist = getAltNodeList( std::string( start+1, ei-start-2));
			std::vector<std::string>::const_iterator si = nodelist.begin(), se = nodelist.end();
			for (; si != se; ++si)
			{
				std::vector<TreeNode>::iterator ni = getOrCreateChildNode( *si);
				ni->setFollowTypeUnique( followType_);
				ni->addFollow( id_, ei, valueType_, examples_, related_);
			}
		}
		else
		{
			ElementType childElementType = NullType;
			if (*ei == '@')
			{
				childElementType = AttributeType;
				++ei;
			}
			char const* start = ei;
			ei = skipElement( ei);
			
			std::string nodename( start, ei-start);
			std::vector<TreeNode>::iterator ni = getOrCreateChildNode( nodename);

			if (childElementType != AttributeType && *ei == '(')
			{
				childElementType = ValueType;
				if (followType_ == FollowImmediate && valueType_ == papuga_TypeVoid
				&&  ni->elementType == StructType && ni->followType == FollowDeep
				&&  id_ == ni->id)
				{
					addRelated( Related( id_, papuga_ResolveTypeArray));
					return;
				}
			}
			ni->setFollowTypeUnique( followType_);
			if (childElementType != NullType)
			{
				if (childElementType == ValueType && ni->isAttributeOnlyStruct())
				{
					ni = ni->getOrCreateChildNode( ""/*name*/);
					ni->setFollowTypeUnique( TreeNode::FollowImmediate);
				}
				ni->setElementTypeUnique( childElementType);
			}
			ni->addFollow( id_, ei, valueType_, examples_, related_);
		}
	}

	void addSubTree( int id_, const char* expression_, papuga_Type valueType_, const char* examples_, const Related& related_)
	{
		if (isDelimiter( expression_[0]))
		{
			addFollow( id_, expression_, valueType_, examples_, related_);
		}
		else
		{
			addElement( id_, expression_, valueType_, TreeNode::FollowImmediate, examples_, related_);
		}
	}

	void updateResolveType()
	{
		std::vector<TreeNode>::iterator ci = chld.begin(), ce = chld.end();
		for (; ci != ce; ++ci)
		{
			std::vector<TreeNode::Related>::const_iterator ri = related.begin(), re = related.end();
			for (; ri != re && ri->id != ci->id; ++ri)
			if (ri != re)
			{
				ci->resolveType = ri->resolveType;
			}
			ci->updateResolveType();
		}
	}

	bool isSimpleContent() const
	{
		if (elementType != StructType) return false;
		bool hasEmptyContent = false;
		std::vector<TreeNode>::const_iterator ci = chld.begin(), ce = chld.end();
		for (; ci != ce; ++ci)
		{
			if (ci->elementType == AttributeType) continue;
			if (ci->elementType == ValueType && ci->name.empty())
			{
				hasEmptyContent = true;
				continue;
			}
			return false;
		}
		return hasEmptyContent;
	}
	const TreeNode& simpleContentNode() const
	{
		std::vector<TreeNode>::const_iterator ci = chld.begin(), ce = chld.end();
		for (; ci != ce; ++ci)
		{
			if (ci->elementType == AttributeType) continue;
			if (ci->elementType == ValueType && ci->name.empty())
			{
				return *ci;
			}
		}
		throw ErrorException( papuga_LogicError);
	}
};

class PathStack
{
public:
	PathStack() :m_ar(){}

	void push_back( const TreeNode& nd)
	{
		m_ar.push_back( std::string());
		if (!nd.name.empty())
		{
			switch (nd.followType)
			{
				case TreeNode::FollowNull:
					m_ar.back().push_back( '?');
					break;
				case TreeNode::FollowImmediate:
					if (nd.elementType == TreeNode::AttributeType)
					{
						m_ar.back().push_back( '@');
					}
					else
					{
						m_ar.back().push_back( '/');
					}
					break;
				case TreeNode::FollowDeep:
					m_ar.back().append( "//");
					break;
			}
			m_ar.back().append( nd.name);
		}
	}
	void pop_back()
	{
		if (m_ar.empty()) throw ErrorException( papuga_LogicError);
		m_ar.pop_back();
	}
	std::string str() const
	{
		std::string rt;
		std::vector<std::string>::const_iterator ai = m_ar.begin(), ae = m_ar.end();
		for (; ai != ae; ++ai)
		{
			rt.append( *ai);
		}
		return rt;
	}

private:
	std::vector<std::string> m_ar;
};

class SchemaDescription
{
public:
	mutable papuga_ErrorCode lasterr;
	std::string lastexpr;
	TreeNode tree;
	bool done;

	SchemaDescription()
		:lasterr(papuga_Ok),lastexpr(),tree(),done(false){}

	void addElement( int id, const char* expression, papuga_Type valueType, const char* examples)
	{
		tree.addSubTree( id, expression, valueType, examples, TreeNode::Related());
	}

	void addRelation( int structid, const char* expression, int elemid, papuga_ResolveType resolveType)
	{
		tree.addSubTree( structid, expression, papuga_TypeVoid, NULL, TreeNode::Related( elemid, resolveType));
	}

	struct ItemReference
	{
		TreeNode::Scope scope;
		int id;

		ItemReference( const TreeNode::Scope& scope_, int id_)			:scope(scope_),id(id_){}
		ItemReference( const ItemReference& o)					:scope(o.scope),id(o.id){}
		ItemReference()								:scope(),id(TreeNode::NullId){}
	};
	typedef std::map<std::string,ItemReference> NameItemMap;

	struct ItemNameDef
	{
		TreeNode::Scope scope;
		std::string name;

		ItemNameDef( const TreeNode::Scope& scope_, const std::string& name_)	:scope(scope_),name(name_){}
		ItemNameDef( const ItemNameDef& o)					:scope(o.scope),name(o.name){}
		ItemNameDef()								:scope(),name(){}
	};
	typedef std::multimap<int,ItemNameDef> ItemNameMap;

	ItemNameMap invNameItemMap( const NameItemMap& map)
	{
		ItemNameMap rt;
		NameItemMap::const_iterator mi = map.begin(), me = map.end();
		for (; mi != me; ++mi)
		{
			rt.insert( std::pair<int,ItemNameDef>( mi->second.id, ItemNameDef( mi->second.scope, mi->first)));
		}
		return rt;
	}

	void declareDeepNodesAsGlobals( TreeNode& node, NameItemMap& nameItemMap, std::vector<TreeNode>& newRootNodes)
	{
		std::size_t cidx = 0;
		while (cidx < node.chld.size())
		{
			TreeNode& chldnode = node.chld[ cidx];
			if (chldnode.followType == TreeNode::FollowDeep)
			{
				if (nameItemMap.insert(
						std::pair<std::string,ItemReference>(
							chldnode.name,
							ItemReference( chldnode.scope,chldnode.id))).second == false/*not new*/)
				{
					//... duplicate definition
					throw ErrorException( papuga_AmbiguousReference);
				}
				if (&node != &tree)
				{
					TreeNode recelem( chldnode);
					recelem.followType = TreeNode::FollowImmediate;
					declareDeepNodesAsGlobals( recelem, nameItemMap, newRootNodes);
					newRootNodes.push_back( recelem);
					node.chld.erase( node.chld.begin() + cidx);
				}
				else
				{
					//.. is alread top-level node
					chldnode.followType = TreeNode::FollowImmediate;
					declareDeepNodesAsGlobals( chldnode, nameItemMap, newRootNodes);
					++cidx;
				}
			}
			else
			{
				declareDeepNodesAsGlobals( chldnode, nameItemMap, newRootNodes);
				++cidx;
			}
		}
	}

	bool findDeepReference( TreeNode& node, const std::string& name)
	{
		std::vector<TreeNode>::iterator ci = node.chld.begin(), ce = node.chld.end();
		for (; ci != ce; ++ci)
		{
			if (ci->elementType == TreeNode::ReferenceType && ci->name == name)
			{
				return true;
			}
			if (findDeepReference( *ci, name)) return true;
		}
		return false;
	}

	void resolveDeepNodeReferences( TreeNode& node, const ItemNameMap& invnamemap, const TreeNode::Scope& parentScope=TreeNode::Scope())
	{
		typedef ItemNameMap::const_iterator ItemNameItr;
		typedef std::pair<ItemNameItr,ItemNameItr> ItemNameItrRange;

		std::vector<TreeNode>::iterator ci = node.chld.begin(), ce = node.chld.end();
		for (; ci != ce; ++ci)
		{
			if (ci->elementType == TreeNode::ValueType && ci->valueType == papuga_TypeVoid)
			{
				ItemNameItrRange range = invnamemap.equal_range( ci->id);
				ItemNameItr ri = range.first, re = range.second;
				if (ri != re)
				{
					ci->elementType = TreeNode::UnionType;
					for (; ri != re; ++ri)
					{
						if (ri->second.scope.inside( parentScope))
						{
							ci->chld.push_back( TreeNode( ri->second.name, TreeNode::NullId, TreeNode::ReferenceType, papuga_TypeVoid, TreeNode::FollowImmediate));
						}
					}
				}
			}
			else
			{
				resolveDeepNodeReferences( *ci, invnamemap, node.scope);
			}
		}
		if (node.elementType == TreeNode::StructType && parentScope.defined())
		{
			std::vector<TreeNode::Related>::const_iterator mi = node.related.begin(), me = node.related.end();
			for (; mi != me; ++mi)
			{
				ItemNameItrRange range = invnamemap.equal_range( mi->id);
				ItemNameItr ri = range.first, re = range.second;
				for (; ri != re; ++ri)
				{
					if (ri->second.scope.inside( parentScope))
					{
						std::vector<TreeNode>::iterator ni = node.getOrCreateChildNode( ri->second.name);
						ni->setFollowTypeUnique( TreeNode::FollowImmediate);
						ni->resolveType = mi->resolveType;
						if (ni->elementType != TreeNode::NullType) throw ErrorException( papuga_AmbiguousReference);
						ni->elementType = TreeNode::ReferenceType;
					}
				}
			}
		}
	}

	void checkTree( const TreeNode& node, PathStack& stk)
	{
		stk.push_back( node);
		if (node.elementType == TreeNode::NullType) throw ErrorException( papuga_ValueUndefined, stk.str());
		if (node.followType != TreeNode::FollowImmediate) throw ErrorException( papuga_ValueUndefined, stk.str());
		if (node.elementType == TreeNode::ValueType && !node.chld.empty()) throw ErrorException( papuga_LogicError, stk.str());

		std::vector<TreeNode>::const_iterator ci = node.chld.begin(), ce = node.chld.end();
		for (; ci != ce; ++ci)
		{
			checkTree( *ci, stk);
		}
		stk.pop_back();
	}

	void finish()
	{
		tree.elementType = TreeNode::StructType;
		tree.followType = TreeNode::FollowImmediate;
		tree.updateResolveType();
		tree.assignScope( 0);

		NameItemMap nameItemMap;
		std::vector<TreeNode> newRootNodes;

		declareDeepNodesAsGlobals( tree, nameItemMap, newRootNodes);

		tree.chld.insert( tree.chld.end(), newRootNodes.begin(), newRootNodes.end());
		ItemNameMap itemNameMap = invNameItemMap( nameItemMap);

		resolveDeepNodeReferences( tree, itemNameMap);
		PathStack stk;
		checkTree( tree, stk);
		done = true;
	}

	std::string buildExample() const
	{
		return std::string();
	}
};

class XsdSchema
{
public:
	XsdSchema(){}

	static std::string buildText( const TreeNode& tree)
	{
		std::ostringstream out;
		out << "<?xml version=\"1.0\"?>\n";
		out << "<xs:schema xmlns:xs=\"http://www.w3.org/2001/XMLSchema\">\n";

		if (tree.name.empty() && tree.elementType == TreeNode::StructType)
		{
			printXsdSchemaElementAttributes( out, tree);
			printXsdSchemaElementChildNodes( out, tree);
		}
		else
		{
			printXsdSchemaElements( out, tree);
		}
		out << "</xs:schema>\n";
		return out.str();
	}

private:
	static const char* xsdSchemaAtomTypeName( papuga_Type tp)
	{
		switch (tp)
		{
			case papuga_TypeVoid: break;
			case papuga_TypeDouble: return "xs:decimal";
			case papuga_TypeInt: return "xs:integer";
			case papuga_TypeBool: return "xs:boolean";
			case papuga_TypeString: return "xs:string";
			case papuga_TypeHostObject: break;
			case papuga_TypeSerialization: break;
			case papuga_TypeIterator: break;
		}
		return "xs:unknown";
	}

	static std::string xsdElementUseSpecifier( papuga_ResolveType tp)
	{
		switch (tp)
		{
			case papuga_ResolveTypeRequired:
				return " minOccurs=\"1\" maxOccurs=\"1\"";
			case papuga_ResolveTypeOptional:
				return " minOccurs=\"0\" maxOccurs=\"1\"";
			case papuga_ResolveTypeInherited:
				return " minOccurs=\"1\" maxOccurs=\"1\"";
			case papuga_ResolveTypeArray:
				return " minOccurs=\"0\" maxOccurs=\"unbounded\"";
			case papuga_ResolveTypeArrayNonEmpty:
				return " minOccurs=\"1\" maxOccurs=\"unbounded\"";
		}
		return std::string();
	}

	static std::string attributeXsdUseSpecifier( papuga_ResolveType tp)
	{
		switch (tp)
		{
			case papuga_ResolveTypeRequired:
				return " use=\"required\"";
			case papuga_ResolveTypeOptional:
				break;
			case papuga_ResolveTypeInherited:
			case papuga_ResolveTypeArray:
			case papuga_ResolveTypeArrayNonEmpty:
				break;
		}
		return std::string();
	}

	static void printXsdSchemaElementAttributes( std::ostream& out, const TreeNode& node)
	{
		std::vector<TreeNode>::const_iterator ci = node.chld.begin(), ce = node.chld.end();
		for (; ci != ce; ++ci)
		{
			if (ci->elementType == TreeNode::AttributeType) printXsdSchemaElements( out, *ci);
		}
	}

	static void printXsdSchemaElementChildNodes( std::ostream& out, const TreeNode& node)
	{
		std::vector<TreeNode>::const_iterator ci = node.chld.begin(), ce = node.chld.end();
		for (; ci != ce; ++ci)
		{
			if (ci->elementType != TreeNode::AttributeType) printXsdSchemaElements( out, *ci);
		}
	}

	static void printXsdSchemaElements( std::ostream& out, const TreeNode& node)
	{
		if (node.name.empty()) throw ErrorException( papuga_LogicError);
		switch (node.elementType)
		{
			case TreeNode::NullType:
				out << "<xs:element name=\"" << node.name << "\" type=\"" << xsdSchemaAtomTypeName( papuga_TypeString) << xsdElementUseSpecifier( node.resolveType) << " nillable=\"true\"/>\n";
				break;
			case TreeNode::AttributeType:
				out << "<xs:attribute name=\"" << node.name << "\" type=\"" << xsdSchemaAtomTypeName( node.valueType) << attributeXsdUseSpecifier( node.resolveType) << "\"/>\n";
				break;
			case TreeNode::ValueType:
				out << "<xs:element name=\"" << node.name << "\" type=\"" << xsdSchemaAtomTypeName( node.valueType) << xsdElementUseSpecifier( node.resolveType) << "\"/>\n";
				break;
			case TreeNode::StructType:
				out << "<xs:element name=\"" << node.name << "\"" << xsdElementUseSpecifier( node.resolveType) << ">\n";
				if (node.isSimpleContent())
				{
					const TreeNode& cnode = node.simpleContentNode();
					out << "<xs:simpleContent>\n";
					out << "<xs:extension base=\"" << xsdSchemaAtomTypeName( cnode.valueType) << xsdElementUseSpecifier( cnode.resolveType) << "\"\n";
					out << "</xs:extension>\n";
					printXsdSchemaElementAttributes( out, node);
					out << "</xs:simpleContent>\n";
				}
				else
				{
					out << "<xs:complexType>\n";
					out << "<xs:any>\n";
					printXsdSchemaElementChildNodes( out, node);
					printXsdSchemaElementAttributes( out, node);
					out << "</xs:any>\n";
					out << "</xs:complexType>\n";
				}
				out << "</xs:element>\n";
				break;
			case TreeNode::UnionType:
				out << "<xs:element name=\"" << node.name << "\"" << xsdElementUseSpecifier( node.resolveType) << ">\n";
				out << "<xs:complexType>\n";
				out << "<xs:union>\n";
				printXsdSchemaElementChildNodes( out, node);
				printXsdSchemaElementAttributes( out, node);
				out << "</xs:union>\n";
				out << "</xs:complexType>\n";
				out << "</xs:element>\n";
				break;
			case TreeNode::ReferenceType:
				out << "<xs:element ref=\"" << node.name << "\"" << xsdElementUseSpecifier( node.resolveType) << "/>\n";
				break;
		}
	}
};

#if 0
{
  "type": "object",
  "properties": {
    "number":      { "type": "number" },
    "street_name": { "type": "string" },
    "street_type": { "type": "string",
                     "enum": ["Street", "Avenue", "Boulevard"]
                   }
  }
}
#endif

typedef struct papuga_SchemaDescription
{
	SchemaDescription impl;
} papuga_SchemaDescription;


extern "C" papuga_SchemaDescription* papuga_create_SchemaDescription()
{
	try
	{
		papuga_SchemaDescription* rt = new papuga_SchemaDescription();
		return rt;
	}
	catch (const std::bad_alloc&)
	{
		return NULL;
	}
	catch (const std::runtime_error& err)
	{
		return NULL;
	}
}

extern "C" void papuga_destroy_SchemaDescription( papuga_SchemaDescription* self)
{
	delete self;
}

extern "C" papuga_ErrorCode papuga_SchemaDescription_last_error( const papuga_SchemaDescription* self)
{
	return self->impl.lasterr;
}

extern "C" const char* papuga_SchemaDescription_error_expression( const papuga_SchemaDescription* self)
{
	return self->impl.lastexpr.empty() ? NULL : self->impl.lastexpr.c_str();
}

extern "C" bool papuga_SchemaDescription_add_element( papuga_SchemaDescription* self, int id, const char* expression, papuga_Type valueType, const char* examples)
{
	try
	{
		if (self->impl.done)
		{
			self->impl.lasterr = papuga_ExecutionOrder;
			return false;
		}
		self->impl.addElement( id, expression, valueType, examples);
		self->impl.lasterr = papuga_Ok;
		return true;
	}
	catch (const std::bad_alloc&)
	{
		self->impl.lasterr = papuga_NoMemError;
		return false;
	}
	catch (const std::runtime_error& err)
	{
		self->impl.lasterr = papuga_UncaughtException;
		return false;
	}
	catch (const ErrorException& err)
	{
		self->impl.lasterr = err.err();
		self->impl.lastexpr = err.path();
		return false;
	}
}

extern "C" bool papuga_SchemaDescription_add_relation( papuga_SchemaDescription* self, int id, const char* expression, int elemid, papuga_ResolveType resolveType)
{
	try
	{
		if (self->impl.done)
		{
			self->impl.lasterr = papuga_ExecutionOrder;
			return false;
		}
		self->impl.addRelation( id, expression, elemid, resolveType);
		self->impl.lasterr = papuga_Ok;
		return true;
	}
	catch (const std::bad_alloc&)
	{
		self->impl.lasterr = papuga_NoMemError;
		return false;
	}
	catch (const std::runtime_error& err)
	{
		self->impl.lasterr = papuga_UncaughtException;
		return false;
	}
	catch (const ErrorException& err)
	{
		self->impl.lasterr = err.err();
		self->impl.lastexpr = err.path();
		return false;
	}
}

extern "C" bool papuga_SchemaDescription_done( papuga_SchemaDescription* self)
{
	try
	{
		if (self->impl.done) return true;
		self->impl.finish();
		self->impl.lasterr = papuga_Ok;
		return true;
	}
	catch (const std::bad_alloc&)
	{
		self->impl.lasterr = papuga_NoMemError;
		return false;
	}
	catch (const std::runtime_error& err)
	{
		self->impl.lasterr = papuga_UncaughtException;
		return false;
	}
	catch (const ErrorException& err)
	{
		self->impl.lasterr = err.err();
		self->impl.lastexpr = err.path();
		return false;
	}
}

static const void* copyString( papuga_Allocator* allocator, const std::string& str, papuga_StringEncoding enc, size_t* len, papuga_ErrorCode* err)
{
	papuga_ValueVariant val;
	papuga_init_ValueVariant_string( &val, str.c_str(), str.size());
	size_t usize = papuga_StringEncoding_unit_size( enc);
	size_t bufsize = str.size() + usize;
	void* buf = papuga_Allocator_alloc( allocator, bufsize, usize);
	if (!buf)
	{
		*err = papuga_NoMemError;
		return 0;
	}
	const void* rt = papuga_ValueVariant_tostring_enc( &val, enc, buf, bufsize, len, err);
	return rt;
}

extern "C" const void* papuga_SchemaDescription_get_text( const papuga_SchemaDescription* self, papuga_Allocator* allocator, papuga_ContentType doctype, papuga_StringEncoding enc, size_t* len)
{
	try
	{
		if (!self->impl.done)
		{
			self->impl.lasterr = papuga_ExecutionOrder;
			return NULL;
		}
		std::string textUTF8;
		switch (doctype)
		{
			case papuga_ContentType_Unknown:
				self->impl.lasterr = papuga_ValueUndefined;
				return NULL;
			case papuga_ContentType_XML:
				textUTF8 = XsdSchema::buildText( self->impl.tree);
				break;
			case papuga_ContentType_JSON:
				self->impl.lasterr = papuga_NotImplemented;
				return NULL;
		}
		self->impl.lasterr = papuga_Ok;
		return copyString( allocator, textUTF8, enc, len, &self->impl.lasterr);
	}
	catch (const std::bad_alloc&)
	{
		self->impl.lasterr = papuga_NoMemError;
		return NULL;
	}
	catch (const std::runtime_error& err)
	{
		self->impl.lasterr = papuga_UncaughtException;
		return NULL;
	}
	catch (const ErrorException& err)
	{
		self->impl.lasterr = err.err();
		return NULL;
	}
}

extern "C" const void* papuga_SchemaDescription_get_example( const papuga_SchemaDescription* self, papuga_Allocator* allocator, papuga_ContentType doctype, papuga_StringEncoding enc, size_t* len)
{
	try
	{
		if (!self->impl.done)
		{
			self->impl.lasterr = papuga_ExecutionOrder;
			return NULL;
		}
		std::string textUTF8;
		switch (doctype)
		{
			case papuga_ContentType_Unknown:
				self->impl.lasterr = papuga_ValueUndefined;
				return NULL;
			case papuga_ContentType_XML:
				self->impl.lasterr = papuga_NotImplemented;
				return NULL;
			case papuga_ContentType_JSON:
				self->impl.lasterr = papuga_NotImplemented;
				return NULL;
		}
		self->impl.lasterr = papuga_Ok;
		return copyString( allocator, textUTF8, enc, len, &self->impl.lasterr);
	}
	catch (const std::bad_alloc&)
	{
		self->impl.lasterr = papuga_NoMemError;
		return NULL;
	}
	catch (const std::runtime_error& err)
	{
		self->impl.lasterr = papuga_UncaughtException;
		return NULL;
	}
	catch (const ErrorException& err)
	{
		self->impl.lasterr = err.err();
		return NULL;
	}
}


