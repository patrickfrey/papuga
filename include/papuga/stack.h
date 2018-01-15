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
typedef struct papuga_StackNode
{
	struct papuga_StackNode* prev;		/*< previous node on the stack */
	size_t allocsize;			/*< allocation size of the stack in elements */
	size_t arsize;				/*< number of nodes used */
} papuga_StackNode;

/*
* @brief Papuga stack (helper) structure
*/
typedef struct papuga_Stack
{
	papuga_StackNode* top;			/*< top node of the stack */
	void* buf;				/*< static buffer used first for allocation */
	size_t elemsize;			/*< sizeof element in bytes */
	size_t nodesize;			/*< number of elements per node */
	size_t size;				/*< number of elements on the stack */
} papuga_Stack;

/*
* @brief Constructor
* @param[out] self pointer to structure 
* @param[in] elemsize sizeof stack element in bytes
* @param[in] nodesize number of elements allocated per node
* @param[in] buf static buffer used first for first allocations
* @param[in] bufsize size of buf in bytes
*/
void papuga_init_Stack( papuga_Stack* self, size_t elemsize, size_t nodesize, void* buf, size_t bufsize);

/*
* @brief Destructor
* @param[in] self pointer to structure 
*/
void papuga_destroy_Stack( papuga_Stack* self);

/*
* @brief Get the size of the stack in number of elements inserted
* @param[in] self pointer to structure 
*/
#define papuga_Stack_size( self)	(self)->size

/*
* @brief Test if the stack has no elements inserted
* @param[in] self pointer to structure 
*/
#define papuga_Stack_empty( self)	(!(self)->size)

/*
* @brief Push an element on the stack (memcpy)
* @param[in] self pointer to structure 
* @return the memory of the new element pushed to be initialized by the caller 
*/
void* papuga_Stack_push( papuga_Stack* self);

/*
* @brief Pop an element from the stack 
* @param[in] self pointer to structure 
* @return a pointer to the top element popped or NULL if stack is empty
* @remark the element returned is only valid till the next stack operation
*/
void* papuga_Stack_pop( papuga_Stack* self);

/*
* @brief Get a reference to the top element on the stack
* @param[in] self pointer to structure
* @return a pointer to the top element or NULL if stack is empty
* @remark the element returned is only valid till the next stack operation
*/
void* papuga_Stack_top( const papuga_Stack* self);

/*
* @brief Get a reference to the top n elements
* @param[in] self pointer to structure
* @param[out] buf buffer filled with pointers to the top N elements
* @param[in] N number of elements to get the reference of
* @return true if there are N elements returned, false if the stack contains less elements
*/
bool papuga_Stack_top_n( const papuga_Stack* self, void** buf, size_t N);

#ifdef __cplusplus
}
#endif
#endif
