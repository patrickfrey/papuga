/*
* Copyright (c) 2017 Patrick P. Frey
*
* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/
/*
 * \brief Library implementation for lua bindings built by papuga
 * \file papuga/lib/lua_dev.h
 */
#include "papuga/lib/lua_dev.h"
#include "papuga.h"
#include "private/dll_tags.h"
#include <stddef.h>
#include <math.h>
#include <float.h>
#include <stdio.h>
#include <string.h>
#include <stdio.h>

#define MAX_DOUBLE_INT            ((int64_t)1<<53)
#define MIN_DOUBLE_INT           -((int64_t)1<<53)
#define IS_CONVERTIBLE_TOINT( x)  ((x-floor(x) <= 2*DBL_EPSILON) && x < MAX_DOUBLE_INT && x > MIN_DOUBLE_INT)
#define NUM_EPSILON               (2*DBL_EPSILON)

#define PAPUGA_DEEP_TYPE_CHECKING

#undef PAPUGA_LOWLEVEL_DEBUG
#ifdef PAPUGA_LOWLEVEL_DEBUG
void STACKTRACE( lua_State* ls, const char* where)
{
	int ii;
	int top = lua_gettop( ls);

	fprintf( stderr, "CALLING %s STACK %d: ", where, top);

	for (ii = 1; ii <= top; ii++)
	{
		if (ii>1) fprintf( stderr, ", ");
		int t = lua_type( ls, ii);
		switch (t) {
			case LUA_TNIL:
				fprintf( stderr, "NIL");
				break;
			case LUA_TSTRING:
			{
				char strbuf[ 64];
				size_t len = snprintf( strbuf, sizeof(strbuf), "'%s'", lua_tostring( ls, ii));
				if (len >= sizeof(strbuf))
				{
					memcpy( strbuf + sizeof(strbuf) - 5, "...'", 4);
					strbuf[ sizeof(strbuf)-1] = 0;
				}
				fprintf( stderr, "'%s'", strbuf);
				break;
			}
			case LUA_TBOOLEAN:
				fprintf( stderr, "%s",lua_toboolean( ls, ii) ? "true" : "false");
				break;
			case LUA_TNUMBER:
				fprintf( stderr, "%f", lua_tonumber( ls, ii));
				break;
			default:
				fprintf( stderr, "<%s>", lua_typename( ls, t));
				break;
		}
	}
	fprintf( stderr, "\n");
}
#else
#define STACKTRACE( ls, where)
#endif

static const char* get_classname( const papuga_lua_ClassNameMap* classnamemap, unsigned int classid)
{
	--classid;
	return (classid > classnamemap->size) ? NULL : classnamemap->ar[ classid];
}

struct papuga_lua_UserData
{
	int classid;
	int checksum;
	void* objectref;
	papuga_Deleter destructor;
	const papuga_lua_ClassNameMap* classnamemap;
};

#define KNUTH_HASH 2654435761U
static int calcCheckSum( const papuga_lua_UserData* udata)
{
	return (((udata->classid ^ (uintptr_t)udata->objectref) * KNUTH_HASH) ^ (uintptr_t)udata->destructor ^ ((uintptr_t)udata->classnamemap << 7));
}

static int papuga_lua_destroy_UserData( lua_State* ls)
{
	papuga_lua_UserData* udata = (papuga_lua_UserData*)lua_touserdata( ls, 1);
	if (calcCheckSum(udata) != udata->checksum)
	{
		papuga_lua_error( ls, "destructor", papuga_InvalidAccess);
	}
	++udata->checksum;
	if (udata->destructor) udata->destructor( udata->objectref);
	return 0;
}

static const papuga_lua_UserData* get_UserData( lua_State* ls, int idx)
{
	const char* cname;
	const papuga_lua_UserData* udata = (const papuga_lua_UserData*)lua_touserdata( ls, idx);
	if (!udata) return 0;
	cname = get_classname( udata->classnamemap, udata->classid);
	if (calcCheckSum(udata) != udata->checksum) return 0;
	if (!cname) return 0;
	if (!luaL_testudata( ls, idx, cname)) return 0;
	return udata;
}

static void release_UserData( papuga_lua_UserData* udata)
{
	udata->classid = 0;
	udata->objectref = 0;
	udata->destructor = 0;
	udata->checksum = 0;
	udata->classnamemap = 0;
}

static void createClassMetaTable( lua_State* ls, const char* classname, unsigned int classid, const luaL_Reg* mt)
{
	luaL_newmetatable( ls, classname);
	luaL_setfuncs( ls, mt, 0);
	lua_pushliteral( ls, "__index");
	lua_pushvalue( ls, -2);
	lua_rawset( ls, -3);

	lua_pushliteral( ls, "__newindex");
	lua_pushvalue( ls, -2);
	lua_rawset( ls, -3);

	lua_pushliteral( ls, "classname");
	lua_pushstring( ls, classname);
	lua_rawset( ls, -3);

	lua_pushliteral( ls, "classid");
	lua_pushinteger( ls, classid);
	lua_rawset( ls, -3);

	lua_pushliteral( ls, "__gc");
	lua_pushcfunction( ls, papuga_lua_destroy_UserData);
	lua_rawset( ls, -3);

	lua_setglobal( ls, classname);
}

#define ITERATOR_METATABLE_NAME "strus_iteratorclosure"
static void createIteratorMetaTable( lua_State* ls)
{
	luaL_newmetatable( ls, ITERATOR_METATABLE_NAME);
	lua_pushliteral( ls, "__gc");
	lua_pushcfunction( ls, papuga_lua_destroy_UserData);
	lua_rawset( ls, -3);
	lua_pop( ls, 1);
}

static const papuga_lua_UserData* get_IteratorUserData( lua_State* ls, int idx)
{
	const papuga_lua_UserData* udata = (const papuga_lua_UserData*)lua_touserdata( ls, idx);
	if (calcCheckSum(udata) != udata->checksum)
	{
		return 0;
	}
	if (!luaL_testudata( ls, idx, ITERATOR_METATABLE_NAME))
	{
		return 0;
	}
	return udata;
}

static int iteratorGetNext( lua_State* ls)
{
	int rt = 0;
	papuga_GetNext getNext;
	papuga_CallResult retval;
	papuga_ErrorCode errcode = papuga_Ok;
	char errbuf[ 2048];
	const papuga_lua_UserData* udata;

	void* objref = lua_touserdata( ls, lua_upvalueindex( 1));
	*(void **) &getNext = lua_touserdata( ls, lua_upvalueindex( 2));
	/* ... PF:HACK circumvents warning "ISO C forbids conversion of object pointer to function pointer type" */

	udata = get_IteratorUserData( ls, lua_upvalueindex( 3));
	if (!udata) papuga_lua_error( ls, "iterator get next", papuga_InvalidAccess);

	papuga_init_CallResult( &retval, errbuf, sizeof(errbuf));
	if (!(*getNext)( objref, &retval))
	{
		bool haserr = papuga_CallResult_hasError( &retval);
		papuga_destroy_CallResult( &retval);
		if (haserr)
		{
			papuga_lua_error_str( ls, "iterator get next", errbuf);
		}
		return 0;
	}
	rt = papuga_lua_move_CallResult( ls, &retval, udata->classnamemap, &errcode);
	if (rt < 0) papuga_lua_error( ls, "iterator get next", errcode);
	return rt;
}

static void pushIterator( lua_State* ls, void* objectref, papuga_Deleter destructor, papuga_GetNext getNext, const papuga_lua_ClassNameMap* classnamemap)
{
	papuga_lua_UserData* udata;
	lua_pushlightuserdata( ls, objectref);
	lua_pushlightuserdata( ls, *(void**)&getNext);
	udata = papuga_lua_new_userdata( ls, ITERATOR_METATABLE_NAME);
	papuga_lua_init_UserData( udata, 0, objectref, destructor, classnamemap);
	lua_pushcclosure( ls, iteratorGetNext, 3);
}

static bool Serialization_pushName_number( papuga_Serialization* result, double numval)
{
	if (IS_CONVERTIBLE_TOINT( numval))
	{
		if (numval > 0.0)
		{
			return papuga_Serialization_pushName_uint( result, (uint64_t)(numval + NUM_EPSILON));
		}
		else
		{
			return papuga_Serialization_pushName_int( result, (int64_t)(numval - NUM_EPSILON));
		}
	}
	else
	{
		return papuga_Serialization_pushName_double( result, numval);
	}
}

static bool Serialization_pushValue_number( papuga_Serialization* result, double numval)
{
	if (IS_CONVERTIBLE_TOINT( numval))
	{
		if (numval > 0.0)
		{
			return papuga_Serialization_pushValue_uint( result, (uint64_t)(numval + NUM_EPSILON));
		}
		else
		{
			return papuga_Serialization_pushValue_int( result, (int64_t)(numval - NUM_EPSILON));
		}
	}
	else
	{
		return papuga_Serialization_pushValue_double( result, numval);
	}
}

static void init_ValueVariant_number( papuga_ValueVariant* result, double numval)
{
	if (IS_CONVERTIBLE_TOINT( numval))
	{
		if (numval > 0.0)
		{
			papuga_init_ValueVariant_uint( result, (uint64_t)(numval + NUM_EPSILON));
		}
		else
		{
			papuga_init_ValueVariant_int( result, (int64_t)(numval - NUM_EPSILON));
		}
	}
	else
	{
		papuga_init_ValueVariant_double( result, numval);
	}
}


static bool serialize_key( papuga_CallArgs* as, papuga_Serialization* result, lua_State* ls, int li)
{
	bool rt = true;
	switch (lua_type (ls, li))
	{
		case LUA_TNIL:
			rt &= papuga_Serialization_pushName_void( result);
			break;
		case LUA_TNUMBER:
			rt &= Serialization_pushName_number( result, lua_tonumber( ls, li));
			break;
		case LUA_TBOOLEAN:
			rt &= papuga_Serialization_pushName_bool( result, lua_toboolean( ls, li));
			break;
		case LUA_TSTRING:
		{
			size_t strsize;
			const char* str = lua_tolstring( ls, li, &strsize);
			rt &= papuga_Serialization_pushName_string( result, str, strsize);
			break;
		}
		case LUA_TUSERDATA:
		case LUA_TTABLE:
		case LUA_TFUNCTION:
		case LUA_TTHREAD:
		case LUA_TLIGHTUSERDATA:
		default:
			as->errcode = papuga_TypeError; return false;
	}
	if (!rt)
	{
		as->errcode = papuga_NoMemError;
		return false;
	}
	return true;
}

static bool serialize_node( papuga_CallArgs* as, papuga_Serialization* result, lua_State *ls, int li);

static bool serialize_value( papuga_CallArgs* as, papuga_Serialization* result, lua_State* ls, int li)
{
	bool rt = true;
	const char* str;
	size_t strsize;

	switch (lua_type (ls, li))
	{
		case LUA_TNIL:
			rt &= papuga_Serialization_pushValue_void( result);
			break;
		case LUA_TNUMBER:
			rt &= Serialization_pushValue_number( result, lua_tonumber( ls, li));
			break;
		case LUA_TBOOLEAN:
			rt &= papuga_Serialization_pushValue_bool( result, lua_toboolean( ls, li));
			break;
		case LUA_TSTRING:
			str = lua_tolstring( ls, li, &strsize);
			rt &= papuga_Serialization_pushValue_string( result, str, strsize);
			break;
		case LUA_TTABLE:
			rt &= papuga_Serialization_pushOpen( result);
			rt &= serialize_node( as, result, ls, li);
			rt &= papuga_Serialization_pushClose( result);
			break;
		case LUA_TUSERDATA:
		{
			papuga_HostObject* hostobj;
			const papuga_lua_UserData* udata = get_UserData( ls, li);
			if (!udata)
			{
				as->errcode = papuga_TypeError;
				return false;
			}
			hostobj = (papuga_HostObject*)papuga_Allocator_alloc_HostObject( &as->allocator, udata->classid, udata->objectref, 0);
			if (!hostobj)
			{
				as->errcode = papuga_NoMemError;
				return false;
			}
			rt &= papuga_Serialization_pushValue_hostobject( result, hostobj);
		}
		case LUA_TFUNCTION:
		case LUA_TTHREAD:
		case LUA_TLIGHTUSERDATA:
		default:
			as->errcode = papuga_TypeError; return false;
	}
	if (!rt)
	{
		as->errcode = papuga_NoMemError;
		return false;
	}
	return true;
}

static bool serialize_node( papuga_CallArgs* as, papuga_Serialization* result, lua_State* ls, int li)
{
	int idx = 0;
	papuga_SerializationIter start;
	papuga_init_SerializationIter_last( &start, result);
	bool start_at_eof = papuga_SerializationIter_eof( &start);
	/*... we mark the end of the result as start of conversion to an associative array (map), if assumption of array fails */

	if (!lua_checkstack( ls, 8))
	{
		as->errcode = papuga_NoMemError;
		return false;
	}
	STACKTRACE( ls, "loop before serialize table as map or array");
	lua_pushvalue( ls, li);
	lua_pushnil( ls);
	while (lua_next( ls, -2))
	{
		STACKTRACE( ls, "loop next serialize table as map or array");
		if (!lua_isinteger( ls, -2) || lua_tointeger( ls, -2) != ++idx)
		{
			/*... not an array, convert to map and continue to build rest of map */
			if (!start_at_eof)
			{
				papuga_SerializationIter_skip( &start);
				if (!papuga_Serialization_convert_array_assoc( result, &start, 1, &as->errcode)) goto ERROR;
			}
			if (!serialize_key( as, result, ls, -2)) goto ERROR;
			if (!serialize_value( as, result, ls, -1)) goto ERROR;
			lua_pop(ls, 1);

			while (lua_next( ls, -2))
			{
				STACKTRACE( ls, "loop next serialize table as map");
				if (!serialize_key( as, result, ls, -2)) goto ERROR;
				if (!serialize_value( as, result, ls, -1)) goto ERROR;
				lua_pop( ls, 1);
			}
			break;
		}
		else
		{
			serialize_value( as, result, ls, -1);
			lua_pop(ls, 1);
		}
	}
	lua_pop( ls, 1);
	STACKTRACE( ls, "loop after serialize array");
	return true;
ERROR:
	lua_pop( ls, 3);
	STACKTRACE( ls, "loop after serialize array (error)");
	return false;
}

static bool serialize_root( papuga_CallArgs* as, lua_State *ls, int li)
{
	papuga_Serialization* result = papuga_Allocator_alloc_Serialization( &as->allocator);
	if (!result)
	{
		as->errcode = papuga_NoMemError;
		return false;
	}
	papuga_init_ValueVariant_serialization( &as->argv[as->argc], result);
	as->argc += 1;
	bool rt = true;
	rt &= serialize_node( as, result, ls, li);
	return rt;
}

static void deserialize_root( papuga_CallResult* retval, papuga_Serialization* ser, lua_State *ls, const papuga_lua_ClassNameMap* classnamemap);

static void deserialize_key( papuga_ValueVariant* item, lua_State *ls)
{
	switch (item->valuetype)
	{
		case papuga_TypeVoid:
			lua_pushnil( ls);
			break;
		case papuga_TypeDouble:
			lua_pushnumber( ls, item->value.Double);
			break;
		case papuga_TypeUInt:
			lua_pushinteger( ls, item->value.UInt);
			break;
		case papuga_TypeInt:
			lua_pushinteger( ls, item->value.Int);
			break;
		case papuga_TypeBool:
			lua_pushboolean( ls, item->value.Bool);
			break;
		case papuga_TypeString:
			lua_pushlstring( ls, item->value.string, item->length);
			break;
		case papuga_TypeLangString:
			if (item->encoding == papuga_UTF8 || item->encoding == papuga_Binary)
			{
				lua_pushlstring( ls, item->value.langstring, item->length);
			}
			else
			{
				papuga_lua_error( ls, "deserialize result", papuga_TypeError);
			}
			break;
		case papuga_TypeSerialization:
		case papuga_TypeHostObject:
			papuga_lua_error( ls, "deserialize result", papuga_TypeError);
		case papuga_TypeIterator:
		default:
			papuga_lua_error( ls, "deserialize result", papuga_NotImplemented);
	}
}

static void deserialize_value( papuga_CallResult* retval, const papuga_ValueVariant* item, lua_State *ls, const papuga_lua_ClassNameMap* classnamemap)
{
	switch (item->valuetype)
	{
		case papuga_TypeVoid:
			lua_pushnil( ls);
			break;
		case papuga_TypeDouble:
			lua_pushnumber( ls, item->value.Double);
			break;
		case papuga_TypeUInt:
			lua_pushinteger( ls, item->value.UInt);
			break;
		case papuga_TypeInt:
			lua_pushinteger( ls, item->value.Int);
			break;
		case papuga_TypeBool:
			lua_pushboolean( ls, item->value.Bool);
			break;
		case papuga_TypeString:
			lua_pushlstring( ls, item->value.string, item->length);
			break;
		case papuga_TypeLangString:
			if (item->encoding == papuga_UTF8 || item->encoding == papuga_Binary)
			{
				lua_pushlstring( ls, item->value.langstring, item->length);
			}
			else
			{
				papuga_lua_error( ls, "deserialize result", papuga_TypeError);
			}
			break;
		case papuga_TypeHostObject:
		{
			papuga_HostObject* obj = item->value.hostObject;
			papuga_lua_UserData* udata;
			const char* cname = get_classname( classnamemap, obj->classid);
			if (!cname)
			{
				papuga_lua_error( ls, "deserialize result", papuga_LogicError);
			}
			/* MEMORY LEAK ON ERROR: papuga_destroy_CallResult( retval) not called when papuga_lua_new_userdata fails because of a memory allocation error */
			udata = papuga_lua_new_userdata( ls, cname);
			papuga_lua_init_UserData( udata, obj->classid, obj->data, obj->destroy, classnamemap);
			papuga_release_HostObject( obj);
			break;
		}
		case papuga_TypeSerialization:
		{
			deserialize_root( retval, item->value.serialization, ls, classnamemap);
			break;
		}
		case papuga_TypeIterator:
		{
			papuga_Iterator* itr = item->value.iterator;
			pushIterator( ls, itr->data, itr->destroy, itr->getNext, classnamemap);
			papuga_release_Iterator( itr);
			break;
		}
		default:
			papuga_lua_error( ls, "deserialize result", papuga_NotImplemented);
	}
}

static void deserialize_node( papuga_CallResult* retval, papuga_SerializationIter* seriter, lua_State *ls, const papuga_lua_ClassNameMap* classnamemap)
{
	unsigned int keyindex = 0;
	papuga_ValueVariant name;

	papuga_init_ValueVariant( &name);

	for (; papuga_SerializationIter_tag(seriter) != papuga_TagClose; papuga_SerializationIter_skip(seriter))
	{
		switch (papuga_SerializationIter_tag(seriter))
		{
			case papuga_TagOpen:
				STACKTRACE( ls, "deserialize node open");
				lua_newtable( ls);
				papuga_SerializationIter_skip( seriter);
				deserialize_node( retval, seriter, ls, classnamemap);
				if (papuga_ValueVariant_defined( &name))
				{
					deserialize_key( &name, ls);
					papuga_init_ValueVariant( &name);
				}
				else
				{
					lua_pushinteger( ls, ++keyindex);
				}
				if (papuga_SerializationIter_tag(seriter) != papuga_TagClose)
				{
					papuga_lua_error( ls, "deserialize result", papuga_TypeError);
				}
				lua_insert( ls, -2);
				lua_rawset( ls, -3);
				break;
			case papuga_TagClose:
				return;
			case papuga_TagName:
				STACKTRACE( ls, "deserialize node name");
				if (papuga_ValueVariant_defined( &name))
				{
					papuga_lua_error( ls, "deserialize result", papuga_TypeError);
				}
				papuga_init_ValueVariant_copy( &name, papuga_SerializationIter_value(seriter));
				break;
			case papuga_TagValue:
				STACKTRACE( ls, "deserialize node value");
				if (papuga_ValueVariant_defined( &name))
				{
					deserialize_key( &name, ls);
					papuga_init_ValueVariant( &name);
				}
				else
				{
					lua_pushinteger( ls, ++keyindex);
				}
				deserialize_value( retval, papuga_SerializationIter_value(seriter), ls, classnamemap);
				lua_rawset( ls, -3);
				break;
		}
	}
	STACKTRACE( ls, "deserialize node close");
}

static void deserialize_root( papuga_CallResult* retval, papuga_Serialization* ser, lua_State *ls, const papuga_lua_ClassNameMap* classnamemap)
{
	papuga_SerializationIter seriter;
#ifdef PAPUGA_LOWLEVEL_DEBUG
	char* str = papuga_Serialization_tostring( ser);
	if (ser)
	{
		fprintf( stderr, "DESERIALIZE STRUCT:\n%s\n", str);
		free( str);
	}
#endif
	papuga_init_SerializationIter( &seriter, ser);
	lua_newtable( ls);
	deserialize_node( retval, &seriter, ls, classnamemap);
	if (!papuga_SerializationIter_eof( &seriter))
	{
		papuga_lua_error( ls, "deserialize result", papuga_TypeError);
	}
}

DLL_PUBLIC void papuga_lua_init( lua_State* ls)
{
	createIteratorMetaTable( ls);
}

DLL_PUBLIC void papuga_lua_declare_class( lua_State* ls, int classid, const char* classname, const luaL_Reg* mt)
{
	createClassMetaTable( ls, classname, classid, mt);
}

DLL_PUBLIC papuga_lua_UserData* papuga_lua_new_userdata( lua_State* ls, const char* classname)
{
	papuga_lua_UserData* rt = (papuga_lua_UserData*)lua_newuserdata( ls, sizeof(papuga_lua_UserData));
	release_UserData( rt);
	luaL_getmetatable( ls, classname);
	lua_setmetatable( ls, -2);
	return rt;
}

DLL_PUBLIC void papuga_lua_init_UserData( papuga_lua_UserData* udata, int classid, void* objectref, papuga_Deleter destructor, const papuga_lua_ClassNameMap* classnamemap)
{
	udata->classid = classid;
	udata->objectref = objectref;
	udata->destructor = destructor;
	udata->classnamemap = classnamemap;
	udata->checksum = calcCheckSum( udata);
}

DLL_PUBLIC void papuga_lua_error( lua_State* ls, const char* function, papuga_ErrorCode err)
{
	luaL_error( ls, "%s (%s)", papuga_ErrorCode_tostring( err), function);
}

DLL_PUBLIC void papuga_lua_error_str( lua_State* ls, const char* function, const char* errormsg)
{
	luaL_error( ls, "%s (%s)", errormsg, function);
}

DLL_PUBLIC bool papuga_lua_init_CallArgs( papuga_CallArgs* as, lua_State *ls, int argc, const char* classname)
{
	int argi = 1;
	papuga_init_CallArgs( as);

	if (classname)
	{
		const papuga_lua_UserData* udata = get_UserData( ls, 1);
		if (argc <= 0 || !udata)
		{
			as->errcode = papuga_MissingSelf;
			return false;
		}
		as->self = udata->objectref;
		++argi;
	}
	if (argc > papuga_MAX_NOF_ARGUMENTS)
	{
		as->errcode = papuga_NofArgsError;
		return false;
	}
	for (; argi <= argc; ++argi)
	{
		switch (lua_type (ls, argi))
		{
			case LUA_TNIL:
#ifdef PAPUGA_LOWLEVEL_DEBUG
				fprintf( stderr, "PARAM %u %s\n", argi, "NIL");
#endif
				papuga_init_ValueVariant( &as->argv[as->argc]);
				as->argc += 1;
				break;
			case LUA_TNUMBER:
				init_ValueVariant_number( &as->argv[as->argc], lua_tonumber( ls, argi));
#ifdef PAPUGA_LOWLEVEL_DEBUG
				fprintf( stderr, "PARAM %u NUMBER %f\n", argi, lua_tonumber( ls, argi));
#endif
				as->argc += 1;
				break;
			case LUA_TBOOLEAN:
				papuga_init_ValueVariant_bool( &as->argv[as->argc], lua_toboolean( ls, argi));
#ifdef PAPUGA_LOWLEVEL_DEBUG
				fprintf( stderr, "PARAM %u BOOL %d\n", argi, (int)lua_toboolean( ls, argi));
#endif
				as->argc += 1;
				break;
			case LUA_TSTRING:
			{
				size_t strsize;
				const char* str = lua_tolstring( ls, argi, &strsize);
#ifdef PAPUGA_LOWLEVEL_DEBUG
				fprintf( stderr, "PARAM %u STRING %s\n", argi, str);
#endif
				papuga_init_ValueVariant_string( &as->argv[as->argc], str, strsize);
				as->argc += 1;
				break;
			}
			case LUA_TTABLE:
#ifdef PAPUGA_LOWLEVEL_DEBUG
				fprintf( stderr, "PARAM %u TABLE\n", argi);
#endif
				if (!serialize_root( as, ls, argi)) goto ERROR;
				break;
			case LUA_TUSERDATA:
			{
#ifdef PAPUGA_LOWLEVEL_DEBUG
				fprintf( stderr, "PARAM %u USERDATA\n", argi);
#endif
				papuga_HostObject* hostobj;
				const papuga_lua_UserData* udata = get_UserData( ls, argi);
				if (!udata) goto ERROR;
				hostobj = (papuga_HostObject*)papuga_Allocator_alloc_HostObject( &as->allocator, udata->classid, udata->objectref, 0);
				if (!hostobj) goto ERROR;
				papuga_init_ValueVariant_hostobj( &as->argv[as->argc], hostobj);
				as->argc += 1;
				break;
			}
			case LUA_TFUNCTION:	goto ERROR;
			case LUA_TTHREAD:	goto ERROR;
			case LUA_TLIGHTUSERDATA:goto ERROR;
			default:		goto ERROR;
		}
	}
	return true;
ERROR:
	as->erridx = argi;
	as->errcode = papuga_TypeError;
	papuga_destroy_CallArgs( as);
	return false;
}

DLL_PUBLIC int papuga_lua_move_CallResult( lua_State *ls, papuga_CallResult* retval, const papuga_lua_ClassNameMap* classnamemap, papuga_ErrorCode* errcode)
{
	int ni = 0, ne = retval->nofvalues;
	if (papuga_CallResult_hasError( retval))
	{
		lua_pushlstring( ls, retval->errorbuf.ptr, retval->errorbuf.size);
		papuga_destroy_CallResult( retval);
		lua_error( ls);
	}
	for (; ni != ne; ++ni)
	{
		switch (retval->valuear[ni].valuetype)
		{
			case papuga_TypeVoid:
				lua_pushnil( ls);
				break;
			case papuga_TypeDouble:
				lua_pushnumber( ls, retval->valuear[ ni].value.Double);
				break;
			case papuga_TypeUInt:
				lua_pushnumber( ls, retval->valuear[ ni].value.UInt);
				break;
			case papuga_TypeInt:
				lua_pushinteger( ls, retval->valuear[ ni].value.Int);
				break;
			case papuga_TypeBool:
				lua_pushboolean( ls, retval->valuear[ ni].value.Bool);
				break;
			case papuga_TypeString:
				/* MEMORY LEAK ON ERROR: papuga_destroy_CallResult( retval) not called when lua_pushlstring fails because of a memory allocation error */
				lua_pushlstring( ls, retval->valuear[ ni].value.string, retval->valuear[ ni].length);
				break;
			case papuga_TypeLangString:
			{
				size_t strsize;
				const char* str = papuga_ValueVariant_tostring( &retval->valuear[ ni], &retval->allocator, &strsize, errcode);
				if (str)
				{
					/* MEMORY LEAK ON ERROR: papuga_destroy_CallResult( retval) not called when lua_pushlstring fails because of a memory allocation error */
					lua_pushlstring( ls, str, strsize);
				}
				else
				{
					papuga_destroy_CallResult( retval);
					papuga_lua_error( ls, "move result", papuga_NoMemError);
				}
				break;
			}
			case papuga_TypeHostObject:
			{
				papuga_HostObject* obj = retval->valuear[ ni].value.hostObject;
				papuga_lua_UserData* udata;
				const char* cname = get_classname( classnamemap, obj->classid);
				if (cname)
				{
					/* MEMORY LEAK ON ERROR: papuga_destroy_CallResult( retval) not called when papuga_lua_new_userdata fails because of a memory allocation error */
					udata = papuga_lua_new_userdata( ls, cname);
					papuga_lua_init_UserData( udata, obj->classid, obj->data, obj->destroy, classnamemap);
					papuga_release_HostObject( obj);
				}
				else
				{
					papuga_destroy_CallResult( retval);
					papuga_lua_error( ls, "move result", papuga_LogicError);
				}
				break;
			}
			case papuga_TypeSerialization:
				/* MEMORY LEAK ON ERROR: papuga_destroy_CallResult( retval) not called when papuga_lua_new_userdata fails because of a memory allocation error */
				deserialize_root( retval, retval->valuear[ ni].value.serialization, ls, classnamemap);
				break;
			case papuga_TypeIterator:
			{
				papuga_Iterator* itr = retval->valuear[ ni].value.iterator;
				/* MEMORY LEAK ON ERROR: papuga_destroy_CallResult( retval) not called when papuga_lua_new_userdata fails because of a memory allocation error */
				pushIterator( ls, itr->data, itr->destroy, itr->getNext, classnamemap);
				papuga_release_Iterator( itr);
				break;
			}
			default:
				papuga_destroy_CallResult( retval);
				papuga_lua_error( ls, "move result", papuga_TypeError);
				break;
		}
	}
	papuga_destroy_CallResult( retval);
	return retval->nofvalues;
}


