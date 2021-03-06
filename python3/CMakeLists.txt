cmake_minimum_required( VERSION 2.8 FATAL_ERROR )

# --------------------------------------
# SOURCES AND INCLUDES
# --------------------------------------
set( python3_dev_source_files
	libpapuga_python3_dev.c
)

set( python3_gen_source_files
	printPython3Doc.cpp
	printPython3Mod.cpp
        libpapuga_python3_gen.cpp
)

include_directories(
	"${Intl_INCLUDE_DIRS}"
        "${FMT_INCLUDE_DIRS}"
	"${PYTHON3_INCLUDE_DIRS}"
	"${PAPUGA_INCLUDE_DIRS}"
)

link_directories(
	"${FMT_LIBRARY_DIRS}"
	"${PAPUGA_LIBRARY_DIRS}"
	${PYTHON3_LIBRARY_DIRS}
)


# ------------------------------
# LIBRARIES
# ------------------------------
add_library( papuga_python3_gen SHARED ${python3_gen_source_files} )
target_link_libraries( papuga_python3_gen papuga_gen_utils papuga_fmt  )

set_target_properties(
    papuga_python3_gen
    PROPERTIES
    DEBUG_POSTFIX "${CMAKE_DEBUG_POSTFIX}"
    SOVERSION "${PAPUGA_MAJOR_VERSION}.${PAPUGA_MINOR_VERSION}"
    VERSION ${PAPUGA_VERSION}
)

add_library( papuga_python3_dev SHARED ${python3_dev_source_files} )
target_link_libraries( papuga_python3_dev papuga_gen_utils papuga_devel ${PYTHON3_LIBRARIES} )

set_target_properties(
    papuga_python3_dev
    PROPERTIES
    DEBUG_POSTFIX "${CMAKE_DEBUG_POSTFIX}"
    SOVERSION "${PAPUGA_MAJOR_VERSION}.${PAPUGA_MINOR_VERSION}"
    VERSION ${PAPUGA_VERSION}
)

# Copy python3 devel library into a created directory easy relocatable by tests:
add_custom_command( TARGET papuga_python3_dev POST_BUILD
					  COMMAND ${CMAKE_COMMAND} -E make_directory  ${CMAKE_BINARY_DIR}/lib )
add_custom_command( TARGET papuga_python3_dev POST_BUILD
					  COMMAND ${CMAKE_COMMAND} -E copy_if_different  $<TARGET_FILE:papuga_python3_dev>  ${CMAKE_BINARY_DIR}/lib/
					  COMMENT "Copy Python3 devel library built to ${CMAKE_BINARY_DIR}/lib/" )

# ------------------------------
# INSTALLATION
# ------------------------------
install( TARGETS papuga_python3_gen
           LIBRARY DESTINATION ${LIB_INSTALL_DIR}/papuga )

install( TARGETS papuga_python3_dev
           LIBRARY DESTINATION ${LIB_INSTALL_DIR}/papuga )



