/*
 * Copyright (c) 2017 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
#ifndef _PAPUGA_VALUE_VARIANT_H_INCLUDED
#define _PAPUGA_VALUE_VARIANT_H_INCLUDED
/*
* @brief Representation of a typed value for language bindings
* @file valueVariant.h
*/
#include "papuga/typedefs.h"
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
* @brief Mask for checking a variant value type for being numeric
*/
#define papuga_NumericTypeMask	((1U<<(unsigned int)papuga_TypeInt)|(1U<<(unsigned int)papuga_TypeBool)|(1U<<(unsigned int)papuga_TypeDouble))

/*
* @brief Mask for checking a variant value type for being a string
*/
#define papuga_StringTypeMask	(1U<<(unsigned int)papuga_TypeString)

/*
* @brief Mask for checking a variant value type for being atomic (numeric or a string)
*/
#define papuga_AtomicTypeMask	(((unsigned int)papuga_NumericTypeMask | (unsigned int)papuga_StringTypeMask))

/*
* @brief Variant value initializer as a NULL value
* @param[out] self_ pointer to structure 
*/
#define papuga_init_ValueVariant(self_)				{papuga_ValueVariant* s = self_; s->valuetype = (unsigned char)papuga_TypeVoid; s->encoding=0; s->_tag=0; s->length=0; s->value.string=0;}

/*
* @brief Variant value initializer as a double precision floating point value
* @param[out] self_ pointer to structure 
* @param[in] val_ value to initialize structure with
*/
#define papuga_init_ValueVariant_double(self_,val_)		{papuga_ValueVariant* s = self_; s->valuetype = (unsigned char)papuga_TypeDouble; s->encoding=0; s->_tag=0; s->length=0; s->value.Double=(val_);}

/*
* @brief Variant value initializer as a boolean value
* @param[out] self_ pointer to structure 
* @param[in] val_ value to initialize structure with
*/
#define papuga_init_ValueVariant_bool(self_,val_)		{papuga_ValueVariant* s = self_; s->valuetype = (unsigned char)papuga_TypeBool; s->encoding=0; s->_tag=0; s->length=0; s->value.Bool=!!(val_);}

/*
* @brief Variant value initializer as a signed integer value
* @param[out] self_ pointer to structure 
* @param[in] val_ value to initialize structure with
*/
#define papuga_init_ValueVariant_int(self_,val_)		{papuga_ValueVariant* s = self_; s->valuetype = (unsigned char)papuga_TypeInt; s->encoding=0; s->_tag=0; s->length=0; s->value.Int=(val_);}

/*
* @brief Variant value initializer as c binary blob reference
* @param[out] self_ pointer to structure 
* @param[in] val_ value to initialize structure with
*/
#define papuga_init_ValueVariant_blob(self_,val_,sz_)		{papuga_ValueVariant* s = self_; s->valuetype = (unsigned char)papuga_TypeString; s->encoding=papuga_Binary; s->_tag=0; s->length=(sz_); s->value.string=(const char*)(val_);}

/*
* @brief Variant value initializer as c string (UTF-8) reference
* @param[out] self_ pointer to structure 
* @param[in] val_ value to initialize structure with
*/
#define papuga_init_ValueVariant_charp(self_,val_)		{papuga_ValueVariant* s = self_; const char* o = (const char*)val_; s->valuetype = (unsigned char)papuga_TypeString; s->encoding=papuga_UTF8; s->_tag=0; s->length=strlen(o); s->value.string=o;}

/*
* @brief Variant value initializer as c string (UTF-8) reference with size
* @param[out] self_ pointer to structure 
* @param[in] val_ value to initialize structure with
*/
#define papuga_init_ValueVariant_string(self_,val_,sz_)		{papuga_ValueVariant* s = self_; s->valuetype = (unsigned char)papuga_TypeString; s->encoding=papuga_UTF8; s->_tag=0; s->length=(sz_); s->value.string=(val_);}

/*
* @brief Variant value initializer as unicode string reference with size and encoding
* @param[out] self_ pointer to structure 
* @param[in] enc_ string character set encoding
* @param[in] val_ value to initialize structure with
* @param[in] sz_ size of val_ in bytes
*/
#define papuga_init_ValueVariant_string_enc(self_,enc_,val_,sz_){papuga_ValueVariant* s = self_; s->valuetype = (unsigned char)papuga_TypeString; s->encoding=(enc_); s->_tag=0; s->length=(sz_); s->value.string=(const char*)(val_);}

/*
* @brief Variant value initializer as a reference to a host object
* @param[out] self_ pointer to structure 
* @param[in] hostobj_ hostobject reference to initialize structure with
*/
#define papuga_init_ValueVariant_hostobj(self_,hostobj_)	{papuga_ValueVariant* s = self_; s->valuetype = (unsigned char)papuga_TypeHostObject; s->encoding=0; s->_tag=0; s->length=0; s->value.hostObject=(hostobj_);}

/*
* @brief Variant value initializer as a serialization of an object defined in the language binding
* @param[out] self_ pointer to structure 
* @param[in] ser_ serialization reference to initialize structure with
*/
#define papuga_init_ValueVariant_serialization(self_,ser_)	{papuga_ValueVariant* s = self_; s->valuetype = (unsigned char)papuga_TypeSerialization; s->encoding=0; s->_tag=0; s->length=0; s->value.serialization=(ser_);}

/*
* @brief Variant value initializer as a serialization of an object defined in the language binding
* @param[out] self_ pointer to structure 
* @param[in] itr_ iterator reference to initialize structure with
*/
#define papuga_init_ValueVariant_iterator(self_,itr_)		{papuga_ValueVariant* s = self_; s->valuetype = (unsigned char)papuga_TypeIterator; s->encoding=0; s->_tag=0; s->length=0; s->value.iterator=(itr_);}

/*
* @brief Variant value initializer as a shallow copy of another variant value
* @param[out] self_ pointer to structure 
* @param[in] o_ pointer to value variant to initialize structure with
*/
#define papuga_init_ValueVariant_copy(self_,o_)			{papuga_ValueVariant* s = self_; const papuga_ValueVariant* v = o_; s->valuetype=v->valuetype;s->encoding=v->encoding;s->_tag=v->_tag;s->length=v->length;s->value.string=v->value.string;}

/*
* @brief Test if the variant value is not NULL
* @param[in] self_ pointer to structure 
* @return true, if yes
*/
#define papuga_ValueVariant_defined(self_)			((self_)->valuetype!=papuga_TypeVoid)

/*
* @brief Test if the variant value is numeric
* @param[in] self_ pointer to structure 
* @return true, if yes
*/
#define papuga_ValueVariant_isnumeric(self_)			(0!=((1U << (self_)->valuetype) & papuga_NumericTypeMask))

/*
* @brief Test if the variant value is atomic
* @param[in] self_ pointer to structure 
* @return true, if yes
*/
#define papuga_ValueVariant_isatomic(self_)			(0!=((1U << (self_)->valuetype) & papuga_AtomicTypeMask))

/*
* @brief Test if the variant value is a string
* @param[in] self_ pointer to structure 
* @return true, if yes
*/
#define papuga_ValueVariant_isstring(self_)			(0!=((1U << (self_)->valuetype) & papuga_StringTypeMask))

/*
* @brief Convert a value variant to an UTF-8 string with its length specified (not necessarily null terminated)
* @param[in] self pointer to structure
* @param[in,out] allocator allocator to use for deep copy of string
* @param[out] len length of the string copied
* @param[out] err error code in case of error (untouched if call succeeds)
* @return the pointer to the first character of the result string
*/
const char* papuga_ValueVariant_tostring( const papuga_ValueVariant* self, papuga_Allocator* allocator, size_t* len, papuga_ErrorCode* err);

/*
* @brief Convert a value variant to a unicode string of a specific encoding using a static buffer with limited size
* @param[in] self pointer to structure 
* @param[in] enc encoding of the result string
* @param[out] buf pointer to character buffer to use for deep copies
* @param[in] bufsize allocation size of 'buf' in bytes
* @param[out] len length of the string copied
* @param[out] err error code in case of error (untouched if call succeeds)
* @return the pointer to the result string or NULL in case of an error (conversion error or the result buffer allocation size is too small)
*/
const void* papuga_ValueVariant_tostring_enc( const papuga_ValueVariant* self, papuga_StringEncoding enc, void* buf, size_t bufsize, size_t* len, papuga_ErrorCode* err);

/*
* @brief Convert a value variant to a binary blob
* @param[in] self pointer to structure 
* @param[in,out] allocator allocator to use for deep copy of memory
* @param[out] len length of the result in bytes
* @param[out] err error code in case of error (untouched if call succeeds)
* @return the pointer to the result blob
*/
const void* papuga_ValueVariant_toblob( const papuga_ValueVariant* self, papuga_Allocator* allocator, size_t* len, papuga_ErrorCode* err);

/*
* @brief Convert a value variant to a signed integer
* @param[in] self pointer to structure 
* @param[out] err error code in case of error (untouched if call succeeds)
* @return the result value in case the conversion succeeded
*/
int64_t papuga_ValueVariant_toint( const papuga_ValueVariant* self, papuga_ErrorCode* err);

/*
* @brief Convert a value variant to an unsigned integer with a maximum size of std::numeric_limits<int64_t>::max(), using only 63 bits !
* @param[in] self pointer to structure 
* @param[out] err error code in case of error (untouched if call succeeds)
* @return the result value in case the conversion succeeded
*/
uint64_t papuga_ValueVariant_touint( const papuga_ValueVariant* self, papuga_ErrorCode* err);

/*
* @brief Convert a value variant to a double precision floating point value
* @param[in] self pointer to structure 
* @param[out] err error code in case of error (untouched if call succeeds)
* @return the result value in case the conversion succeeded
*/
double papuga_ValueVariant_todouble( const papuga_ValueVariant* self, papuga_ErrorCode* err);

/*
* @brief Convert a value variant to a boolean value
* @param[in] self pointer to structure 
* @param[out] err error code in case of error (untouched if call succeeds)
* @return the result value in case the conversion succeeded
*/
bool papuga_ValueVariant_tobool( const papuga_ValueVariant* self, papuga_ErrorCode* err);

/*
* @brief Convert a value variant to a numeric value
* @param[in] self pointer to structure 
* @param[out] err error code in case of error (untouched if call succeeds)
* @return pointer to the result value in case the conversion succeeded
*/
papuga_ValueVariant* papuga_ValueVariant_tonumeric( const papuga_ValueVariant* self, papuga_ValueVariant* res, papuga_ErrorCode* err);

/*
* @brief Try to convert a variant value to an ASCII string in a buffer of restricted size
* @param[out] destbuf buffer to use if a copy of the result string is needed to be made
* @param[in] destbufsize allocation size of 'destbuf' in bytes
* @param[in] self pointer to structure 
* @return the pointer to the result string if succeeded, NULL else
*/
const char* papuga_ValueVariant_toascii( char* destbuf, size_t destbufsize, const papuga_ValueVariant* self);

/*
* @brief Get the value of a type enum as string
* @param[in] type the type enum value
* @return the corresponding string
*/
const char* papuga_Type_name( papuga_Type type);

/*
* @brief Print a value variant (its .._tostring value) to a text file
* @param[in,out] out file where to print the output to
* @param[in] val value variant value
* @return true on success, false if papuga_ValueVariant_tostring failed (out of memory)
*/
bool papuga_ValueVariant_print( FILE* out, const papuga_ValueVariant* val);

#ifdef __cplusplus
}
#endif
#endif

