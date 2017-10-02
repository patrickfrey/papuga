/*
 * Copyright (c) 2017 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
#ifndef _PAPUGA_TYPEDEFS_H_INCLUDED
#define _PAPUGA_TYPEDEFS_H_INCLUDED
/*
* @brief Typedefs of papuga data structures
* @file typedefs.h
*/
#include <stddef.h>

#ifdef _MSC_VER
#error stdint definitions missing for Windows
#else
#include <stdint.h>
#endif
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
* @brief Error codes for conversion functions
*/
typedef enum papuga_ErrorCode
{
	papuga_Ok=0,
	papuga_LogicError=1,
	papuga_NoMemError=2,
	papuga_TypeError=3,
	papuga_EncodingError=4,
	papuga_BufferOverflowError=5,
	papuga_OutOfRangeError=6,
	papuga_NofArgsError=7,
	papuga_MissingSelf=8,
	papuga_InvalidAccess=9,
	papuga_UnexpectedEof=10,
	papuga_NotImplemented=11,
	papuga_ValueUndefined=12
} papuga_ErrorCode;

/*
* @brief Static buffer for error message
*/
typedef struct papuga_ErrorBuffer
{
	char* ptr;
	size_t size;
} papuga_ErrorBuffer;

/*
* @brief Forward declaration
*/
typedef struct papuga_LangString papuga_LangString;
/*
* @brief Forward declaration
*/
typedef struct papuga_HostObject papuga_HostObject;
/*
* @brief Forward declaration
*/
typedef struct papuga_Serialization papuga_Serialization;
/*
* @brief Forward declaration
*/
typedef struct papuga_Iterator papuga_Iterator;
/*
* @brief Forward declaration
*/
typedef struct papuga_CallResult papuga_CallResult;

/*
* @brief Enumeration of value type identifiers
*/
typedef enum papuga_Type {
	papuga_TypeVoid			= 0x00,		/*< NULL Value without type */
	papuga_TypeDouble		= 0x01,		/*< double precision floating point value (C double) */
	papuga_TypeUInt			= 0x02,		/*< unsigned integer value (maximum width 64 bits) */
	papuga_TypeInt			= 0x03,		/*< signed integer value (maximum width 64 bits) */
	papuga_TypeBool			= 0x04,		/*< boolean value */
	papuga_TypeString		= 0x05,		/*< host environment string (null-terminated UTF-8) */
	papuga_TypeLangString		= 0x06,		/*< bindings language string (unicode string with a defined encoding - papuga_StringEncoding) */
	papuga_TypeHostObject		= 0x07,		/*< class object defined in the host environment, part of the interface */
	papuga_TypeSerialization	= 0x08,		/*< serialization of an object constructed in the binding language */
	papuga_TypeIterator		= 0x09		/*< iterator closure */
} papuga_Type;

/*
* @brief Unsigned integer type as represented by papuga
*/
typedef uint64_t papuga_UInt;
/*
* @brief Signed integer type as represented by papuga
*/
typedef int64_t papuga_Int;
/*
* @brief Floating point value type as represented by papuga
*/
typedef double papuga_Float;

/*
* @brief Enumeration of character set encodings used for strings defined in the binding language (papuga_LangString)
*/
typedef enum papuga_StringEncoding {
	papuga_UTF8,		/*< Unicode UTF-8 encoding */
	papuga_UTF16BE,		/*< Unicode UTF-16 big endian encoding */
	papuga_UTF16LE,		/*< Unicode UTF-16 little endian encoding */
	papuga_UTF16,		/*< Unicode UTF-16 machine endianess encoding */
	papuga_UTF32BE,		/*< Unicode chars big endian */
	papuga_UTF32LE,		/*< Unicode chars little endian */
	papuga_UTF32,		/*< Unicode chars in machine endianess encoding */
	papuga_Binary		/*< Binary blob of bytes */
} papuga_StringEncoding;

/*
* @brief Tag identifier of a papuga serialization node
*/
typedef enum papuga_Tag
{
	papuga_TagValue,	/*< Atomic value */
	papuga_TagOpen,		/*< Open a scope */
	papuga_TagClose,	/*< Closes a scope */
	papuga_TagName		/*< The name of the following value (Value) or structure (Open) */
} papuga_Tag;

/*
* @brief Representation of a variadic value type
*/
typedef struct papuga_ValueVariant
{
	uint8_t valuetype;				/*< casts to a papuga_Type */
	uint8_t encoding;				/*< casts to a papuga_StringEncoding */
	uint8_t _tag;					/*< private element tag used by papuga_Node in serialization */
	int32_t length;					/*< length of a string in bytes */
	union {
		double Double;				/*< double precision floating point value */
		uint64_t UInt;				/*< unsigned integer value */
		int64_t Int;				/*< signed integer value */
		bool Bool;				/*< boolean value */
		const char* string;			/*< null terminated UTF-8 string (host string representation) */
		const void* langstring;			/*< string value (not nessesarily null terminated) for other character set encodings (binding language string representation) */
		papuga_HostObject* hostObject;		/*< reference of an object represented in the host environment */
		papuga_Serialization* serialization;	/*< reference of an object serialization */
		papuga_Iterator* iterator;		/*< reference of an iterator closure */
	} value;
} papuga_ValueVariant;

/*
* @brief String defined in the binding language
*/
struct papuga_LangString
{
	papuga_StringEncoding encoding;		/*< specifies the encoding of this langstring type */
	int length;				/*< length of the langstring in items (UTF-8 item length = 1, UTF-16 item length = 2, etc.) */
	void* ptr;				/*< pointer to array of characters of the string */
};

/*
* @brief Destructor function of an object
*/
typedef void (*papuga_Deleter)( void* obj);

/*
* @brief Papuga host object 
*/
struct papuga_HostObject
{
	int classid;				/*< class identifier the object */
	void* data;				/*< pointer to the object */
	papuga_Deleter destroy;			/*< destructor of the host object in case of this structure holding ownership of it */
};

/*
* @brief Get next method of an iterator
*/
typedef bool (*papuga_GetNext)( void* self, papuga_CallResult* result);

/*
* @brief Papuga iterator closure 
*/
struct papuga_Iterator
{
	void* data;				/*< pointer to the object */
	papuga_Deleter destroy;			/*< destructor of the iterated object in case of this structure holding ownership of it */
	papuga_GetNext getNext;			/*< method to fetch the next iteration element */
};

/*
* @brief Enumeration of value type identifiers with destructor to call by allocator on disposal of the object
*/
typedef enum papuga_RefType {
	papuga_RefTypeHostObject,		/*< object of type papuga_HostObject */
	papuga_RefTypeIterator,			/*< object of type papuga_Iterator */
	papuga_RefTypeAllocator			/*< object of type papuga_Allocator */
} papuga_RefType;

/*
* @brief Header for an object that needs a call of a destructor when freed
*/
typedef struct papuga_ReferenceHeader
{
	papuga_RefType type;			/*< type of allocator object with a destructor */
	struct papuga_ReferenceHeader* next;	/*< next of single linked list */
} papuga_ReferenceHeader;

/*
* @brief One memory chunk of an allocator
*/
typedef struct papuga_AllocatorNode
{
	unsigned int allocsize;			/*< allocation size of this block */
	unsigned int arsize;			/*< number of bytes allocated in this block */
	char* ar;				/*< pointer to memory */
	bool allocated;				/*< true if this block has to be freed */
	struct papuga_AllocatorNode* next;	/*< next buffer in linked list of buffers */
} papuga_AllocatorNode;

/*
* @brief Allocator for objects
*/
typedef struct papuga_Allocator
{
	papuga_AllocatorNode root;		/*< root node */
	papuga_ReferenceHeader* reflist;	/*< list of objects object that need a call of a destructor when freed */
} papuga_Allocator;

/*
* @brief One node of a papuga serialization sequence
*/
typedef struct papuga_Node
{
	papuga_ValueVariant content;		/*< value of the serialization node */
} papuga_Node;

/*
* @brief Allocation chunk for serialization node sequences
*/
#define papuga_NodeChunkSize 2
typedef struct papuga_NodeChunk
{
	papuga_Node ar[ papuga_NodeChunkSize];
	struct papuga_NodeChunk* next;
	int size;
} papuga_NodeChunk;

/*
* @brief Papuga serialization structure
*/
struct papuga_Serialization
{
	papuga_NodeChunk head;
	papuga_Allocator* allocator;
	papuga_NodeChunk* current;
};

/*
* @brief Papuga serialization iterator structure
*/
typedef struct papuga_SerializationIter
{
	papuga_NodeChunk const* chunk;
	papuga_Tag tag;
	int chunkpos;
	const papuga_ValueVariant* value;
} papuga_SerializationIter;

/*
* @brief Structure representing the result of an interface method call
*/
struct papuga_CallResult
{
	papuga_ValueVariant value;		/*< result value */
	papuga_Allocator allocator;		/*< allocator for values that had to be copied */
	papuga_ErrorBuffer errorbuf;		/*< static buffer for error messages */
	int allocbuf[ 1024];			/*< static buffer for allocator */
};

/*
* @brief Maximum number of arguments a method call can have
* @note More positional parameters do not make sense, pass a structure with named arguments instead
*/
#define papuga_MAX_NOF_ARGUMENTS 32

/*
* @brief Structure representing the parameters of an interface method call
*/
typedef struct papuga_CallArgs
{
	int erridx;				/*< index of argument (starting with 1), that caused the error or 0 */
	papuga_ErrorCode errcode;		/*< papuga error code */
	void* self;				/*< pointer to host-object of the method called */
	size_t argc;				/*< number of arguments passed to call */
	papuga_Allocator allocator;		/*< allocator used for deep copies */
	papuga_ValueVariant argv[ papuga_MAX_NOF_ARGUMENTS];	/* argument list */
	int allocbuf[ 2048];			/*< static buffer for allocator to start with (to avoid early malloc) */
} papuga_CallArgs;

#ifdef __cplusplus
}
#endif
#endif

