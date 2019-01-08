cmake_minimum_required( VERSION 2.8 FATAL_ERROR )

# Subdirectories:
add_subdirectory( fmt_build )

IF (WITH_LUA STREQUAL "YES")
add_subdirectory( lua_cmake )
ENDIF (WITH_LUA STREQUAL "YES")

add_subdirectory( cjson )
