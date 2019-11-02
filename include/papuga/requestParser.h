/*
 * Copyright (c) 2017 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/* \brief Structures and functions for parsing papuga XML and JSON requests for further processing
 * @file requestParser.h
 */
#ifndef _PAPUGA_REQUEST_PARSER_H_INCLUDED
#define _PAPUGA_REQUEST_PARSER_H_INCLUDED
#include "papuga/typedefs.h"
#include "papuga/request.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * @brief Get the content type name as used for MIME
 * @param[in] type the content type id
 * @return content type MIME name string
 */
const char* papuga_ContentType_mime( papuga_ContentType type);

/*
 * @brief Get the content type as string
 * @param[in] type the content type id
 * @return content type name string
 */
const char* papuga_ContentType_name( papuga_ContentType type);

/*
 * @brief Parse a content type from a string
 * @param[in] name content type as string
 * @return true on success, false on failure
 */
papuga_ContentType papuga_contentTypeFromName( const char* name);

/*
 * @brief Guess content type
 * @param[in] src pointer to source
 * @param[in] srcsize size of src in bytes
 * @return content type, special content type for none
 */
papuga_ContentType papuga_guess_ContentType( const char* src, size_t srcsize);

/*
 * @brief Guess character set encoding from content
 * @param[in] src pointer to source
 * @param[in] srcsize size of src in bytes
 * @return encoding, binary for unknown
 */
papuga_StringEncoding papuga_guess_StringEncoding( const char* src, size_t srcsize);

/*
 * @brief Extract the root element name as UTF-8 string from an XML content
 * @param[in] src pointer to source
 * @param[in] srcsize size of src in bytes
 * @param[in] buf pointer to buffer to use for the string returned
 * @param[in] bufsize allocation size of buf in bytes
 * @return pointer to root element name as UTF-8 stored in buffer passed
 */
const char* papuga_parseRootElement_xml( char* buf, size_t bufsize, const char* src, size_t srcsize);

/*
 * @brief Extract the first root element name as UTF-8 string from a JSON content
 * @note Does not check wheter there is only a single root element defined
 * @param[in] src pointer to source
 * @param[in] srcsize size of src in bytes
 * @param[in] buf pointer to buffer to use for the string returned
 * @param[in] bufsize allocation size of buf in bytes
 * @return pointer to root element name as UTF-8 stored in buffer passed
 */
const char* papuga_parseRootElement_json( char* buf, size_t bufsize, const char* src, size_t srcsize);

/*
 * @brief Request parser element type enumeration
 */
typedef enum {
	papuga_RequestElementType_None,					/*< unknown element type (in case of error set) or EOF (in case of no error) */
	papuga_RequestElementType_Open,					/*< open tag */
	papuga_RequestElementType_Close,				/*< close tag */
	papuga_RequestElementType_AttributeName,			/*< attribute name */
	papuga_RequestElementType_AttributeValue,			/*< attribute value */
	papuga_RequestElementType_Value					/*< content value */
} papuga_RequestElementType;

const char* papuga_requestElementTypeName( papuga_RequestElementType tp);

/*
 * Document parser interface
 */
typedef struct papuga_RequestParser papuga_RequestParser;

/*
 * @brief Header structure of every papuga_RequestParser implementation
*/
typedef struct papuga_RequestParserHeader {
	papuga_ContentType type;						/*< document type of this document parser */
	papuga_ErrorCode errcode;						/*< last error of this document parser */
	int errpos;								/*< last error position of this document parser if available */
	const char* libname;							/*< library name of this document parser */

	void (*destroy)( 
		papuga_RequestParser* self);					/*< methodtable: destructor */
	papuga_RequestElementType (*next)(
		papuga_RequestParser* self, papuga_ValueVariant* value);	/*< methodtable: method fetching the next element */
	int (*position)(
		const papuga_RequestParser* self, char* buf, size_t size);	/*< methodtable: method getting the current position with a location hint as string */
} papuga_RequestParserHeader;

/*
 * @brief Create a document parser for an XML document
 * @param[in] allocator allocator to use
 * @param[in] encoding character set encoding
 * @param[in] content pointer to source
 * @param[in] size size of src in bytes
 * @param[out] error code set in case of failure
 * @return The document parser structure or NULL in case of failure
 */
papuga_RequestParser* papuga_create_RequestParser_xml( papuga_Allocator* allocator, papuga_StringEncoding encoding, const char* content, size_t size, papuga_ErrorCode* errcode);

/*
 * @brief Create a document parser for a JSON document
 * @param[in] allocator allocator to use
 * @param[in] encoding character set encoding
 * @param[in] content pointer to source
 * @param[in] size size of src in bytes
 * @param[out] error code set in case of failure
 * @return The document parser structure or NULL in case of failure
 */
papuga_RequestParser* papuga_create_RequestParser_json( papuga_Allocator* allocator, papuga_StringEncoding encoding, const char* content, size_t size, papuga_ErrorCode* errcode);

/*
 * @brief Create a document parser for a document depending on a document type
 * @param[in] allocator allocator to use
 * @param[in] doctype content type
 * @param[in] encoding character set encoding
 * @param[in] content pointer to source
 * @param[in] size size of src in bytes
 * @param[out] error code set in case of failure
 * @return The document parser structure or NULL in case of failure
 */
papuga_RequestParser* papuga_create_RequestParser( papuga_Allocator* allocator, papuga_ContentType doctype, papuga_StringEncoding encoding, const char* content, size_t size, papuga_ErrorCode* errcode);


/*
 * @brief Destroy a document parser
 * @param[in] self the document parser structure to free
 */
void papuga_destroy_RequestParser( papuga_RequestParser* self);

/*
 * @brief Fetch the next element from the document
 * @param[in] self the document parser structure to fetch the next element from
 * @param[out] value value of the element fetched
 */
papuga_RequestElementType papuga_RequestParser_next( papuga_RequestParser* self, papuga_ValueVariant* value);

/*
 * @brief Get the last error of the document parser
 * @param[in] self document parser to get the last error from
 * @return error code
 */
papuga_ErrorCode papuga_RequestParser_last_error( const papuga_RequestParser* self);

/*
 * @brief Get the current position with a hint about the location as string
 * @param[in] self document parser to get the last error position from
 * @param[out] locbuf where to write the location info to
 * @param[in] locbufsize allocation size of locbuf in bytes
 * @return position or -1 if not available
 */
int papuga_RequestParser_get_position( const papuga_RequestParser* self, char* locbuf, size_t locbufsize);

/*
 * @brief Feed a request iterating with a request parser on some content
 * @param[in,out] parser the iterator on content
 * @param[in,out] request the request to feed
 * @param[out] errcode the error code in case of an error
 * @return true on success, error on failure
 * @note to get the error position in case of an error with a hint on the location call 'papuga_RequestParser_get_position'
 */
bool papuga_RequestParser_feed_request( papuga_RequestParser* parser, papuga_Request* request, papuga_ErrorCode* errcode);

/*
 * @brief Get the request content as it is seen from a request parser in a scope defined by an ordinal as string
 * @note intended to be used for location info in error messages or for logging the content of a request
 * @param[in] allocator allocator to use
 * @param[in] doctype content type
 * @param[in] encoding character set encoding
 * @param[in] docstr pointer to source
 * @param[in] doclen size of docstr in bytes
 * @param[in] scopestart the ordinal position of the start of the scope to print (ordinal position means the count of request parser events, calls of papuga_RequestParser_next)
 * @param[in] maxdepth maximum depth of substructures to print (deeper structures are represented with "...")
 * @param[out] reslength length of the returned string in bytes
 * @param[out] errcode the error code in case of an error
 * @return the content structure or NULL in case of failure
 */
const char* papuga_request_content_tostring( papuga_Allocator* allocator, papuga_ContentType doctype, papuga_StringEncoding encoding, const char* docstr, size_t doclen, int scopestart, int maxdepth, int* reslength, papuga_ErrorCode* errcode);

/*
 * @brief Initialize value variant from a JSON string
 * @param[out] self value variant initialized
 * @param[in] allocator allocator to use
 * @param[in] encoding character set encoding of content to parse
 * @param[in] contentstr pointer to source
 * @param[in] contentlen size of docstr in bytes
 * @param[out] errcode the error code in case of an error
 * @return bool true in case of success, false else
 */
bool papuga_init_ValueVariant_json( papuga_ValueVariant* self, papuga_Allocator* allocator, papuga_StringEncoding encoding, const char* contentstr, size_t contentlen, papuga_ErrorCode* errcode);

#ifdef __cplusplus
}
#endif
#endif

