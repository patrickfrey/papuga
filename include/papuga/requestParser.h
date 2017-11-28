/*
 * Copyright (c) 2017 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/// \brief Structures and functions for parsing papuga XML and JSON requests for further processing
/// \file requestParser.h
#ifndef _PAPUGA_REQUEST_PARSER_H_INCLUDED
#define _PAPUGA_REQUEST_PARSER_H_INCLUDED
#include "papuga/typedefs.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * \brief Request content type enumeration
 */
typedef enum {
	papuga_ContentType_Unknown,						/*< Content type is not known */
	papuga_ContentType_XML,							/*< Content  type is XML*/
	papuga_ContentType_JSON							/*< Content  type is JSON */
} papuga_ContentType;

/*
 * \brief Guess content type
 * \param[in] src pointer to source
 * \param[in] srcsize size of src in bytes
 * \return content type, special content type for none
 */
papuga_ContentType papuga_guess_ContentType( const char* src, size_t srcsize);

/*
 * \brief Guess character set encoding from content
 * \param[in] src pointer to source
 * \param[in] srcsize size of src in bytes
 * \return encoding, binary for unknown
 */
papuga_StringEncoding papuga_guess_StringEncoding( const char* src, size_t srcsize);

/*
 * \brief Request parser element type enumeration
 */
typedef enum {
	papuga_RequestElementType_None,					/*< unknown element type of EOF */
	papuga_RequestElementType_Open,					/*< open tag */
	papuga_RequestElementType_Close,					/*< close tag */
	papuga_RequestElementType_AttributeName,				/*< attribute name */
	papuga_RequestElementType_AttributeValue,				/*< attribute value */
	papuga_RequestElementType_Value					/*< content value */
} papuga_RequestElementType;

/*
 * Document parser interface
 */
typedef struct papuga_RequestParser papuga_RequestParser;

/*
 * \brief Header structure of every papuga_RequestParser implementation
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
} papuga_RequestParserHeader;

/*
 * \brief Create a document parser for an XML document
 * \param[in] src pointer to source
 * \param[in] srcsize size of src in bytes
 * \return The document parser structure
 */
papuga_RequestParser* papuga_create_RequestParser_xml( papuga_StringEncoding encoding, const char* content, size_t size);

/*
 * \brief Create a document parser for a JSON document
 * \param[in] src pointer to source
 * \param[in] srcsize size of src in bytes
 * \return The document parser structure
 */
papuga_RequestParser* papuga_create_RequestParser_json( papuga_StringEncoding encoding, const char* content, size_t size);

/*
 * \brief Destroy a document parser
 * \param[in] self the document parser structure to free
 */
void papuga_destroy_RequestParser( papuga_RequestParser* self);

/*
 * \brief Fetch the next element from the document
 * \param[in] self the document parser structure to fetch the next element from
 * \param[out] value value of the element fetched
 */
papuga_RequestElementType papuga_RequestParser_next( papuga_RequestParser* self, papuga_ValueVariant* value);

/*
 * \brief Get the last error of the document parser
 * \param[in] self document parser to get the last error from
 * \return error code
 */
papuga_ErrorCode papuga_RequestParser_last_error( const papuga_RequestParser* self);

/*
 * \brief Get the position of the last error of the document parser if available
 * \param[in] self document parser to get the last error position from
 * \return error position or -1 if not available
 */
int papuga_RequestParser_last_error_pos( const papuga_RequestParser* self);



#ifdef __cplusplus
}
#endif
#endif

