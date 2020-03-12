/*
 * Copyright (c) 2017 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
#ifndef _PAPUGA_STRING_BUFFER_H_INCLUDED
#define _PAPUGA_STRING_BUFFER_H_INCLUDED
/*
* @brief Allocator for memory blocks with ownership returned by papuga language binding functions
* @file allocator.h
*/
#include "papuga/typedefs.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
* @brief Constructor of AllocatorNode
* @param[out] self_ pointer to structure initialized
* @param[in] buf_ pointer to preallocated (local) buffer
* @param[in] bufsize_ allocation size of buf in bytes
*/
#define papuga_init_AllocatorNode(self_,buf_,bufsize_)	{papuga_Allocator* s = self_; s->allocsize=bufsize_;s->arsize=0;s->allocated=!bufsize_;s->ar=(char*)buf_;s->next=0;}

/*
* @brief Constructor of Allocator
* @param[out] self_ pointer to structure initialized
* @param[in] buf_ pointer to preallocated (local) buffer
* @param[in] bufsize_ allocation size of buf in bytes
*/
#define papuga_init_Allocator(self_,buf_,bufsize_)	{papuga_Allocator* s = self_; s->root.allocsize=bufsize_;s->root.arsize=0;s->root.allocated=((const void*)(buf_)==NULL);s->root.ar=(char*)buf_;s->root.next=0;s->reflist=0;}

/*
* @brief Evaluate if an Allocator has any allocations made
* @param[out] self_ pointer to allocator structure
*/
#define papuga_Allocator_empty(self_)			(((papuga_Allocator*)self_)->root.arsize==0 && ((papuga_Allocator*)self_)->reflist==0)

/*
* @brief Destructor of Allocator
* @param[in] self_ pointer to structure destroyed
*/
#define papuga_destroy_Allocator(self_)			{papuga_Allocator* s = self_; if (s->reflist != NULL) papuga_destroy_ReferenceHeader( s->reflist); papuga_destroy_AllocatorNode( &s->root);}

/*
* @brief Destructor of linked list of AllocatorNode
* @param[in] nd pointer to node
*/
void papuga_destroy_AllocatorNode( papuga_AllocatorNode* nd);

/*
* @brief Destructor of linked list of AllocatorNode
* @param[in] nd pointer to node
*/
void papuga_destroy_ReferenceHeader( papuga_ReferenceHeader* ref);

/*
* @brief Allocate a block of memory
* @param[in,out] self pointer to structure 
* @param[in] blocksize size of block to allocate
* @param[in] alignment allocated block alingment in bytes or 0, if the default alignment (sizeof non empty struct) should be used
* @remark currently no allignment bigger than the standard malloc alignment (sizeof non empty struct) is accepted
* @return the pointer to the allocated block or NULL if alignment is invalid or malloc failed
*/
void* papuga_Allocator_alloc( papuga_Allocator* self, size_t blocksize, unsigned int alignment);

/*
* @brief Allocate a block of memory
* @param[in,out] self pointer to structure 
* @param[in] mem pointer to memory to add to the allocators context, to be freed (with malloc free) when the allocator is destroyed
* @return true on success, false on failure
*/
bool papuga_Allocator_add_free_mem( papuga_Allocator* self, void* mem);

/*
* @brief Add an allocator ownership to the context of an allocator, to be freed on destruction of this allocator
* @param[in,out] self pointer to structure 
* @param[in] allocator pointer to allocator to add ownership to this allocators context, to be freed when this allocator (self) is destroyed
* @return true on success, false on memory allocation error
*/
bool papuga_Allocator_add_free_allocator( papuga_Allocator* self, const papuga_Allocator* allocator);

/*
* @brief Shrink the last memory block allocated, making the freed memory for following allocations available
* @param[in,out] self pointer to structure
* @param[in] ptr pointer to allocated block to shrink
* @param[in] oldsize previous size of block
* @param[in] newsize new size of block
* @return true on success, false on failure
*/
bool papuga_Allocator_shrink_last_alloc( papuga_Allocator* self, void* ptr, size_t oldsize, size_t newsize);

/*
* @brief Allocate a string copy
* @param[in,out] self pointer to structure 
* @param[in] str pointer to string to copy
* @param[in] len length of string to copy in bytes without 0 termination (that does not have to be provided)
* @return the pointer to the string copy
*/
char* papuga_Allocator_copy_string( papuga_Allocator* self, const char* str, size_t len);

/*
* @brief Allocate a string copy with encoding
* @param[in,out] self pointer to structure 
* @param[in] str pointer to string to copy
* @param[in] len length of string to copy in units without 0 termination (that does not have to be provided)
* @param[in] enc character set encoding of the string to copy
* @return the pointer to the string copy
*/
char* papuga_Allocator_copy_string_enc( papuga_Allocator* self, const char* str, size_t len, papuga_StringEncoding enc);

/*
* @brief Allocate a string copy
* @param[in,out] self pointer to allocator structure 
* @param[in] str pointer to 0-terminated string to copy
* @return the pointer to the C-string to copy
*/
char* papuga_Allocator_copy_charp( papuga_Allocator* self, const char* str);

/*
* @brief Allocate a host object reference
* @param[out] self pointer to allocator structure
* @param[in] object_ pointer to host object
* @param[in] destroy_ destructor of the host object in case of ownership
* @return the pointer to the allocated host object reference
*/
papuga_HostObject* papuga_Allocator_alloc_HostObject( papuga_Allocator* self, int classid_, void* object_, papuga_Deleter destroy_);

/*
* @brief Allocate a serialization object
* @param[out] self pointer to structure initialized by constructor
* @return the pointer to the allocated serialization object
*/
papuga_Serialization* papuga_Allocator_alloc_Serialization( papuga_Allocator* self);

/*
* @brief Allocate an iterator
* @param[out] self pointer to allocator structure
* @param[in] object_ pointer to iterator context object
* @param[in] destroy_ destructor of the iterated object in case of ownership
* @param[in] getNext_ method of the iterated object to fetch the next element
* @return the pointer to the allocated iterator object
*/
papuga_Iterator* papuga_Allocator_alloc_Iterator( papuga_Allocator* self, void* object_, papuga_Deleter destroy_, papuga_GetNext getNext_);

/*
* @brief Allocate an iterator
* @param[out] self pointer to allocator structure
* @return the pointer to the allocated allocator object
*/
papuga_Allocator* papuga_Allocator_alloc_Allocator( papuga_Allocator* self);

/*
* @brief Explicit destruction of an host object conrolled by the allocator (just calling the destructor, not freeing all the memory)
* @param[in] self pointer to allocator structure
* @param[in] hobj pointer host object to free
*/
void papuga_Allocator_destroy_HostObject( papuga_Allocator* self, papuga_HostObject* hobj);

/*
* @brief Explicit destruction of an iterator object conrolled by the allocator (just calling the destructor, not freeing all the memory)
* @param[in] self pointer to allocator structure
* @param[in] hitr pointer iterator object to free
*/
void papuga_Allocator_destroy_Iterator( papuga_Allocator* self, papuga_Iterator* hitr);

/*
* @brief Explicit destruction of an allocator conrolled by the first allocator (just calling the destructor, not freeing all the memory)
* @param[in] self pointer to allocator structure
* @param[in] al pointer allocator object to free
*/
void papuga_Allocator_destroy_Allocator( papuga_Allocator* self, papuga_Allocator* al);


/*
* @brief Make a deep copy of a variant value in this allocator, with ownership of of unshareable objects kept in source or moved to dest, depending on a parameter flag
* @param[in] self pointer to allocator structure where to allocate the deep copy
* @param[in] dest where to write result of deep copy
* @param[in] orig value to copy
* @param[in] movehostobj parameter flag deciding who of dest or orig keeps the ownership of host object references, true => ownership moved to dest, false owner ship remains in orig
* @param[out] errcode error code in case of error, untouched on success
* @return the pointer to the deep value copy object
* @note iterators cannot be copied as such, they are expanded as serialization
*/
bool papuga_Allocator_deepcopy_value( papuga_Allocator* self, papuga_ValueVariant* dest, papuga_ValueVariant* orig, bool movehostobj, papuga_ErrorCode* errcode);

#ifdef __cplusplus
}
#endif
#endif

