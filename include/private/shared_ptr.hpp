/*
 * Copyright (c) 2017 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/// \brief Definition of papuga::shared_ptr
#ifndef _PAPUGA_BASE_SHARED_PTR_HPP_INCLUDED
#define _PAPUGA_BASE_SHARED_PTR_HPP_INCLUDED

#if __cplusplus >= 201103L
#define PAPUGA_USE_STD_SHARED_PTR
#else
#undef PAPUGA_USE_STD_SHARED_PTR
#endif

#if defined PAPUGA_USE_STD_SHARED_PTR
#include <memory>

namespace papuga {

template <class X>
class shared_ptr
	:public std::shared_ptr<X>
{
public:
	typedef void (*Deleter)( X* ptr);
public:
	shared_ptr( X* ptr)
		:std::shared_ptr<X>(ptr){}
	shared_ptr( X* ptr, Deleter deleter)
		:std::shared_ptr<X>(ptr,deleter){}
	shared_ptr( const shared_ptr& o)
		:std::shared_ptr<X>(o){}
	shared_ptr( const std::shared_ptr<X>& o)
		:std::shared_ptr<X>(o){}
	shared_ptr()
		:std::shared_ptr<X>(){}
};
} //namespace

#else //PAPUGA_USE_STD_SHARED_PTR

#include <boost/shared_ptr.hpp>
namespace papuga {

template <class X>
class shared_ptr
	:public boost::shared_ptr<X>
{
public:
	shared_ptr( X* ptr)
		:boost::shared_ptr<X>(ptr){}
	shared_ptr( const shared_ptr& o)
		:boost::shared_ptr<X>(o){}
	shared_ptr()
		:boost::shared_ptr<X>(){}
};
} //namespace

#endif //PAPUGA_USE_STD_SHARED_PTR

#endif

