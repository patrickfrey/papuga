/*
 * Copyright (c) 2019 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/// \brief Tree structure to be build from serialization and mapped to serialization
#include "treeNode.hpp"
#include "private/internationalization.hpp"
#include "papuga/errors.h"
#include "papuga/valueVariant.hpp"
#include "papuga/serialization.h"
#include <memory>

/// \brief Placeholder for later use of gettext
#define _TXT(txt) txt

using namespace papuga;

TreeNode::TreeNode( Type type_)
	:type(type_)
{
	switch (type)
	{
		case Atomic:
			select.atomic = new TreeValueAtomic();
			break;
		case Dict:
			select.dict = new TreeValueDict();
			break;
		case Array:
			select.array = new TreeValueArray();
			break;
	}
}

TreeNode::~TreeNode()
{
	switch (type)
	{
		case Atomic:
			delete select.atomic;
			break;
		case Dict:
			delete select.dict;
			break;
		case Array:
			delete select.array;
			break;
	}
}

TreeValueDict::~TreeValueDict()
{
	Map::iterator mi = map.begin(), me = map.end();
	for (; mi != me; ++mi) if (mi->second) delete mi->second;
}

TreeValueArray::~TreeValueArray()
{
	List::iterator li = list.begin(), le = list.end();
	for (; li != le; ++li) if (*li) delete *li;
}

static void expectClose( papuga_SerializationIter& itr)
{
	if (papuga_SerializationIter_tag( &itr) != papuga_TagClose)
	{
		throw papuga::runtime_error(_TXT("expected close at end of structure"));
	}
	papuga_SerializationIter_skip( &itr);
}

TreeNode* TreeNode::createFromSerialization( papuga_SerializationIter& itr)
{
	std::unique_ptr<TreeNode> nd;
	if (papuga_SerializationIter_tag( &itr) == papuga_TagName)
	{
		nd.reset( new TreeNode( TreeNode::Dict));
		while (papuga_SerializationIter_tag( &itr) == papuga_TagName)
		{
			papuga_ErrorCode errcode = papuga_Ok;
			std::string key = papuga::ValueVariant_tostring( *papuga_SerializationIter_value( &itr), errcode);
			if (errcode != papuga_Ok) throw papuga::runtime_error(_TXT("error in conversion of key to string: %s"), papuga_ErrorCode_tostring( errcode));
			papuga_SerializationIter_skip( &itr);

			if (papuga_SerializationIter_tag( &itr) == papuga_TagValue)
			{
				std::unique_ptr<TreeNode> valuenode( new TreeNode( TreeNode::Atomic));
				valuenode->select.atomic->value = papuga::ValueVariant_tostring( *papuga_SerializationIter_value( &itr), errcode);
				if (errcode != papuga_Ok) throw papuga::runtime_error(_TXT("error in conversion of value to string, key '%s': %s"), key.c_str(), papuga_ErrorCode_tostring( errcode));
				nd->select.dict->map[ key] = valuenode.get();
				valuenode.release();
				papuga_SerializationIter_skip( &itr);
			}
			else if (papuga_SerializationIter_tag( &itr) == papuga_TagOpen)
			{
				papuga_SerializationIter_skip( &itr);
				std::unique_ptr<TreeNode> valuenode( createFromSerialization( itr));
				nd->select.dict->map[ key] = valuenode.get();
				valuenode.release();
				expectClose( itr);
			}
		}
		if (papuga_SerializationIter_tag( &itr) != papuga_TagClose)
		{
			throw std::runtime_error(_TXT("dictionary structure not terminated with close or mixed declaration of array/dictionary"));
		}
		return nd.release();
	}
	else if (papuga_SerializationIter_tag( &itr) == papuga_TagClose)
	{
		return new TreeNode( TreeNode::Dict);
	}
	else
	{
		nd.reset( new TreeNode( TreeNode::Array));
		while (papuga_SerializationIter_tag( &itr) != papuga_TagClose)
		{
			papuga_ErrorCode errcode = papuga_Ok;
			if (papuga_SerializationIter_tag( &itr) == papuga_TagValue)
			{
				std::unique_ptr<TreeNode> valuenode( new TreeNode( TreeNode::Atomic));
				valuenode->select.atomic->value = papuga::ValueVariant_tostring( *papuga_SerializationIter_value( &itr), errcode);
				if (errcode != papuga_Ok) throw papuga::runtime_error(_TXT("error in conversion of array element to string: %s"), papuga_ErrorCode_tostring( errcode));
				nd->select.array->list.push_back( valuenode.get());
				valuenode.release();
				papuga_SerializationIter_skip( &itr);
			}
			else if (papuga_SerializationIter_tag( &itr) == papuga_TagOpen)
			{
				papuga_SerializationIter_skip( &itr);
				std::unique_ptr<TreeNode> valuenode( createFromSerialization( itr));
				nd->select.array->list.push_back( valuenode.get());
				valuenode.release();
				expectClose( itr);
			}
			else
			{
				throw std::runtime_error(_TXT("mixed construction dictionary/array not allowed"));
			}
		}
		return nd.release();
	}
}

static void pushName( papuga_Serialization& ser, const std::string& name)
{
	if (!papuga_Serialization_pushName_string( &ser, name.c_str(), name.size()))
	{
		throw std::bad_alloc();
	}
}

static void pushValue( papuga_Serialization& ser, const std::string& name)
{
	if (!papuga_Serialization_pushValue_string( &ser, name.c_str(), name.size()))
	{
		throw std::bad_alloc();
	}
}

static void pushOpen( papuga_Serialization& ser)
{
	if (!papuga_Serialization_pushOpen( &ser))
	{
		throw std::bad_alloc();
	}
}

static void pushClose( papuga_Serialization& ser)
{
	if (!papuga_Serialization_pushClose( &ser))
	{
		throw std::bad_alloc();
	}
}

void TreeNode::serialize( papuga_Serialization& ser)
{
	switch (type)
	{
		case TreeNode::Atomic:
			pushValue( ser, select.atomic->value);
			break;
		case TreeNode::Dict:
		{
			TreeValueDict::Map::const_iterator mi = select.dict->map.begin(), me = select.dict->map.end();
			for (; mi != me; ++mi)
			{
				pushName( ser, mi->first);
				if (mi->second->type != TreeNode::Atomic) pushOpen( ser);
				mi->second->serialize( ser);
				if (mi->second->type != TreeNode::Atomic) pushClose( ser);
			}
			break;
		}
		case TreeNode::Array:
		{
			TreeValueArray::List::const_iterator li = select.array->list.begin(), le = select.array->list.end();
			for (; li != le; ++li)
			{
				if ((*li)->type != TreeNode::Atomic) pushOpen( ser);
				(*li)->serialize( ser);
				if ((*li)->type != TreeNode::Atomic) pushClose( ser);
			}
			break;
		}
	}
}

const TreeNode* TreeNode::get( const std::string& name) const
{
	if (type != TreeNode::Dict) throw std::runtime_error(_TXT("expected dictionary"));
	TreeValueDict::Map::const_iterator mi = select.dict->map.find( name);
	return (mi != select.dict->map.end()) ? mi->second : NULL;
}

TreeNode* TreeNode::get( const std::string& name)
{
	if (type != TreeNode::Dict) throw std::runtime_error(_TXT("expected dictionary"));
	TreeValueDict::Map::iterator mi = select.dict->map.find( name);
	return (mi != select.dict->map.end()) ? mi->second : NULL;
}

const TreeNode* TreeNode::get( const std::size_t& idx) const
{
	if (type != TreeNode::Array) throw std::runtime_error(_TXT("expected array"));
	return select.array->list[ idx];
}

TreeNode* TreeNode::get( const std::size_t& idx)
{
	if (type != TreeNode::Array) throw std::runtime_error(_TXT("expected array"));
	return select.array->list[ idx];
}

TreeNode* TreeNode::release( const std::string& key)
{
	if (type != TreeNode::Dict) throw std::runtime_error(_TXT("expected dictionary"));
	TreeValueDict::Map::iterator mi = select.dict->map.find( key);
	if (mi == select.dict->map.end()) throw papuga::runtime_error(_TXT("key not found in map '%s'"), key.c_str());
	TreeNode* rt = mi->second;
	select.dict->map.erase( mi);
	return rt;
}

TreeNode* TreeNode::release( const std::size_t& idx)
{
	if (type != TreeNode::Array) throw std::runtime_error(_TXT("expected array"));
	TreeNode* rt = select.array->list[ idx];
	select.array->list.erase( select.array->list.begin() + idx);
	return rt;
}

const std::string& TreeNode::getValue() const
{
	if (type != TreeNode::Array) throw std::runtime_error(_TXT("expected atomic value"));
	return select.atomic->value;
}

TreeNode* TreeNode::createDict()
{
	return new TreeNode( TreeNode::Dict);
}

TreeNode* TreeNode::createArray()
{
	return new TreeNode( TreeNode::Array);
}

TreeNode* TreeNode::createValue( const std::string& value)
{
	std::unique_ptr<TreeNode> rt( new TreeNode( TreeNode::Atomic));
	rt->select.atomic->value = value;
	return rt.release();
}

void TreeNode::set( const std::string& key, TreeNode* node)
{
	std::unique_ptr<TreeNode> nd( node);
	if (type != TreeNode::Dict) throw std::runtime_error(_TXT("TreeNode::set with string key only implemented for map"));
	TreeValueDict::Map::iterator mi = select.dict->map.find( key);
	if (mi != select.dict->map.end()) delete mi->second;
	select.dict->map[ key] = nd.get();
	nd.release();
}

void TreeNode::set( std::size_t idx, TreeNode* node)
{
	std::unique_ptr<TreeNode> nd( node);
	if (type != TreeNode::Array) throw std::runtime_error(_TXT("TreeNode::set with index only implemented for array"));
	delete select.array->list[ idx];
	select.array->list[ idx] = nd.get();
	nd.release();
}

void TreeNode::setValue( const std::string& value)
{
	if (type != TreeNode::Atomic) throw std::runtime_error(_TXT("TreeNode::setValue only implemented for atomic value"));
	select.atomic->value = value;
}

void TreeNode::append( TreeNode* node)
{
	std::unique_ptr<TreeNode> nd( node);
	if (type != TreeNode::Array) throw std::runtime_error(_TXT("TreeNode::append only implemented for array"));
	select.array->list.push_back( nd.get());
	nd.release();
}

void TreeNode::remove( const std::string& key)
{
	if (type != TreeNode::Dict) throw std::runtime_error(_TXT("TreeNode::remove with key only implemented for map"));
	TreeValueDict::Map::iterator mi = select.dict->map.find( key);
	if (mi != select.dict->map.end())
	{
		delete mi->second;
		select.dict->map.erase( mi);
	}
}

void TreeNode::remove( const std::size_t arrayidx)
{
	if (type != TreeNode::Array) throw std::runtime_error(_TXT("TreeNode::remove with index only implemented for array"));
	TreeValueArray::List::iterator li = select.array->list.begin() + arrayidx;
	if (li < select.array->list.end())
	{
		delete *li;
		select.array->list.erase( li);
	}
}

void TreeNode::removePath( const std::vector<std::string>& path)
{
	std::vector<std::string>::const_iterator pi = path.begin(), pe = path.end();
	TreeNode* nd = this;
	TreeNode* chld = NULL;
	for (; pi != pe && nd; ++pi)
	{
		nd = chld;
		chld = nd->get( *pi);
	}
	if (pi == pe && chld)
	{
		nd->remove( path.back());
		if (nd->size() == 0 && path.size() > 0)
		{
			removePath( std::vector<std::string>( path.begin(), path.end()-1));
		}
	}
}

int TreeNode::size() const
{
	switch (type)
	{
		case Atomic:
			break;
		case Dict:
			return select.dict->map.size();
		case Array:
			return select.array->list.size();
	}
	throw std::runtime_error(_TXT("array or dictionary expected for size()"));
}

const TreeNode* TreeNode::firstChild() const
{
	switch (type)
	{
		case Atomic:
			break;
		case Dict:
			return select.dict->map.empty() ? NULL : select.dict->map.begin()->second;
		case Array:
			return select.array->list.empty() ? NULL : *select.array->list.begin();
	}
	throw std::runtime_error(_TXT("array or dictionary expected for first child"));
}

TreeNode* TreeNode::firstChild()
{
	switch (type)
	{
		case Atomic:
			break;
		case Dict:
			return select.dict->map.empty() ? NULL : select.dict->map.begin()->second;
		case Array:
			return select.array->list.empty() ? NULL : *select.array->list.begin();
	}
	throw std::runtime_error(_TXT("array or dictionary expected for first child"));
}

const std::string& TreeNode::firstKey() const
{
	switch (type)
	{
		case Atomic:
			break;
		case Dict:
			if (select.dict->map.empty()) throw std::runtime_error(_TXT("non empty dictionary expected for first key"));
			return select.dict->map.begin()->first;
		case Array:
			throw std::runtime_error(_TXT("dictionary expected for first key"));
	}
	throw std::runtime_error(_TXT("dictionary expected for first key"));
}

TreeNode* TreeNode::getPath( const std::vector<std::string>& path)
{
	std::vector<std::string>::const_iterator pi = path.begin(), pe = path.end();
	TreeNode* nd = this;
	for (; pi != pe && nd; ++pi)
	{
		nd = nd->get( *pi);
	}
	return nd;
}

TreeNode* TreeNode::getOrCreate( const std::string& key, const Type& crtype)
{
	TreeNode* chld = get( key);
	if (chld)
	{
		if (type != crtype) throw std::runtime_error(_TXT("conflicting element types in tree"));
	}
	else
	{
		if (type != Dict) throw std::runtime_error(_TXT("cannot assign key value to other type than map in tree"));
		chld = new TreeNode( crtype);
		set( key, chld);
	}
	return chld;
}

TreeNode* TreeNode::getOrCreatePath( const std::vector<std::string>& path, const Type& crtype)
{
	std::vector<std::string>::const_iterator pi = path.begin(), pe = path.end();
	TreeNode* nd = this;
	for (; pi != pe && nd; ++pi)
	{
		TreeNode* chld = nd->get( *pi);
		if (!chld)
		{
			if (nd->type != Dict) throw std::runtime_error(_TXT("conflicting element types in tree"));
			if (pi+1 == pe)
			{
				chld = new TreeNode( crtype);
			}
			else
			{
				chld = createDict();
			}
			set( *pi, chld);
		}
		nd = chld;
	}
	if (type != crtype) throw std::runtime_error(_TXT("conflicting element types in tree"));
	return nd;
}

