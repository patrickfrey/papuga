/*
 * Copyright (c) 2017 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
#ifndef _PAPUGA_H_INCLUDED
#define _PAPUGA_H_INCLUDED
/*
* @brief Main C include file of the papuga library
* @file papuga.h
*/ 
/* Implemented in library papuga_devel: */
#include "papuga/typedefs.h"
#include "papuga/constants.h"
#include "papuga/version.h"
#include "papuga/languages.h"
#include "papuga/errors.h"
#include "papuga/callResult.h"
#include "papuga/callArgs.h"
#include "papuga/hostObject.h"
#include "papuga/iterator.h"
#include "papuga/serialization.h"
#include "papuga/allocator.h"
#include "papuga/valueVariant.h"
#include "papuga/stack.h"
#include "papuga/interfaceDescription.h"

/* Implemented in library papuga_request_devel: */
#include "papuga/classdef.h"
#include "papuga/request.h"
#include "papuga/requestParser.h"
#include "papuga/requestHandler.h"

#endif

