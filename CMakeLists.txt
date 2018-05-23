cmake_minimum_required( VERSION 2.8 FATAL_ERROR )

# must be done BEFORE project in order to work!
include(cmake/install_prefix.cmake)

# Project globals:
project( Papuga )
set( PAPUGA_MAJOR_VERSION 0 )
set( PAPUGA_MINOR_VERSION 0 )
set( PAPUGA_PATCH_VERSION 0 )
set( PAPUGA_VERSION ${PAPUGA_MAJOR_VERSION}.${PAPUGA_MINOR_VERSION}.${PAPUGA_PATCH_VERSION} )

set( CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake" )
include( cmake/cxx11.cmake )
check_for_cxx11_compiler(HAVE_CXX11)
IF(HAVE_CXX11)
  enable_cxx11()
  include( cmake/CXX11Features.cmake )
ENDIF()
include( cmake/dist.cmake )
include( cmake/LinuxDistribution.cmake )
include( cmake/platform.cmake )
include( cmake/build_rules.cmake )
include( cmake/boost.cmake)
include( cmake/link_rules.cmake )
include( cmake/intl.cmake )
include( cmake/libdl.cmake )
IF (NOT TARGET uninstall AND NOT TARGET distclean )
include( cmake/standard_targets.cmake )
ENDIF (NOT TARGET uninstall AND NOT TARGET distclean )

IF (WITH_PHP STREQUAL "YES")
MESSAGE( STATUS  "PHP7 bindings enabled." )
set( PAPUGA_LANGUAGE_SUPPORT_PHP7  1 )
include( cmake/php7.cmake )
ELSE (WITH_PHP STREQUAL "YES")
MESSAGE( STATUS  "PHP7 bindings generation is not enabled. Start cmake with -DWITH_PHP=\"YES\" to enable it." )
set( PAPUGA_LANGUAGE_SUPPORT_PHP7  0 )
ENDIF (WITH_PHP STREQUAL "YES")

IF (WITH_PYTHON STREQUAL "YES")
MESSAGE( STATUS  "Python3 bindings enabled." )
set( PAPUGA_LANGUAGE_SUPPORT_PYTHON3  1 )
include( cmake/python3.cmake )
ELSE (WITH_PYTHON STREQUAL "YES")
MESSAGE( STATUS  "Python3 bindings generation is not enabled. Start cmake with -DWITH_PYTHON=\"YES\" to enable it." )
set( PAPUGA_LANGUAGE_SUPPORT_PYTHON3  0 )
ENDIF (WITH_PYTHON STREQUAL "YES")

set( PAPUGA_GETTEXT_PACKAGE "papuga-dom" )	 #... 1st parameter of bindtextdomain(...)
set( PAPUGA_GETTEXT_LOCALEDIR "" )			 #... 2nd parameter of bindtextdomain(...)
configure_file( "${PROJECT_SOURCE_DIR}/src/internationalization.c.in"           "${CMAKE_CURRENT_BINARY_DIR}/src/internationalization.c"  @ONLY )
configure_file( "${PROJECT_SOURCE_DIR}/include/papuga/version.h.in"          "${CMAKE_CURRENT_BINARY_DIR}/include/papuga/version.h"   @ONLY )
configure_file( "${PROJECT_SOURCE_DIR}/include/papuga/languages.h.in"      "${CMAKE_CURRENT_BINARY_DIR}/include/papuga/languages.h"   @ONLY )

enable_testing()

# Path declarations:
set( FMT_INCLUDE_DIRS                             "${PROJECT_SOURCE_DIR}/3rdParty/fmt_4_1" )
set( FMT_LIBRARY_DIRS                             "${PROJECT_SOURCE_DIR}/3rdParty/fmt_build" )
set( LUA_INCLUDE_DIRS                             "${PROJECT_SOURCE_DIR}/3rdParty/lua" )
set( LUA_LIBRARY_DIRS                             "${PROJECT_SOURCE_DIR}/3rdParty/lua_cmake" )
set( TEXTWOLF_INCLUDE_DIRS                 "${PROJECT_SOURCE_DIR}/3rdParty/textwolf/include" )
set( PAPUGA_INCLUDE_DIRS                      "${CMAKE_CURRENT_BINARY_DIR}/include"  "${PROJECT_SOURCE_DIR}/include" )
set( PAPUGA_SOURCE_DIRS                      "${PROJECT_SOURCE_DIR}/src" )
set( PAPUGA_LIBRARY_DIRS                      "${CMAKE_CURRENT_BINARY_DIR}/src" )
set( PAPUGA_PHP7_SOURCE_DIRS            "${PROJECT_SOURCE_DIR}/php7" )
set( PAPUGA_PHP7_LIBRARY_DIRS            "${CMAKE_CURRENT_BINARY_DIR}/php7" )
set( PAPUGA_PYTHON3_SOURCE_DIRS     "${PROJECT_SOURCE_DIR}/python3" )
set( PAPUGA_PYTHON3_LIBRARY_DIRS     "${CMAKE_CURRENT_BINARY_DIR}/python3" )
set( PAPUGA_LUA_SOURCE_DIRS              "${PROJECT_SOURCE_DIR}/lua" )
set( PAPUGA_LUA_LIBRARY_DIRS              "${CMAKE_CURRENT_BINARY_DIR}/lua" )

# Subdirectories:
add_subdirectory( 3rdParty )
add_subdirectory( src )
add_subdirectory( utils )
add_subdirectory( lua )

IF (WITH_PYTHON STREQUAL "YES")
add_subdirectory( python3 )
ENDIF (WITH_PYTHON STREQUAL "YES")

IF (WITH_PHP STREQUAL "YES")
add_subdirectory( php7 )
ENDIF (WITH_PHP STREQUAL "YES")

add_subdirectory( tests )
  
# Install header files:
install(DIRECTORY include/papuga
          DESTINATION include
          FILES_MATCHING PATTERN "*.hpp")
install(DIRECTORY include/papuga
          DESTINATION include
          FILES_MATCHING PATTERN "*.h")
install(FILES  include/papuga.h
          DESTINATION include )



