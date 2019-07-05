/*
 * Copyright (c) 2017 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
#ifndef _PAPUGA_SERIALIZATION_H_INCLUDED
#define _PAPUGA_SERIALIZATION_H_INCLUDED
/*
* @brief Serialization of structures for papuga language bindings
* @file serialization.h
*/
#include "papuga/typedefs.h"
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
* @brief Serialization constructor
* @param[out] self pointer to structure 
* @param[in] allocator_ pointer to allocator to use
*/
#define papuga_init_Serialization(self_,allocator_)		{papuga_Serialization* s = (self_); s->head.next=NULL; s->head.size=0; s->allocator=(allocator_); s->current=&s->head; s->structid=0;}

/*
* @brief Define the structure to be used for serialization (default 0 for dictionary)
* @param[out] self pointer to structure
* @param[in] structid index of the structure interface in the interface description or 0 in case of no structure interface selected
* @note the selection of the interface for substructures is done with the argument of Open
*/
#define papuga_Serialization_set_structid(self_,structid_)	{papuga_Serialization* s = (self_); s->structid=structid_;}

/*
* @brief Get the toplevel structure to be used for serialization (default 0 for dictionary)
* @param[out] self pointer to structure
*/
#define papuga_Serialization_structid(self_)			((self_)->structid)

/*
* @brief Test if the serialization has no content yet
* @param[out] self pointer to structure
*/
#define papuga_Serialization_empty(self_)			((self_)->head.size==0)

/*
* @brief Get the first element tag of a serialization or papuga_TagClose if it is not defined
* @param[out] self pointer to structure
*/
#define papuga_Serialization_first_tag(self_)			((self_)->head.size==0 ? papuga_TagClose : (papuga_Tag)(self_)->head.ar[0].content._tag)

/*
* @brief Get the first element value (value variant) of a serialization or NULL if it is not defined
* @param[out] self pointer to structure
*/
#define papuga_Serialization_first_value(self_)			((self_)->head.size==0 ? NULL : &(self_)->head.ar[0].content)


/*
* @brief Add a node to the serialization
* @param[in,out] self pointer to structure 
* @param[in] node pointer to the added node
* @return true on success, false on memory allocation error
*/
bool papuga_Serialization_push_node( papuga_Serialization* self, papuga_Node* node);

/*
* @brief Add a node to the serialization
* @param[in,out] self pointer to structure 
* @param[in] tag tag of the added node
* @param[in] value pointer to the value of the added node
* @return true on success, false on memory allocation error
*/
bool papuga_Serialization_push( papuga_Serialization* self, papuga_Tag tag, const papuga_ValueVariant* value);

/*
* @brief Add an 'open' element to the serialization
* @param[in,out] self pointer to structure 
* @return true on success, false on memory allocation error
*/
bool papuga_Serialization_pushOpen( papuga_Serialization* self);

/*
* @brief Add an 'open' element to the serialization
* @param[in,out] self pointer to structure 
* @param[in] structid index of the structure interface in the interface description
* @return true on success, false on memory allocation error
*/
bool papuga_Serialization_pushOpen_struct( papuga_Serialization* self, int structid);

/*
* @brief Add a 'close' element to the serialization
* @param[in,out] self pointer to structure 
* @return true on success, false on memory allocation error
*/
bool papuga_Serialization_pushClose( papuga_Serialization* self);

/*
* @brief Add a 'name' element to the serialization
* @param[in,out] self pointer to structure 
* @param[in] value pointer to value of the added node
* @return true on success, false on memory allocation error
*/
bool papuga_Serialization_pushName( papuga_Serialization* self, const papuga_ValueVariant* value);

/*
* @brief Add a 'value' element to the serialization
* @param[in,out] self pointer to structure 
* @param[in] value pointer to value of the added node
* @return true on success, false on memory allocation error
*/
bool papuga_Serialization_pushValue( papuga_Serialization* self, const papuga_ValueVariant* value);

/*
* @brief Add a 'name' element with a NULL value to the serialization
* @param[in,out] self pointer to structure 
* @return true on success, false on memory allocation error
*/
bool papuga_Serialization_pushName_void( papuga_Serialization* self);

/*
* @brief Add a 'name' element as an UTF-8 string with length to the serialization
* @param[in,out] self pointer to structure 
* @param[in] name pointer to name of the added node
* @param[in] namelen length of the name of the added node in bytes
* @return true on success, false on memory allocation error
*/
bool papuga_Serialization_pushName_string( papuga_Serialization* self, const char* name, int namelen);

/*
* @brief Add a 'name' element as an UTF-8 string to the serialization
* @param[in,out] self pointer to structure 
* @param[in] name pointer to name of the added node
* @return true on success, false on memory allocation error
*/
bool papuga_Serialization_pushName_charp( papuga_Serialization* self, const char* name);

/*
* @brief Add a 'name' element as a string in a specified encoding to the serialization
* @param[in,out] self pointer to structure 
* @param[in] enc character set encoding of the name of the added node
* @param[in] name pointer to name of the added node
* @param[in] namelen length of the name of the added node in character units (bytes in UTF-8, 16bit ints for UTF-16, etc.)
* @return true on success, false on memory allocation error
*/
bool papuga_Serialization_pushName_string_enc( papuga_Serialization* self, papuga_StringEncoding enc, const void* name, int namelen);

/*
* @brief Add a 'name' element as a signed integer to the serialization
* @param[in,out] self pointer to structure 
* @param[in] name numeric name value of the added node
* @return true on success, false on memory allocation error
*/
bool papuga_Serialization_pushName_int( papuga_Serialization* self, int64_t name);

/*
* @brief Add a 'name' element as a double precision floating point value to the serialization
* @param[in,out] self pointer to structure 
* @param[in] name numeric name value of the added node
* @return true on success, false on memory allocation error
*/
bool papuga_Serialization_pushName_double( papuga_Serialization* self, double name);

/*
* @brief Add a 'name' element as a boolean value to the serialization
* @param[in,out] self pointer to structure 
* @param[in] name boolean name value of the added node
* @return true on success, false on memory allocation error
*/
bool papuga_Serialization_pushName_bool( papuga_Serialization* self, bool name);

/*
* @brief Add a 'value' element with a NULL value to the serialization
* @param[in,out] self pointer to structure 
* @return true on success, false on memory allocation error
*/
bool papuga_Serialization_pushValue_void( papuga_Serialization* self);

/*
* @brief Add a 'value' element as an UTF-8 string with length to the serialization
* @param[in,out] self pointer to structure 
* @param[in] value pointer to value of the added node
* @param[in] valuelen length of the value of the added node in bytes
* @return true on success, false on memory allocation error
*/
bool papuga_Serialization_pushValue_string( papuga_Serialization* self, const char* value, int valuelen);

/*
* @brief Add a 'value' element as a null terminated UTF-8 string to the serialization
* @param[in,out] self pointer to structure 
* @param[in] value pointer to value of the added node
* @return true on success, false on memory allocation error
*/
bool papuga_Serialization_pushValue_charp( papuga_Serialization* self, const char* value);

/*
* @brief Add a 'value' element as a string in a specified encoding to the serialization
* @param[in,out] self pointer to structure 
* @param[in] enc character set encoding of the value of the added node
* @param[in] value pointer to value of the added node
* @param[in] valuelen length of the value of the added node in character units (bytes in UTF-8, 16bit ints for UTF-16, etc.)
* @return true on success, false on memory allocation error
*/
bool papuga_Serialization_pushValue_string_enc( papuga_Serialization* self, papuga_StringEncoding enc, const void* value, int valuelen);

/*
* @brief Add a 'value' element as a signed integer to the serialization
* @param[in,out] self pointer to structure 
* @param[in] value numeric value of the added node
* @return true on success, false on memory allocation error
*/
bool papuga_Serialization_pushValue_int( papuga_Serialization* self, int64_t value);

/*
* @brief Add a 'value' element as a double precision floating point value to the serialization
* @param[in,out] self pointer to structure 
* @param[in] value numeric value of the added node
* @return true on success, false on memory allocation error
*/
bool papuga_Serialization_pushValue_double( papuga_Serialization* self, double value);

/*
* @brief Add a 'value' element as a host object reference to the serialization
* @param[in,out] self pointer to structure 
* @param[in] value pointer to hostobject of the added node
* @return true on success, false on memory allocation error
*/
bool papuga_Serialization_pushValue_hostobject( papuga_Serialization* self, papuga_HostObject* value);

/*
* @brief Add a 'value' element as a serialization reference to the serialization
* @param[in,out] self pointer to structure
* @param[in] value pointer to serialization of the added node
* @return true on success, false on memory allocation error
*/
bool papuga_Serialization_pushValue_serialization( papuga_Serialization* self, papuga_Serialization* value);

/*
* @brief Add a 'value' element as a boolean value to the serialization
* @param[in,out] self pointer to structure 
* @param[in] value boolean value of the added node
* @return true on success, false on memory allocation error
*/
bool papuga_Serialization_pushValue_bool( papuga_Serialization* self, bool value);

/*
* @brief Add a JSON document as structure without starting/ending open/close to the serialization
* @param[in,out] self pointer to structure
* @param[in] content pointer to content of the JSON document to append
* @param[in] contentlen length of the content of the JSON document in bytes
* @param[in] enc encoding of the content of the JSON document to append
* @param[in] withRoot true if to serialize with single root element, false if the root element is not part of the serialization
* @param[out] errcode error code in case of error
* @return true on success, false on error, see error code returned as out parameter for the error
*/
bool papuga_Serialization_append_json( papuga_Serialization* self, const char* content, size_t contentlen, papuga_StringEncoding enc, bool withRoot, papuga_ErrorCode* errcode);

/*
* @brief Add an XML document as structure without starting/ending open/close to the serialization
* @param[in,out] self pointer to structure
* @param[in] content pointer to content of the JSON document to append
* @param[in] contentlen length of the content of the JSON document in bytes
* @param[in] enc encoding of the content of the JSON document to append
* @param[in] withRoot true if to serialize with single root element, false if the root element is not part of the serialization
* @param[in] ignoreEmptyContent true, accept beautified XML, ignoring content containing only spaces and end of lines, false standard XML behaviour
* @param[out] errcode error code in case of error
* @return true on success, false on error, see error code returned as out parameter for the error
*/
bool papuga_Serialization_append_xml( papuga_Serialization* self, const char* content, size_t contentlen, papuga_StringEncoding enc, bool withRoot, bool ignoreEmptyContent, papuga_ErrorCode* errcode);

/*
* @brief Conversing a tail sequence from an array to an associative array
* @param[in,out] self pointer to structure
* @param[in] seriter iterator pointing to start of the serialization of the array to convert
* @param[in] countfrom start counting of the inserted indices
* @return true on success, false on error
*/
bool papuga_Serialization_convert_array_assoc( papuga_Serialization* self, const papuga_SerializationIter* seriter, unsigned int countfrom, papuga_ErrorCode* errcode);

/*
* @brief Print serialization in readable form as null terminated string, 
* @param[in] self pointer to structure
* @param[in] allocator allocator to use for the copy
* @param[in] linemode true if to print codes line by line, false if to print structure
* @param[in] maxdepth maximum depth of elements to print
* @param[out] errcode error code in case of an error
* @return NULL on memory allocation error, null terminated string with serialization printed, allocated with malloc, to free by the caller, on success
*/
const char* papuga_Serialization_tostring( const papuga_Serialization* self, papuga_Allocator* allocator, bool linemode, int maxdepth, papuga_ErrorCode* errcode);

/*
* @brief Serialization iterator constructor
* @param[out] self pointer to structure 
* @param[in] ser serialization to iterate on
*/
void papuga_init_SerializationIter( papuga_SerializationIter* self, const papuga_Serialization* ser);

/*
* @brief Serialization iterator constructor for empty serialization
* @param[out] self pointer to structure 
*/
#define papuga_init_SerializationIter_empty( self)	{papuga_SerializationIter* s_=self; s_->chunk=0; s_->chunkpos=0; s_->tag=papuga_TagClose; s_->value=NULL;}

/*
* @brief Serialization iterator constructor skipping to last element of serialization
* @param[out] self pointer to structure 
* @param[in] ser serialization to iterate on
*/
void papuga_init_SerializationIter_last( papuga_SerializationIter* self, const papuga_Serialization* ser);

/*
* @brief Serialization iterator copy constructor
* @param[out] self_ pointer to structure 
* @param[in] oth_ serialization iterator to copy
*/
#define papuga_init_SerializationIter_copy(self_,oth_)	{papuga_SerializationIter* s_=self_;const papuga_SerializationIter* o_=oth_; s_->chunk=o_->chunk;s_->tag=o_->tag;s_->chunkpos=o_->chunkpos;s_->value=o_->value;}

/*
* @brief Skip to next element of serialization
* @param[in,out] self pointer to structure 
*/
void papuga_SerializationIter_skip( papuga_SerializationIter* self);

/*
* @brief Skip over the next value or structure
* @note If the current value is an open, then skip to the first element after the end of this open, otherwise just skip the element
* @param[in,out] self pointer to structure 
* @return true of success, false on syntax error
*/
bool papuga_SerializationIter_skip_structure( papuga_SerializationIter* self);

/*
* @brief Test if serialization is at eof
* @remark Has to be checked if we got an unexpected close, meaning an unexpected eof
* @param[in] self pointer to structure 
*/
#define papuga_SerializationIter_eof(self_)		(!(self_)->value)

/*
* @brief Test if two serialization interators are pointing to the same element
* @param[in] self_ pointer to structure 
* @param[in] oth_ pointer to iterator to compare
*/
#define papuga_SerializationIter_isequal(self_,oth_)	((self_)->value == (oth_)->value)

/*
* @brief Read the current tag
* @param[in] self pointer to structure 
*/
#define papuga_SerializationIter_tag(self_)		((papuga_Tag)(self_)->tag)

/*
* @brief Get the follow tag of the current element
* @param[in,out] self pointer to structure 
* @return the tag of the following element
*/
papuga_Tag papuga_SerializationIter_follow_tag( const papuga_SerializationIter* self);

/*
* @brief Read the current value
* @param[in] self pointer to structure 
*/
#define papuga_SerializationIter_value(self_)		((self_)->value)

#ifdef __cplusplus
}
#endif
#endif


