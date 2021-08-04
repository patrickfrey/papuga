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

#define MAX_INT                   ((int)1<<(8*sizeof(int)-2))
#define MAX_DOUBLE_INT            ((int64_t)1<<53)
#define MIN_DOUBLE_INT           -((int64_t)1<<53)
#define NUM_EPSILON               (4*DBL_EPSILON)
#define IS_CONVERTIBLE_TOINT64( x)  ((x-floor(x) <= NUM_EPSILON) && x < MAX_DOUBLE_INT && x > MIN_DOUBLE_INT)
#define IS_CONVERTIBLE_TOUINT( x)   ((x-floor(x) <= NUM_EPSILON) && x < MAX_INT && x > -NUM_EPSILON)

#define PAPUGA_DEEP_TYPE_CHECKING

#undef PAPUGA_LOWLEVEL_DEBUG
#ifdef PAPUGA_LOWLEVEL_DEBUG
void STACKTRACE( lua_State* ls, const char* where)
{
	int ii;
	int tp;
	int top = lua_gettop( ls);

	fprintf( stderr, "CALLING %s STACK %d: ", where, top);

	for (ii = 1; ii <= top; ii++)
	{
		if (ii>1) fprintf( stderr, ", ");
		tp = lua_type( ls, ii);
		switch (tp) {
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
				fprintf( stderr, "<%s>", lua_typename( ls, tp));
				break;
		}
	}
	fprintf( stderr, "\n");
}
void DUMP_SERIALIZATION( const char* title, papuga_Serialization* ser)
{
	papuga_Allocator allocator;
	const char* str;
	papuga_ErrorCode errcode;

	papuga_init_Allocator( &allocator, 0, 0);
	str = papuga_Serialization_tostring( ser, &allocator, true/*linemode*/, 30/*maxdepth*/, &errcode);
	if (ser)
	{
		fprintf( stderr, "%s:\n%s\n", title, str);
	}
	else
	{
		fprintf( stderr, "%s:\nERR %s\n", title, papuga_ErrorCode_tostring( errcode));
	}
	papuga_destroy_Allocator( &allocator);
}
#else
#define STACKTRACE( ls, where)
#define DUMP_SERIALIZATION( title, ser)
#endif

static const char* get_classname( const papuga_lua_ClassEntryMap* cemap, unsigned int classid)
{
	--classid;
	return (classid > cemap->hoarsize) ? NULL : cemap->hoar[ classid];
}
static const char** get_structmembers( const papuga_lua_ClassEntryMap* cemap, unsigned int structid)
{
	--structid;
	return (structid > cemap->soarsize) ? NULL : cemap->soar[ structid];
}

struct papuga_lua_UserData
{
	int classid;
	int checksum;
	void* objectref;
	papuga_Deleter destructor;
	const papuga_lua_ClassEntryMap* cemap;
};

#define KNUTH_HASH 2654435761U
static int calcCheckSum( const papuga_lua_UserData* udata)
{
	return (((udata->classid ^ (uintptr_t)udata->objectref) * KNUTH_HASH) ^ (uintptr_t)udata->destructor ^ ((uintptr_t)udata->cemap << 7));
}

static int papuga_lua_destroy_UserData( lua_State* ls)
{
	papuga_lua_UserData* udata = (papuga_lua_UserData*)lua_touserdata( ls, 1);
	if (calcCheckSum(udata) != udata->checksum)
	{
		papuga_lua_error( ls, "destructor", papuga_InvalidAccess);
		/* exit function, lua throws (longjumps) */
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
	cname = get_classname( udata->cemap, udata->classid);
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
	udata->cemap = 0;
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
	papuga_Allocator allocator;
	papuga_CallResult retval;
	papuga_ErrorCode errcode = papuga_Ok;
	char membuf[ 4096];
	char errbuf[ 256];
	const papuga_lua_UserData* udata;

	void* objref = lua_touserdata( ls, lua_upvalueindex( 1));
	*(void **) &getNext = lua_touserdata( ls, lua_upvalueindex( 2));
	/* ... PF:HACK circumvents warning "ISO C forbids conversion of object pointer to function pointer type" */

	udata = get_IteratorUserData( ls, lua_upvalueindex( 3));
	if (!udata)
	{
		papuga_lua_error( ls, "iterator get next", papuga_InvalidAccess);
		/* exit function, lua throws (longjumps) */
	}
	papuga_init_Allocator( &allocator, membuf, sizeof(membuf));
	papuga_init_CallResult( &retval, &allocator, true/*allocator ownership*/, errbuf, sizeof(errbuf));
	if (!(*getNext)( objref, &retval))
	{
		bool haserr = papuga_CallResult_hasError( &retval);
		papuga_destroy_CallResult( &retval);
		if (haserr)
		{
			papuga_lua_error_str( ls, "iterator get next", errbuf);
			/* exit function, lua throws (longjumps) */
		}
		return 0;
	}
	rt = papuga_lua_move_CallResult( ls, &retval, udata->cemap, &errcode);
	if (rt < 0)
	{
		papuga_lua_error( ls, "iterator get next", errcode);
		/* exit function, lua throws (longjumps) */
	}
	return rt;
}

static void pushIterator( lua_State* ls, void* objectref, papuga_Deleter destructor, papuga_GetNext getNext, const papuga_lua_ClassEntryMap* cemap)
{
	papuga_lua_UserData* udata;
	lua_pushlightuserdata( ls, objectref);
	lua_pushlightuserdata( ls, *(void**)&getNext);
	udata = papuga_lua_new_userdata( ls, ITERATOR_METATABLE_NAME);
	papuga_lua_init_UserData( udata, 0, objectref, destructor, cemap);
	lua_pushcclosure( ls, iteratorGetNext, 3);
}

static bool Serialization_pushName_number( papuga_Serialization* result, double numval)
{
	if (IS_CONVERTIBLE_TOINT64( numval))
	{
		if (numval < 0.0)
		{
			return papuga_Serialization_pushName_int( result, (int64_t)(numval - NUM_EPSILON));
		}
		else
		{
			return papuga_Serialization_pushName_int( result, (int64_t)(numval + NUM_EPSILON));
		}
	}
	else
	{
		return papuga_Serialization_pushName_double( result, numval);
	}
}

static bool Serialization_pushValue_number( papuga_Serialization* result, double numval)
{
	if (IS_CONVERTIBLE_TOINT64( numval))
	{
		if (numval < 0.0)
		{
			return papuga_Serialization_pushValue_int( result, (int64_t)(numval - NUM_EPSILON));
		}
		else
		{
			return papuga_Serialization_pushValue_int( result, (int64_t)(numval + NUM_EPSILON));
		}
	}
	else
	{
		return papuga_Serialization_pushValue_double( result, numval);
	}
}

static void init_ValueVariant_number( papuga_ValueVariant* result, double numval)
{
	if (IS_CONVERTIBLE_TOINT64( numval))
	{
		if (numval < 0.0)
		{
			papuga_init_ValueVariant_int( result, (int64_t)(numval - NUM_EPSILON));
		}
		else
		{
			papuga_init_ValueVariant_int( result, (int64_t)(numval + NUM_EPSILON));
		}
	}
	else
	{
		papuga_init_ValueVariant_double( result, numval);
	}
}


static bool serialize_key( papuga_Serialization* result, lua_State* ls, int li, papuga_ErrorCode* errcode)
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
			*errcode = papuga_TypeError;
			return false;
	}
	if (!rt)
	{
		*errcode = papuga_NoMemError;
		return false;
	}
	return true;
}

static bool serialize_node( papuga_Serialization* result, lua_State* ls, int li, papuga_ErrorCode* errcode);

static bool serialize_value( papuga_Serialization* result, lua_State* ls, int li, papuga_ErrorCode* errcode)
{
	bool rt = true;
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
		{
			size_t strsize;
			const char* str = lua_tolstring( ls, li, &strsize);
			rt &= papuga_Serialization_pushValue_string( result, str, strsize);
			break;
		}
		case LUA_TTABLE:
			rt &= papuga_Serialization_pushOpen( result);
			rt &= serialize_node( result, ls, li, errcode);
			rt &= papuga_Serialization_pushClose( result);
			break;
		case LUA_TUSERDATA:
		{
			papuga_HostObject* hostobj;
			const papuga_lua_UserData* udata = get_UserData( ls, li);
			if (!udata)
			{
				*errcode = papuga_TypeError;
				return false;
			}
			hostobj = (papuga_HostObject*)papuga_Allocator_alloc_HostObject( result->allocator, udata->classid, udata->objectref, 0);
			if (!hostobj)
			{
				*errcode = papuga_NoMemError;
				return false;
			}
			rt &= papuga_Serialization_pushValue_hostobject( result, hostobj);
			break;
		}
		case LUA_TFUNCTION:
		case LUA_TTHREAD:
		case LUA_TLIGHTUSERDATA:
		default:
			*errcode = papuga_TypeError; return false;
	}
	if (!rt)
	{
		*errcode = papuga_NoMemError;
		return false;
	}
	return true;
}

static bool get_value( papuga_ValueVariant* result, papuga_Allocator* allocator, lua_State* ls, int li, papuga_ErrorCode* errcode)
{
	switch (lua_type (ls, li))
	{
		case LUA_TNIL:
			papuga_init_ValueVariant( result);
			break;
		case LUA_TNUMBER:
			papuga_init_ValueVariant_double( result, lua_tonumber( ls, li));
			break;
		case LUA_TBOOLEAN:
			papuga_init_ValueVariant_bool( result, lua_toboolean( ls, li));
			break;
		case LUA_TSTRING:
		{
			size_t strsize;
			const char* str = lua_tolstring( ls, li, &strsize);
			str = papuga_Allocator_copy_string( allocator, str, strsize);
			if (!str)
			{
				*errcode = papuga_NoMemError;
				return false;
			}
			papuga_init_ValueVariant_string( result, str, strsize);
			break;
		}
		case LUA_TTABLE:
		{
			papuga_Serialization* ser = papuga_Allocator_alloc_Serialization( allocator);
			if (!serialize_node( ser, ls, li, errcode)) return false;
			papuga_init_ValueVariant_serialization( result, ser);
			break;
		}
		case LUA_TUSERDATA:
		{
			papuga_HostObject* hostobj;
			const papuga_lua_UserData* udata = get_UserData( ls, li);
			if (!udata)
			{
				*errcode = papuga_TypeError;
				return false;
			}
			hostobj = (papuga_HostObject*)papuga_Allocator_alloc_HostObject( allocator, udata->classid, udata->objectref, 0);
			if (!hostobj)
			{
				*errcode = papuga_NoMemError;
				return false;
			}
			papuga_init_ValueVariant_hostobj( result, hostobj);
			break;
		}
		case LUA_TFUNCTION:
		case LUA_TTHREAD:
		case LUA_TLIGHTUSERDATA:
		default:
			*errcode = papuga_TypeError;
			return false;
	}
	return true;
}

static bool serialize_map( papuga_Serialization* result, lua_State* ls, int li, papuga_ErrorCode* errcode)
{
	STACKTRACE( ls, "loop before serialize table as map");
	lua_pushvalue( ls, li);
	lua_pushnil( ls);

	while (lua_next( ls, -2))
	{
		STACKTRACE( ls, "loop next serialize table as map");
		if (!serialize_key( result, ls, -2, errcode)) goto ERROR;
		if (!serialize_value( result, ls, -1, errcode)) goto ERROR;
		lua_pop( ls, 1);
	}
	lua_pop( ls, 1);
	STACKTRACE( ls, "loop after serialize table as map");
	return true;
ERROR:
	lua_pop( ls, 3);
	STACKTRACE( ls, "loop after serialize table as map (error)");
	return false;
}

static bool is_array_index( lua_State* ls, int li, int idx)
{
	if (lua_isnumber( ls, li))
	{
		double idxval = lua_tonumber( ls, li);
		if (IS_CONVERTIBLE_TOUINT( idxval))
		{
			int curidx = (int)(idxval + NUM_EPSILON);
			return (curidx == idx);
		}
	}
	return false;
}

static bool serialize_node( papuga_Serialization* result, lua_State* ls, int li, papuga_ErrorCode* errcode)
{
	int idx = 0;
	papuga_SerializationIter arrayStart;
	papuga_init_SerializationIter_end( &arrayStart, result);
	/*... assume array first */

	if (!lua_checkstack( ls, 8))
	{
		*errcode = papuga_NoMemError;
		return false;
	}
	STACKTRACE( ls, "loop before serialize table as map or array");
	lua_pushvalue( ls, li);
	lua_pushnil( ls);
	while (lua_next( ls, -2))
	{
		STACKTRACE( ls, "loop next serialize table as map or array");
		if (is_array_index( ls, -2, idx+1))
		{
			/* ... still an array, push sequence of values */
			++idx;
			serialize_value( result, ls, -1, errcode);
			lua_pop(ls, 1);
		}
		else
		{
			/*... not an array, but still assumed as one, restart converting assuming a map as result */
			if (idx) papuga_Serialization_release_tail( result, &arrayStart);
			lua_pop( ls, 3);
			return serialize_map( result, ls, li, errcode);
		}
	}
	lua_pop( ls, 1);
	STACKTRACE( ls, "loop after serialize array");
	return true;
}

static bool serialize_root( papuga_CallArgs* as, lua_State* ls, int li)
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
	rt &= serialize_node( result, ls, li, &as->errcode);
	return rt && as->errcode == papuga_Ok;
}

static bool deserialize_root( papuga_Serialization* ser, lua_State* ls, const papuga_lua_ClassEntryMap* cemap, papuga_ErrorCode* errcode);

static bool push_string( lua_State* ls, const papuga_ValueVariant* item, papuga_ErrorCode* errcode)
{
	if (item->encoding == papuga_UTF8 || item->encoding == papuga_Binary)
	{
		lua_pushlstring( ls, item->value.string, item->length);
	}
	else
	{
		papuga_Allocator allocator;
		size_t len = 0;
		const char* str;

		papuga_init_Allocator( &allocator, 0, 0);
		str = papuga_ValueVariant_tostring( item, &allocator, &len, errcode);
		if (!str)
		{
			papuga_destroy_Allocator( &allocator);
			return false;
		}
		/* MEMORY LEAK ON ERROR: allocator not freed if lua_pushlstring fails */
		lua_pushlstring( ls, str, len);
		papuga_destroy_Allocator( &allocator);
	}
	return true;
}

static bool deserialize_key( const papuga_ValueVariant* item, lua_State* ls, papuga_ErrorCode* errcode)
{
	switch (item->valuetype)
	{
		case papuga_TypeVoid:
			lua_pushnil( ls);
			break;
		case papuga_TypeDouble:
			lua_pushnumber( ls, item->value.Double);
			break;
		case papuga_TypeInt:
			lua_pushinteger( ls, item->value.Int);
			break;
		case papuga_TypeBool:
			lua_pushboolean( ls, item->value.Bool);
			break;
		case papuga_TypeString:
			return push_string( ls, item, errcode);
		case papuga_TypeSerialization:
		case papuga_TypeHostObject:
			*errcode = papuga_TypeError;
			return false;
		case papuga_TypeIterator:
		default:
			*errcode = papuga_NotImplemented;
			return false;
	}
	return true;
}

static bool deserialize_value( const papuga_ValueVariant* item, lua_State* ls, const papuga_lua_ClassEntryMap* cemap, papuga_ErrorCode* errcode)
{
	switch (item->valuetype)
	{
		case papuga_TypeVoid:
			lua_pushnil( ls);
			break;
		case papuga_TypeDouble:
			lua_pushnumber( ls, item->value.Double);
			break;
		case papuga_TypeInt:
			lua_pushinteger( ls, item->value.Int);
			break;
		case papuga_TypeBool:
			lua_pushboolean( ls, item->value.Bool);
			break;
		case papuga_TypeString:
			return push_string( ls, item, errcode);
		case papuga_TypeHostObject:
		{
			papuga_HostObject* obj;
			papuga_lua_UserData* udata;
			const char* cname;
			if (!cemap)
			{
				*errcode = papuga_TypeError;
				return false;
			}
			obj = item->value.hostObject;
			cname = get_classname( cemap, obj->classid);
			if (!cname)
			{
				*errcode = papuga_LogicError;
				return false;
			}
			udata = papuga_lua_new_userdata( ls, cname);
			papuga_lua_init_UserData( udata, obj->classid, obj->data, obj->destroy, cemap);
			papuga_release_HostObject( obj);
			break;
		}
		case papuga_TypeSerialization:
		{
			return deserialize_root( item->value.serialization, ls, cemap, errcode);
		}
		case papuga_TypeIterator:
		{
			papuga_Iterator* itr;
			if (!cemap)
			{
				*errcode = papuga_TypeError;
				return false;
			}
			itr = item->value.iterator;
			pushIterator( ls, itr->data, itr->destroy, itr->getNext, cemap);
			papuga_release_Iterator( itr);
			break;
		}
		default:
			*errcode = papuga_NotImplemented;
			return false;
	}
	return true;
}

typedef enum {NamedStruct,PositionalStruct,UndefStruct} StructElementNamingCategory;

typedef struct StructElementNaming
{
	const papuga_ValueVariant* name;
	papuga_ValueVariant membername;
	const char** members;
	int memberidx;
	StructElementNamingCategory category;
} StructElementNaming;

static bool init_StructElementNaming( StructElementNaming* ths, int structid, const papuga_lua_ClassEntryMap* cemap, papuga_ErrorCode* errcode)
{
	ths->name = NULL;
	papuga_init_ValueVariant( &ths->membername);
	if (structid)
	{
		if (!cemap)
		{
			*errcode = papuga_AtomicValueExpected;
			return false;
		}
		ths->members = get_structmembers( cemap, structid);
		if (!ths->members)
		{
			*errcode = papuga_InvalidAccess;
			return false;
		}
	}
	else
	{
		ths->members = NULL;
	}
	ths->memberidx = 0;
	ths->category = UndefStruct;
	return true;
}

static bool StructElementNaming_set_category( StructElementNaming* ths, StructElementNamingCategory category)
{
	if (ths->category == UndefStruct)
	{
		ths->category = category;
	}
	else if (ths->category != category)
	{
		return false;
	}
	return true;
}

static bool StructElementNaming_set_implicit_name( StructElementNaming* ths, papuga_ErrorCode* errcode)
{
	if (!ths->name)
	{
		if (!StructElementNaming_set_category( ths, PositionalStruct))
		{
			*errcode = papuga_MixedConstruction;
			return false;
		}
		if (!ths->members[ ths->memberidx])
		{
			*errcode = papuga_InvalidAccess;
			return false;
		}
		papuga_init_ValueVariant_charp( &ths->membername, ths->members[ ths->memberidx]);
		++ths->memberidx;
		ths->name = &ths->membername;
	}
	else if (papuga_ValueVariant_isstring( ths->name))
	{
		if (!StructElementNaming_set_category( ths, NamedStruct))
		{
			*errcode = papuga_MixedConstruction;
			return false;
		}
	}
	else if ((papuga_Type)ths->name->valuetype == papuga_TypeInt)
	{
		int new_midx;
		if (!StructElementNaming_set_category( ths, PositionalStruct))
		{
			*errcode = papuga_MixedConstruction;
			return false;
		}
		if (ths->name->value.Int < 0 || ths->name->value.Int > 0x7fFF)
		{
			*errcode = papuga_InvalidAccess;
			return false;
		}
		new_midx  = ths->name->value.Int;
		for (; ths->members[ ths->memberidx] && ths->memberidx < new_midx; ++ths->memberidx){}
		if (ths->memberidx == new_midx)
		{
			papuga_init_ValueVariant_charp( &ths->membername, ths->members[ ths->memberidx]);
			ths->name = &ths->membername;
			++ths->memberidx;
		}
		else
		{
			*errcode = papuga_InvalidAccess;
			return false;
		}
	}
	else
	{
		*errcode = papuga_TypeError;
		return false;
	}
	return true;
}

static bool StructElementNaming_set_name( StructElementNaming* ths, const papuga_ValueVariant* name, papuga_ErrorCode* errcode)
{
	if (ths->name)
	{
		*errcode = papuga_TypeError;
		return false;
	}
	if (!papuga_ValueVariant_isatomic( name))
	{
		*errcode = papuga_TypeError;
		return false;
	}
	ths->name = name;
	return true;
}

static void StructElementNaming_reset_name( StructElementNaming* ths)
{
	ths->name = NULL;
}

static bool deserialize_node( papuga_SerializationIter* seriter, lua_State* ls, int structid, const papuga_lua_ClassEntryMap* cemap, papuga_ErrorCode* errcode)
{
	StructElementNaming state;

	if (!init_StructElementNaming( &state, structid, cemap, errcode))
	{
		return false;
	}
	for (; papuga_SerializationIter_tag(seriter) != papuga_TagClose; papuga_SerializationIter_skip(seriter))
	{
		switch (papuga_SerializationIter_tag(seriter))
		{
			case papuga_TagOpen:
			{
				int substructure_structid = 0;
				const papuga_ValueVariant* openarg = papuga_SerializationIter_value( seriter);
				if (openarg->valuetype == papuga_TypeInt)
				{
					if (openarg->value.Int < 0 || openarg->value.Int > 0x7fFF)
					{
						*errcode = papuga_InvalidAccess;
						return false;
					}
					substructure_structid  = openarg->value.Int;
				}
				STACKTRACE( ls, "deserialize node open");
				lua_newtable( ls);
				papuga_SerializationIter_skip( seriter);
				if (!deserialize_node( seriter, ls, substructure_structid, cemap, errcode))
				{
					return false;
				}
				if (structid)
				{
					if (!StructElementNaming_set_implicit_name( &state, errcode))
					{
						return false;
					}
				}
				if (state.name)
				{
					if (!deserialize_key( state.name, ls, errcode)) return false;
					StructElementNaming_reset_name( &state);
				}
				else
				{
					lua_pushinteger( ls, ++state.memberidx);
				}
				if (papuga_SerializationIter_tag(seriter) != papuga_TagClose)
				{
					*errcode = papuga_TypeError;
					return false;
				}
				lua_insert( ls, -2);
				lua_rawset( ls, -3);
				StructElementNaming_reset_name( &state);
				break;
			}
			case papuga_TagClose:
				STACKTRACE( ls, "deserialize node close");
				return true;
			case papuga_TagName:
				STACKTRACE( ls, "deserialize node name");
				if (!StructElementNaming_set_name( &state, papuga_SerializationIter_value(seriter), errcode))
				{
					return false;
				}
				break;
			case papuga_TagValue:
				STACKTRACE( ls, "deserialize node value");
				if (structid)
				{
					if (!StructElementNaming_set_implicit_name( &state, errcode))
					{
						return false;
					}
				}
				if (state.name)
				{
					if (!deserialize_key( state.name, ls, errcode)) return false;
					StructElementNaming_reset_name( &state);
				}
				else
				{
					lua_pushinteger( ls, ++state.memberidx);
				}
				if (!deserialize_value( papuga_SerializationIter_value(seriter), ls, cemap, errcode))
				{
					return false;
				}
				lua_rawset( ls, -3);
				break;
		}
	}
	STACKTRACE( ls, "deserialize node close");
	return true;
}

static bool deserialize_root( papuga_Serialization* ser, lua_State* ls, const papuga_lua_ClassEntryMap* cemap, papuga_ErrorCode* errcode)
{
	papuga_SerializationIter seriter;
	int structid = papuga_Serialization_structid( ser);
	DUMP_SERIALIZATION( "DESERIALIZE STRUCT", ser);
	papuga_init_SerializationIter( &seriter, ser);
	lua_newtable( ls);
	if (!deserialize_node( &seriter, ls, structid, cemap, errcode))
	{
		return false;
	}
	if (!papuga_SerializationIter_eof( &seriter))
	{
		*errcode = papuga_TypeError;
		return false;
	}
	return true;
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

DLL_PUBLIC void papuga_lua_init_UserData( papuga_lua_UserData* udata, int classid, void* objectref, papuga_Deleter destructor, const papuga_lua_ClassEntryMap* cemap)
{
	udata->classid = classid;
	udata->objectref = objectref;
	udata->destructor = destructor;
	udata->cemap = cemap;
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

DLL_PUBLIC bool papuga_lua_set_CallArgs( papuga_CallArgs* as, lua_State* ls, int argc, const char* classname)
{
	int argi = 1;

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
				papuga_HostObject* hostobj;
				const papuga_lua_UserData* udata = get_UserData( ls, argi);
				if (!udata) goto ERROR;
#ifdef PAPUGA_LOWLEVEL_DEBUG
				fprintf( stderr, "PARAM %u USERDATA\n", argi);
#endif
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

static bool lua_push_ValueVariant( lua_State *ls, const papuga_ValueVariant* value, const papuga_lua_ClassEntryMap* cemap, papuga_ErrorCode* errcode)
{
	/* NOTE: Returns only with false if an error is not thrown by luaL_error */
	papuga_HostObject* obj;
	papuga_lua_UserData* udata;
	const char* cname;
	papuga_Iterator* itr;
	if (!lua_checkstack( ls, 1))
	{
		*errcode = papuga_NoMemError;
		return false;
	}
	switch (value->valuetype)
	{
		case papuga_TypeVoid:
			lua_pushnil( ls);
			break;
		case papuga_TypeDouble:
			lua_pushnumber( ls, value->value.Double);
			break;
		case papuga_TypeInt:
			lua_pushinteger( ls, value->value.Int);
			break;
		case papuga_TypeBool:
			lua_pushboolean( ls, value->value.Bool);
			break;
		case papuga_TypeString:
			return push_string( ls, value, errcode);
		case papuga_TypeHostObject:
			/* REMARK: Ownership of hostobject transfered */
			if (!cemap)
			{
				*errcode = papuga_AtomicValueExpected;
				return false;
			}
			obj = value->value.hostObject;
			cname = get_classname( cemap, obj->classid);
			if (cname)
			{
				udata = papuga_lua_new_userdata( ls, cname);
				papuga_lua_init_UserData( udata, obj->classid, obj->data, obj->destroy, cemap);
				papuga_release_HostObject( obj);
			}
			else
			{
				*errcode = papuga_LogicError;
				return false;
			}
			break;
		case papuga_TypeSerialization:
			return deserialize_root( value->value.serialization, ls, cemap, errcode);
		case papuga_TypeIterator:
			if (!cemap)
			{
				*errcode = papuga_AtomicValueExpected;
				return false;
			}
			/* REMARK: Ownership of iterator transfered */
			itr = value->value.iterator;
			pushIterator( ls, itr->data, itr->destroy, itr->getNext, cemap);
			papuga_release_Iterator( itr);
			break;
		default:
			*errcode = papuga_TypeError;
			return false;
	}
	return true;
}

DLL_PUBLIC int papuga_lua_move_CallResult( lua_State* ls, papuga_CallResult* retval, const papuga_lua_ClassEntryMap* cemap, papuga_ErrorCode* errcode)
{
	/* NOTE:
	 *	Memory leak on error, papuga_destroy_CallResult( retval) not called when Lua fails because of a memory allocation error
	 */
	int ni = 0, ne = retval->nofvalues;
	if (papuga_CallResult_hasError( retval))
	{
		lua_pushlstring( ls, retval->errorbuf.ptr, retval->errorbuf.size);
		papuga_destroy_CallResult( retval);
		lua_error( ls);
	}
	for (; ni != ne; ++ni)
	{
		if (!lua_push_ValueVariant( ls, &retval->valuear[ ni], cemap, errcode))
		{
			papuga_destroy_CallResult( retval);
			papuga_lua_error( ls, "move result", *errcode);
			/* exit function, lua throws (longjumps) */
		}
	}
	papuga_destroy_CallResult( retval);
	return retval->nofvalues;
}

DLL_PUBLIC bool papuga_lua_push_value( lua_State *ls, const papuga_ValueVariant* value, const papuga_lua_ClassEntryMap* cemap, papuga_ErrorCode* errcode)
{
	return lua_push_ValueVariant( ls, value, cemap, errcode);
}

DLL_PUBLIC bool papuga_lua_push_value_plain( lua_State *ls, const papuga_ValueVariant* value, papuga_ErrorCode* errcode)
{
	return lua_push_ValueVariant( ls, value, NULL/*cemap*/, errcode);
}

DLL_PUBLIC bool papuga_lua_push_serialization( lua_State *ls, const papuga_Serialization* ser, const papuga_lua_ClassEntryMap* cemap, papuga_ErrorCode* errcode)
{
	return deserialize_root( (papuga_Serialization*)ser, ls, cemap, errcode);
}

DLL_PUBLIC bool papuga_lua_serialize( lua_State *ls, papuga_Serialization* dest, int li, papuga_ErrorCode* errcode)
{
	bool rt = true;
	rt &= serialize_node( dest, ls, li, errcode);
	return rt && *errcode == papuga_Ok;
}

DLL_PUBLIC bool papuga_lua_value( lua_State *ls, papuga_ValueVariant* result, papuga_Allocator* allocator, int li, papuga_ErrorCode* errcode)
{
	bool rt = true;
	rt &= get_value( result, allocator, ls, li, errcode);
	return rt && *errcode == papuga_Ok;
}

