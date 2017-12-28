cmake_minimum_required( VERSION 2.8 FATAL_ERROR )

# --------------------------------------
# SOURCES AND INCLUDES
# --------------------------------------
set( php7_dev_source_files
	libpapuga_php7_dev.c
)

set( php7_gen_source_files
	printPhp7Doc.cpp
	printPhp7Mod.cpp
        libpapuga_php7_gen.cpp
)

include_directories(
	"${Intl_INCLUDE_DIRS}"
        "${FMT_INCLUDE_DIRS}"
	"${PHP7_INCLUDE_DIRS}"
	"${PAPUGA_INCLUDE_DIRS}"
)

link_directories(
	"${FMT_LIBRARY_DIRS}"
	"${PAPUGA_LIBRARY_DIRS}"
	"${PHP7_LIBRARY_DIRS}"
)

# ------------------------------
# LIBRARIES
# ------------------------------
add_library( papuga_php7_gen SHARED ${php7_gen_source_files} )
target_link_libraries( papuga_php7_gen papuga_gen_utils papuga_fmt  )

set_target_properties(
    papuga_php7_gen
    PROPERTIES
    DEBUG_POSTFIX "${CMAKE_DEBUG_POSTFIX}"
    SOVERSION "${PAPUGA_MAJOR_VERSION}.${PAPUGA_MINOR_VERSION}"
    VERSION ${PAPUGA_VERSION}
)

add_library( papuga_php7_dev SHARED ${php7_dev_source_files} )
target_link_libraries( papuga_php7_dev papuga_gen_utils papuga_devel ${PHP7_LIBRARIES} )

set_target_properties(
	papuga_php7_dev
	PROPERTIES
	DEBUG_POSTFIX "${CMAKE_DEBUG_POSTFIX}"
	SOVERSION "${PAPUGA_MAJOR_VERSION}.${PAPUGA_MINOR_VERSION}"
    VERSION ${PAPUGA_VERSION}
)

if(APPLE)
set_target_properties(
	papuga_php7_dev
	PROPERTIES
	LINK_FLAGS "-undefined dynamic_lookup" )
endif(APPLE)

# Copy php7 devel library into a created directory easy relocatable by tests:
add_custom_command( TARGET papuga_php7_dev POST_BUILD
					  COMMAND ${CMAKE_COMMAND} -E make_directory  ${CMAKE_BINARY_DIR}/lib )
add_custom_command( TARGET papuga_php7_dev POST_BUILD
					  COMMAND ${CMAKE_COMMAND} -E copy_if_different  $<TARGET_FILE:papuga_php7_dev>  ${CMAKE_BINARY_DIR}/lib/
					  COMMENT "Copy Php7 devel library built to ${CMAKE_BINARY_DIR}/lib/" )

# ------------------------------
# INSTALLATION
# ------------------------------
install( TARGETS papuga_php7_gen
           LIBRARY DESTINATION ${LIB_INSTALL_DIR}/papuga )

install( TARGETS papuga_php7_dev
           LIBRARY DESTINATION ${LIB_INSTALL_DIR}/papuga )



