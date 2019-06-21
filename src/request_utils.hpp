/*
 * Copyright (c) 2019 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
#ifndef _PAPUGA_REQUEST_UTILS_HPP_INCLUDED
#define _PAPUGA_REQUEST_UTILS_HPP_INCLUDED
/// \brief Private helper classes and structures to build requests
/// \file requestResult_utils.hpp
#include "papuga/typedefs.h"
#include "papuga/requestResult.h"
#include <string>
#include <vector>
#include <utility>

namespace papuga {

struct Scope
{
	int from;
	int to;

	Scope( int from_, int to_)
		:from(from_),to(to_){}
	Scope( const Scope& o)
		:from(o.from),to(o.to){}

	bool inside( const Scope& o) const
	{
		return (from >= o.from && to <= o.to);
	}
	Scope inner() const
	{
		return Scope( from+1, to);
	}
	bool operator==( const Scope& o) const
	{
		return from == o.from && to == o.to;
	}
	bool operator!=( const Scope& o) const
	{
		return from != o.from || to != o.to;
	}
};

}//namespace
#endif
