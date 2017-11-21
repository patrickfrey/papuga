/*
 * Copyright (c) 2017 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/*
* @brief Representation of the arguments of a call to papuga language bindings
* @file callArgs.c
*/
#include "papuga/callArgs.h"
#include "papuga/valueVariant.h"
#include "papuga/serialization.h"
#include "papuga/hostObject.h"
#include "papuga/iterator.h"
#include "papuga/allocator.h"
#include "papuga/errors.h"
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

void papuga_init_CallArgs( papuga_CallArgs* self, char* membuf, size_t membufsize)
{
	self->erridx = -1;
	self->errcode = 0;
	self->self = 0;
	self->argc = 0;
	papuga_init_Allocator( &self->allocator, membuf, membufsize);
}

void papuga_destroy_CallArgs( papuga_CallArgs* self)
{
	papuga_destroy_Allocator( &self->allocator);
}

