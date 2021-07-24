cmake_minimum_required( VERSION 2.8 FATAL_ERROR )

# --------------------------------------
# SOURCES AND INCLUDES
# --------------------------------------
IF (WITH_LUA STREQUAL "YES")
	set( source_files luaRequestHandler.cpp )
ELSE (WITH_LUA STREQUAL "YES")
	set( source_files luaRequestHandler_stub.cpp )
ENDIF (WITH_LUA STREQUAL "YES")

include_directories(
	"${Intl_INCLUDE_DIRS}"
	"${PROJECT_SOURCE_DIR}/include"
	"${PROJECT_SOURCE_DIR}/3rdParty/textwolf/include"
	"${PROJECT_SOURCE_DIR}/3rdParty/cjson/include"
	${LUA_INCLUDE_DIRS}
	"${PAPUGA_INCLUDE_DIRS}"
)

link_directories(
	"${PAPUGA_LIBRARY_DIRS}"
	${LUA_LIBRARY_DIRS}
)

# ------------------------------
# LIBRARIES
# ------------------------------
add_library( papuga_luarequest STATIC ${source_files} )
target_link_libraries( papuga_luarequest papuga_devel papuga_lua_dev ${Intl_LIBRARIES} )

# ------------------------------
# INSTALLATION
# ------------------------------
install( TARGETS papuga_luarequest
           ARCHIVE DESTINATION ${LIB_INSTALL_DIR}/papuga )

