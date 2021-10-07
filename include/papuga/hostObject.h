/*
 * Copyright (c) 2017 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
#ifndef _PAPUGA_HOST_OBJECT_H_INCLUDED
#define _PAPUGA_HOST_OBJECT_H_INCLUDED
/*
* @brief Representation of an object in the host environment of the papuga language bindings
* @file hostObject.h
*/
#include "papuga/typedefs.h"

#ifdef __cplusplus
extern "C" {
#endif
/*
* @brief Allocator and constructor of a host object reference (uses malloc/free and not allocator as it is living beyond any allocator scope)
* @param[in] classid_ class identifier of the host object
* @param[in] object_ pointer to host object
* @param[in] destroy_ destructor of the host object in case of ownership
*/
papuga_HostObject* papuga_alloc_HostObject( int classid_, void* object_, papuga_Deleter destroy_);

/*
* @brief Add an owner to a host object reference
* @param[in,out] self pointer to structure
*/
#define papuga_reference_HostObject( self)				{(self)->refcnt += 1;}

/*
* @brief Destructor of a host object reference
* @param[in] self pointer to structure
*/
void papuga_destroy_HostObject( papuga_HostObject* self);

#ifdef __cplusplus
}
#endif
#endif

