cmake_minimum_required( VERSION 2.8 FATAL_ERROR )

# --------------------------------------
# PYTHON
# --------------------------------------
include( ./python.cmake )

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
	"${PROJECT_SOURCE_DIR}/3rdParty/fmt"
	"${PROJECT_SOURCE_DIR}/3rdParty/python"
	"${PROJECT_SOURCE_DIR}/include"
	${PYTHON_INCLUDE_DIRS}
)

link_directories(
	"${PROJECT_SOURCE_DIR}/3rdParty/fmt/fmt"
	${PYTHON_LIBRARY_DIRS}
)


# ------------------------------
# LIBRARIES
# ------------------------------
add_library( papuga_python3_gen SHARED ${python3_gen_source_files} )
target_link_libraries( papuga_python3_gen fmt papuga_gen_utils )

set_target_properties(
    papuga_python3_gen
    PROPERTIES
    DEBUG_POSTFIX "_d"
    SOVERSION "${PAPUGA_MAJOR_VERSION}.${PAPUGA_MINOR_VERSION}"
    VERSION ${PAPUGA_VERSION}
)

add_library( papuga_python3_dev SHARED ${python3_dev_source_files} )
target_link_libraries( papuga_python3_dev papuga_gen_utils papuga_devel )

set_target_properties(
    papuga_python3_dev
    PROPERTIES
    DEBUG_POSTFIX "_d"
    SOVERSION "${PAPUGA_MAJOR_VERSION}.${PAPUGA_MINOR_VERSION}"
    VERSION ${PAPUGA_VERSION}
)


# ------------------------------
# INSTALLATION
# ------------------------------
install( TARGETS papuga_python3_gen
           LIBRARY DESTINATION ${LIB_INSTALL_DIR}/papuga )

install( TARGETS papuga_python3_dev
           LIBRARY DESTINATION ${LIB_INSTALL_DIR}/papuga )



