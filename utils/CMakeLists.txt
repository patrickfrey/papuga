cmake_minimum_required( VERSION 2.8 FATAL_ERROR )

# --------------------------------------
# SOURCES AND INCLUDES
# --------------------------------------
set( source_files
	gen_utils.cpp
	sourceDoc.cpp
)

include_directories(
	"${Intl_INCLUDE_DIRS}"
	"${PROJECT_SOURCE_DIR}/src"
	"${PROJECT_SOURCE_DIR}/include"
)

link_directories(
)

# ------------------------------
# LIBRARIES
# ------------------------------
add_library( papuga_gen_utils STATIC ${source_files} )

