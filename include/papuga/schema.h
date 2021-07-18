/*
 * Copyright (c) 2021 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
#ifndef _PAPUGA_SCHEMA_H_INCLUDED
#define _PAPUGA_SCHEMA_H_INCLUDED
/*
* @brief Schema description and schema parser generator
* @file typedefs.h
*/
#ifdef __cplusplus
extern "C" {
#endif

#include "papuga/typedefs.h"

typedef struct papuga_SchemaMap papuga_SchemaMap;
typedef struct papuga_SchemaList papuga_SchemaList;
typedef struct papuga_Schema papuga_Schema;

typedef struct papuga_SchemaError
{
	papuga_ErrorCode code;
	int line;
	char item[ 1024];
} papuga_SchemaError;

#define papuga_init_SchemaError( err) {papuga_SchemaError* st = err; st->code = papuga_Ok; st->line = 0; st->item[0] = 0;}

typedef struct  papuga_SchemaSource
{
	const char* name;
	const char* source;
	int lines;
} papuga_SchemaSource;

papuga_SchemaList* papuga_create_schemalist( const char* source, papuga_SchemaError* err);
void papuga_destroy_schemalist( papuga_SchemaList* list);

papuga_SchemaSource const* papuga_schemalist_get( const papuga_SchemaList* list, const char* schemaname);

papuga_SchemaMap* papuga_create_schemamap( const char* source, papuga_SchemaError* err);
void papuga_destroy_schemamap( papuga_SchemaMap* map);

papuga_Schema const* papuga_schemamap_get( const papuga_SchemaMap* map, const char* schemaname);

bool papuga_schema_parse(
		papuga_Serialization* dest,
		papuga_Schema const* schema,
		papuga_ContentType doctype,
		papuga_StringEncoding encoding,
		const char* contentstr, size_t contentlen,
		papuga_SchemaError* err);

const char* papuga_print_schema_automaton(
		papuga_Allocator* allocator,
		const char* source,
		const char* schema,
		papuga_SchemaError* err);

#ifdef __cplusplus
}
#endif
#endif
