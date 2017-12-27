cmake_minimum_required( VERSION 2.8 FATAL_ERROR )

# --------------------------------------
# SOURCES AND INCLUDES
# --------------------------------------
set( dev_source_files
	libpapuga_lua_dev.c
)

set( gen_source_files
	printLuaDoc.cpp
	printLuaMod.cpp
        libpapuga_lua_gen.cpp
)

include_directories(
	"${Intl_INCLUDE_DIRS}"
	"${FMT_INCLUDE_DIRS}"
	"${LUA_INCLUDE_DIRS}"
	"${PAPUGA_INCLUDE_DIRS}"
)

link_directories(
	"${FMT_LIBRARY_DIRS}"
	"${PAPUGA_LIBRARY_DIRS}"
	"${LUA_LIBRARY_DIRS}"
)


# ------------------------------
# LIBRARIES
# ------------------------------
add_library( papuga_lua_gen SHARED ${gen_source_files} )
target_link_libraries( papuga_lua_gen papuga_gen_utils papuga_fmt )

set_target_properties(
    papuga_lua_gen
    PROPERTIES
    DEBUG_POSTFIX "${CMAKE_DEBUG_POSTFIX}"
    SOVERSION "${PAPUGA_MAJOR_VERSION}.${PAPUGA_MINOR_VERSION}"
    VERSION ${PAPUGA_VERSION}
)

add_library( papuga_lua_dev SHARED ${dev_source_files} )
target_link_libraries( papuga_lua_dev papuga_gen_utils papuga_devel strus_lua )

set_target_properties(
    papuga_lua_dev
    PROPERTIES
    DEBUG_POSTFIX "${CMAKE_DEBUG_POSTFIX}"
    SOVERSION "${PAPUGA_MAJOR_VERSION}.${PAPUGA_MINOR_VERSION}"
    VERSION ${PAPUGA_VERSION}
)

# Copy Lua devel library into a created directory easy relocatable by tests:
add_custom_command( TARGET papuga_lua_dev POST_BUILD
					  COMMAND ${CMAKE_COMMAND} -E make_directory  ${CMAKE_BINARY_DIR}/lib
					  COMMENT "Create local library directory '${CMAKE_BINARY_DIR}/lib/' " )
add_custom_command( TARGET papuga_lua_dev POST_BUILD
					  COMMAND ${CMAKE_COMMAND} -E copy_if_different  $<TARGET_FILE:papuga_lua_dev>  ${CMAKE_BINARY_DIR}/lib/
					  COMMENT "Copy Lua devel library built to local library directory" )

# ------------------------------
# INSTALLATION
# ------------------------------
install( TARGETS papuga_lua_gen
           LIBRARY DESTINATION ${LIB_INSTALL_DIR}/papuga )

install( TARGETS papuga_lua_dev
           LIBRARY DESTINATION ${LIB_INSTALL_DIR}/papuga )



