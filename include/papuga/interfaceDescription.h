/*
* Copyright (c) 2017 Patrick P. Frey
*
* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/
/*
* @brief Bindings language interface description
* @file interfaceDescription.h
*/
#ifndef _PAPUGA_INTERFACE_DESCRIPTION_H_INCLUDED
#define _PAPUGA_INTERFACE_DESCRIPTION_H_INCLUDED
#include <stdbool.h>

/*
* @brief Type of annotation text
*/
typedef enum papuga_AnnotationType
{
	papuga_AnnotationType_Description,		/*< documentation tag telling the 'what' */
	papuga_AnnotationType_Example,			/*< documentation tag illustrating the 'how' */
	papuga_AnnotationType_Note,			/*< documentation tag explaining the 'why's */
	papuga_AnnotationType_Remark			/*< documentation tag explaining the 'dont's */
} papuga_AnnotationType;

/*
* @brief Descriptive annotation (list of items as {NULL,NULL} terminated list)
*/
typedef struct papuga_Annotation
{
	papuga_AnnotationType type;			/*< documentation type */
	const char* text;				/*< documentation text, content attached with the tag */
} papuga_Annotation;

/*
* @brief Structure describing a parameter of a method/constructor call
*/
typedef struct papuga_ParameterDescription
{
	const char* name;				/*< name of the parameter */
	const papuga_Annotation* doc;			/*< description and examples of the parameter value; {NULL,NULL} terminated list */
	bool mandatory;					/*< true, if the parameter is mandatory, false if it is optional (optional parameters must only appear as last arguments, a function parameter list is always a list of mandatory arguments followed by optional arguments) */
} papuga_ParameterDescription;

/*
* @brief Structure describing the return values of a method call
*/
typedef struct papuga_CallResultDescription
{
	const papuga_Annotation* doc;			/*< description and examples of the return values of a method; {NULL,NULL} terminated list */
} papuga_CallResultDescription;

/*
* @brief Structure describing the constructor of a host object class
*/
typedef struct papuga_ConstructorDescription
{
	const char* funcname;				/*< function name of the method */
	const papuga_Annotation* doc;			/*< description and examples of the constructor as {NULL,NULL} terminated list */
	const papuga_ParameterDescription* parameter;	/*< {NULL,..} terminated list of arguments */
} papuga_ConstructorDescription;

/*
* @brief Structure describing a method of a host object class
*/
typedef struct papuga_MethodDescription
{
	const char* name;				/*< name of the method */
	const char* funcname;				/*< function name of the method */
	const papuga_Annotation* doc;			/*< description and examples of the method; {NULL,NULL} terminated list */
	const papuga_CallResultDescription* result;	/*< return value descriptions or 0, if no return value defined */
	bool nonstatic;					/*< method that requires an instance of its class (self pointer) */
	const papuga_ParameterDescription* parameter;	/*< {NULL,..} terminated list of arguments */
} papuga_MethodDescription;

/*
* @brief Structure describing a host object class
*/
typedef struct papuga_ClassDescription
{
	const char* name;				/*< name of class */
	const papuga_Annotation* doc;			/*< description of the class as {NULL,NULL} terminated list */
	const papuga_ConstructorDescription* constructor;/*< function description of the constructor */
	const char* funcname_destructor;		/*< function name of the destructor */
	const papuga_MethodDescription* methodtable;	/*< {NULL,..} terminated list of methods */
} papuga_ClassDescription;

/*
* @brief Info about the project
*/
typedef struct papuga_AboutDescription
{
	const char* author;				/*< author of the project */
	const char* contributors;			/*< contributors of the project */
	const char* copyright;				/*< copyright of the project */
	const char* license;				/*< license name of the project */
	const char* version;				/*< version (MAJOR.MINOR.PATCH) of the project */
	const char* url;				/*< website of the project */
} papuga_AboutDescription;

/*
* @brief Description of a data member used in serialization of return values
*/
typedef struct papuga_StructMemberDescription
{
	const char* name;
} papuga_StructMemberDescription;

/*
* @brief Description of a data structure used in serialization of return values
*/
typedef struct papuga_StructInterfaceDescription
{
	const papuga_StructMemberDescription* members;
} papuga_StructInterfaceDescription;

/*
* @brief Structure describing the interface
*/
typedef struct papuga_InterfaceDescription
{
	const char* name;				/*< name of the project wrapped by the bindings */
	const char* description;			/*< description of the module */
	const char** includefiles;			/*< null terminated list of files to include */
	const papuga_ClassDescription* classes;		/*< {NULL,..} terminated list of classes */
	const papuga_StructInterfaceDescription* structs;/* {NULL,..} terminated list of structure definitions */
	const papuga_AboutDescription* about;		/*< reference to authors,copyright,license,etc... */
} papuga_InterfaceDescription;

#endif

