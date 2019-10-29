/*
 * Copyright (c) 2019 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/// \brief Tree structure to be build from serialization and mapped to serialization
#ifndef _PAPUGA_TREE_NODE_HPP_INCLUDED
#define _PAPUGA_TREE_NODE_HPP_INCLUDED
#include "papuga/typedefs.h"
#include <string>
#include <vector>
#include <map>

namespace papuga {

struct TreeNode;

struct TreeValueAtomic
{
	std::string value;

	TreeValueAtomic( const std::string& value_ = std::string())
		:value(value_){}
	~TreeValueAtomic(){}
};

struct TreeValueDict
{
	typedef std::map<std::string,TreeNode*> Map;
	Map map;

	TreeValueDict() :map(){}
	~TreeValueDict();
};

struct TreeValueArray
{
	typedef std::vector<TreeNode*> List;
	List list;

	TreeValueArray() :list(){}
	~TreeValueArray();
};

struct TreeNode
{
	enum Type {Atomic,Dict,Array};
	Type type;
	union {
		TreeValueAtomic* atomic;
		TreeValueDict* dict;
		TreeValueArray* array;
	} select;

	explicit TreeNode( Type type_);
	~TreeNode();

	static TreeNode* createFromSerialization( papuga_SerializationIter& itr);
	void serialize( papuga_Serialization& ser);

	bool isDict() const						{return type==Dict;}
	bool isAtomic() const						{return type==Atomic;}
	bool isArray() const						{return type==Array;}
	int size() const;

	const TreeNode* firstChild() const;
	TreeNode* firstChild();
	const std::string& firstKey() const;

	const TreeNode* get( const std::string& key) const;
	TreeNode* get( const std::string& key);

	const TreeNode* get( const std::size_t& idx) const;
	TreeNode* get( const std::size_t& idx);

	TreeNode* release( const std::string& key);
	TreeNode* release( const std::size_t& idx);

	const std::string& getValue() const;

	TreeNode* getPath( const std::vector<std::string>& path);
	TreeNode* getOrCreate( const std::string& key, const Type& crtype);
	TreeNode* getOrCreatePath( const std::vector<std::string>& path, const Type& crtype);

	static TreeNode* createDict();
	static TreeNode* createArray();
	static TreeNode* createValue( const std::string& value);

	/// \param[in] node added node pointer (with ownership)
	void set( const std::string& key, TreeNode* node);
	void set( const std::string& key, const std::string& value)	{set( key, createValue(value));}
	void set( std::size_t idx, TreeNode* node);
	void set( std::size_t idx, const std::string& value)		{set( idx, createValue(value));}
	void setValue( const std::string& value);
	void append( TreeNode* node);

	void remove( const std::size_t arrayidx);
	void remove( const std::string& key);
	void removePath( const std::vector<std::string>& path);
};


}//namespace
#endif

