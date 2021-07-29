/*
* Copyright (c) 2017 Patrick P. Frey
*
* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/
/*
* @brief Library interface for lua bindings built by papuga
* @file papuga/lib/lua_dev.h
*/
#ifndef _PAPUGA_LUA_DEV_LIB_H_INCLUDED
#define _PAPUGA_LUA_DEV_LIB_H_INCLUDED
#include "papuga/typedefs.h"
#include "papuga/allocator.h"
#include <setjmp.h>
#ifdef __cplusplus
extern "C" {
#endif
#include "lua.h"
#include "lauxlib.h"
#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
extern "C" {
#endif

/*
* @brief Map of class identifiers to class names (for accessing lua metatable by name)
*/
typedef struct papuga_lua_ClassEntryMap
{
	size_t hoarsize;			/*< number of host object names */
	const char** hoar;			/*< pointer to host obect names */
	size_t soarsize;			/*< number of defined structures for return values */
	const char*** soar;			/*< names of structure members of all structures for return values */
} papuga_lua_ClassEntryMap;

typedef struct papuga_lua_UserData papuga_lua_UserData;

/*
* @brief Initialize papuga globals for lua
* @param[in] ls lua state to initialize
* @remark this function has to be called when initializing the lua context
*/
void papuga_lua_init( lua_State* ls);

/*
* @brief Declares a class with its meta data table
* @param[in] ls lua state context
* @param[in] classid identifier of the class
* @param[in] classname unique name of the class
* @param[in] mt method table of the class
*/
void papuga_lua_declare_class( lua_State* ls, int classid, const char* classname, const luaL_Reg* mt);

/*
* @brief Allocate the userdata for a new instance of a class
* @param[in] ls lua state context
* @param[in] classname unique name of the class
* @return pointer to the user data allocated
*/
papuga_lua_UserData* papuga_lua_new_userdata( lua_State* ls, const char* classname);

/*
* @brief Initialize the userdata of a new class instance created
* @param[in] ls lua state context
* @param[in] classid identifier of the class
* @param[in] objref pointer to user data object (class instance)
* @param[in] destructor destructor of 'objref'
* @param[in] cemap map class identifiers to class names
*/
void papuga_lua_init_UserData( papuga_lua_UserData* udata,
				int classid, void* objref, papuga_Deleter destructor,
				const papuga_lua_ClassEntryMap* cemap);

/*
* @brief Invokes a lua error exception on a papuga error
* @param[in] ls lua state context
* @param[in] function the caller context of the error
* @param[in] err error code
*/
void papuga_lua_error( lua_State* ls, const char* function, papuga_ErrorCode err);

/*
* @brief Invokes a lua error exception on a host function execution error
* @param[in] ls lua state context
* @param[in] function the caller context of the error
* @param[in] errormsg error message string
*/
void papuga_lua_error_str( lua_State* ls, const char* function, const char* errormsg);

/*
* @brief Function that fills a structure with the arguments passed in the lua context for papuga
* @param[out] arg argument structure initialized
* @param[in] ls lua state context
* @param[in] argc number of function arguments
* @param[in] classname name of the class of the called method
*/
bool papuga_lua_set_CallArgs( papuga_CallArgs* arg, lua_State *ls, int argc, const char* classname);

/*
* @brief Procedure that transfers the call result of a function into the lua context, freeing the call result structure
* @param[in] ls lua state context
* @param[out] callres result structure initialized
* @param[in] cemap table of application class names
* @param[out] errcode error code
* @return the number of values returned
*/
int papuga_lua_move_CallResult( lua_State *ls, papuga_CallResult* callres,
				const papuga_lua_ClassEntryMap* cemap, papuga_ErrorCode* errcode);

/*
* @brief Push a variant value to the lua stack
* @param[in,out] ls lua state context
* @param[in] value value to push
* @param[in] cemap class description for object meta data
* @return true, if success, false on failure
*/
bool papuga_lua_push_value( lua_State *ls, const papuga_ValueVariant* value, const papuga_lua_ClassEntryMap* cemap, papuga_ErrorCode* errcode);

/*
* @brief Push a variant value to the lua stack in plain (not accepting host objects or iterators or data without all meta data in plain)
* @param[in,out] ls lua state context
* @param[in] value value to push
* @return true, if success, false on failure
*/
bool papuga_lua_push_value_plain( lua_State *ls, const papuga_ValueVariant* value, papuga_ErrorCode* errcode);

/*
* @brief Serialize a deep copy of a value from the lua stack
* @param[in,out] ls lua state context
* @param[out] dest destination for result
* @param[in] idx Lua stack index
* @param[out] errcode error code in case of failure
* @return true, if success, false on failure
*/
bool papuga_lua_serialize( lua_State *ls, papuga_Serialization* dest, int idx, papuga_ErrorCode* errcode);

/*
* @brief Get a deep copy of a value from the lua stack
* @param[in,out] ls lua state context
* @param[out] dest destination for result
* @param[in,out] allocator allocator to use
* @param[in] idx Lua stack index
* @param[out] errcode error code in case of failure
* @return true, if success, false on failure
*/
bool papuga_lua_value( lua_State *ls, papuga_ValueVariant* result, papuga_Allocator* allocator, int li, papuga_ErrorCode* errcode);

#ifdef __cplusplus
}
#endif
#endif

