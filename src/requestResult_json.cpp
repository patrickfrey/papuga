/*
 * Copyright (c) 2017 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/// \brief Expand a request result as JSON
/// \file requestResult_json.cpp
#include "papuga/requestResult.h"
#include "papuga/serialization.h"
#include "papuga/valueVariant.hpp"
#include "papuga/valueVariant.h"
#include "papuga/allocator.h"
#include "papuga/typedefs.h"
#include "papuga/constants.h"
#include "papuga/callResult.h"
#include "papuga/interfaceDescription.h"
#include "papuga/stack.h"
#include <string>
#include <cstring>


void* papuga_RequestResult_tojson( const papuga_RequestResult* self, papuga_StringEncoding enc, size_t* len, papuga_ErrorCode* err)
{
	*err = papuga_NotImplemented;
	*len = 0;
	return NULL;
}

