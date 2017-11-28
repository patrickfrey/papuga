/*
 * Copyright (c) 2017 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/// \brief Structures and functions for scanning papuga XML and JSON documents for further processing
/// \file document.h
#ifndef _PAPUGA_DOCUMENT_H_INCLUDED
#define _PAPUGA_DOCUMENT_H_INCLUDED
#include "papuga/typedefs.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * \brief Document type enumeration
 */
typedef enum {
	papuga_DocumentType_Unknown,						/*< document type is not known */
	papuga_DocumentType_XML,						/*< document type is XML*/
	papuga_DocumentType_JSON						/*< document type is JSON */
} papuga_DocumentType;

/*
 * \brief Guess document type from content
 * \param[in] src pointer to source
 * \param[in] srcsize size of src in bytes
 * \return document type, special document type for none
 */
papuga_DocumentType papuga_guess_DocumentType( const char* src, size_t srcsize);

/*
 * \brief Guess character set encoding from content
 * \param[in] src pointer to source
 * \param[in] srcsize size of src in bytes
 * \return encoding, binary for unknown
 */
papuga_StringEncoding papuga_guess_StringEncoding( const char* src, size_t srcsize);

/*
 * \brief Document type enumeration
 */
typedef enum {
	papuga_DocumentElementType_None,					/*< unknown element type of EOF */
	papuga_DocumentElementType_Open,					/*< open tag */
	papuga_DocumentElementType_Close,					/*< close tag */
	papuga_DocumentElementType_AttributeName,				/*< attribute name */
	papuga_DocumentElementType_AttributeValue,				/*< attribute value */
	papuga_DocumentElementType_Value					/*< content value */
} papuga_DocumentElementType;

/*
 * Document parser interface
 */
typedef struct papuga_DocumentParser papuga_DocumentParser;

/*
 * \brief Header structure of every papuga_DocumentParser implementation
*/
typedef struct papuga_DocumentParserHeader {
	papuga_DocumentType type;						/*< document type of this document parser */
	papuga_ErrorCode errcode;						/*< last error of this document parser */
	int errpos;								/*< last error position of this document parser if available */
	const char* libname;							/*< library name of this document parser */

	void (*destroy)( 
		papuga_DocumentParser* self);					/*< methodtable: destructor */
	papuga_DocumentElementType (*next)(
		papuga_DocumentParser* self, papuga_ValueVariant* value);	/*< methodtable: method fetching the next element */
} papuga_DocumentParserHeader;

/*
 * \brief Create a document parser for an XML document
 * \param[in] src pointer to source
 * \param[in] srcsize size of src in bytes
 * \return The document parser structure
 */
papuga_DocumentParser* papuga_create_DocumentParser_xml( papuga_StringEncoding encoding, const char* content, size_t size);

/*
 * \brief Create a document parser for a JSON document
 * \param[in] src pointer to source
 * \param[in] srcsize size of src in bytes
 * \return The document parser structure
 */
papuga_DocumentParser* papuga_create_DocumentParser_json( papuga_StringEncoding encoding, const char* content, size_t size);

/*
 * \brief Destroy a document parser
 * \param[in] self the document parser structure to free
 */
void papuga_destroy_DocumentParser( papuga_DocumentParser* self);

/*
 * \brief Fetch the next element from the document
 * \param[in] self the document parser structure to fetch the next element from
 * \param[out] value value of the element fetched
 */
papuga_DocumentElementType papuga_DocumentParser_next( papuga_DocumentParser* self, papuga_ValueVariant* value);

/*
 * \brief Get the last error of the document parser
 * \param[in] self document parser to get the last error from
 * \return error code
 */
papuga_ErrorCode papuga_DocumentParser_last_error( const papuga_DocumentParser* self);

/*
 * \brief Get the position of the last error of the document parser if available
 * \param[in] self document parser to get the last error position from
 * \return error position or -1 if not available
 */
int papuga_DocumentParser_last_error_pos( const papuga_DocumentParser* self);



#ifdef __cplusplus
}
#endif
#endif

