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
include( cmake/dist.cmake )
include( cmake/LinuxDistribution.cmake )
include( cmake/build_rules.cmake )
include( cmake/boost.cmake)
include( cmake/intl.cmake )

set( PAPUGA_GETTEXT_PACKAGE "papuga-dom" )	 #... 1st parameter of bindtextdomain(...)
set( PAPUGA_GETTEXT_LOCALEDIR "" )			 #... 2nd parameter of bindtextdomain(...)
configure_file( "${PROJECT_SOURCE_DIR}/src/internationalization.c.in"       "${PROJECT_SOURCE_DIR}/src/internationalization.c"  @ONLY )
configure_file( "${PROJECT_SOURCE_DIR}/include/papuga/version.h.in"      "${CMAKE_BINARY_DIR}/include/papuga/version.h"   @ONLY )

enable_testing()

IF (NOT WITH_PHP STREQUAL "YES")
MESSAGE( STATUS  "PHP bindings generation is not enabled. Start cmake with -DWITH_PHP=\"YES\" to enable it." )
ENDIF (NOT WITH_PHP STREQUAL "YES")

# Subdirectories:
add_subdirectory( 3rdParty )
add_subdirectory( src )
add_subdirectory( utils )
add_subdirectory( lua )
IF (WITH_PHP STREQUAL "YES")
add_subdirectory( php )
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

IF (NOT TARGET uninstall)
include( cmake/standard_targets.cmake )
ENDIF (NOT TARGET uninstall)


