/*
 * Copyright (c) 2017 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
#ifndef _PAPUGA_STACK_H_INCLUDED
#define _PAPUGA_STACK_H_INCLUDED
/*
* @brief Stack implementation for papuga
* @file stack.h
*/
#include "papuga/typedefs.h"
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
* @brief Papuga stack (helper) structure
*/
typedef struct papuga_Stack
{
	size_t allocsize;			/*< allocation size of the stack in elements */
	size_t arsize;				/*< number of nodes used */
	size_t elemsize;			/*< sizeof element in bytes */
	void* ar;				/*< array of stack elements */
	bool allocated;				/*< wheter the current buffer is static or allocated */
} papuga_Stack;

/*
* @brief Constructor
* @param[out] self pointer to structure 
*/
void papuga_init_Stack( papuga_Stack* self, size_t elemsize, void* buf, size_t bufsize);

/*
* @brief Destructor
* @param[in] self pointer to structure 
*/
void papuga_destroy_Stack( papuga_Stack* self);

/*
* @brief Push an element on the stack (memcpy)
* @param[in] self pointer to structure 
*/
bool papuga_Stack_push( papuga_Stack* self, void* elem);

/*
* @brief Pop an element from the stack 
* @param[in] self pointer to structure 
* @return a pointer to the top element popped or NULL if stack is empty
* @remark the element returned is not guaranteed to stay valid with the further use of the stack. You have to copy the element, if you want to keep it.
*/
void* papuga_Stack_pop( papuga_Stack* self);

/*
* @brief Get a reference to the top element on the stack
* @param[in] self pointer to structure
* @return a pointer to the top element or NULL if stack is empty
* @remark the element returned is not guaranteed to stay valid with the further use of the stack. You have to copy the element, if you want to keep it.
*/
void* papuga_Stack_top( const papuga_Stack* self);

/*
* @brief Get a reference to an element (random access)
* @param[in] self pointer to structure
* @param[in] idx index of element starting with 0
* @return a pointer to the element or NULL if the access is out of bounds
* @remark the element returned is not guaranteed to stay valid with the further use of the stack. You have to copy the element, if you want to keep it.
*/
void* papuga_Stack_element( const papuga_Stack* self, size_t idx);

#ifdef __cplusplus
}
#endif
#endif
