/*
 * Copyright (c) 2021 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/*
* @brief Host object references
* @file hostObject.c
*/
#include "papuga/hostObject.h"
#include <stdlib.h>

papuga_HostObject* papuga_alloc_HostObject( int classid_, void* object_, papuga_Deleter destroy_)
{
	papuga_HostObject* rt = (papuga_HostObject*)malloc( sizeof(papuga_HostObject));
	if (!rt) return 0;
	rt->refcnt = 1;
	rt->classid = classid_; 
	rt->data = object_;
	rt->destroy = destroy_;
	return rt;
}

void papuga_destroy_HostObject( papuga_HostObject* self)
{
	self->refcnt -= 1;
	if (self->refcnt <= 0 && self->destroy && self->data)
	{
		self->destroy( self->data);
		self->destroy = 0;
		free( self);
	}	
}

