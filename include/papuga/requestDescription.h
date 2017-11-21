/*
* Copyright (c) 2017 Patrick P. Frey
*
* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/
/*
* @brief Web request scheme description
* @file requestDescription.h
*/
#ifndef _PAPUGA_REQUEST_SCHEME_DESCRIPTION_H_INCLUDED
#define _PAPUGA_REQUEST_SCHEME_DESCRIPTION_H_INCLUDED
#include <stdbool.h>

struct papuga_RequestAtom
{
	int id;						/*< id of the element */
	const char* selectexpr;				/*< expression selecting the element */
} papuga_RequestAtom;

typedef struct papuga_RequestElementRef
{
	int id;						/*< id of the element */
	bool single;					/*< true, if the element is expected to appear only once if it exists (not an array) */
	const char* name;				/*< name given to the element */
} papuga_RequestElementRef;

typedef struct papuga_RequestElementGroup
{
	int id;						/*< id of the group */
	const int* subids;				
	const char* selectexpr;				/*< expression selecting the group */
} papuga_RequestElementGroup;



/*
* @brief Structure collecting all calls to perform for a request and how to build the result of the request
*/
typedef struct papuga_RequestDescription
{
	const char* name;				/*< name of the service */
	const char* description;			/*< description of the service */
	const papuga_RequestCallDescription* calls;	/*< {NULL,..} terminated list of calls needed to complete a request of the service */
	const papuga_RequestResultDescription* results;	/*< {NULL,..} terminated list of results needed to build the result of a request of the service */
} papuga_RequestDescription;

/*
* @brief Structure collecting all service descriptions of an application
*/
typedef struct papuga_ServiceApiDescription
{
	const char* name;				/*< name of the service */
	const char* description;			/*< description of the service */
	const papuga_RequestDescription* requests;	/*< {NULL,..} terminated list of possible request types of the service */
} papuga_ServiceApiDescription;

/*
* @brief Structure collecting all service descriptions of an application
*/
typedef struct papuga_ServiceApplicationDescription
{
	const char* name;				/*< name of the project */
	const char* description;			/*< description of the project */
	const papuga_ServiceApiDescription* apis;	/*< {NULL,..} terminated list of service apis */
} papuga_ServiceApplicationDescription;

#endif

