cmake_minimum_required( VERSION 2.8 FATAL_ERROR )

# --------------------------------------
# SOURCES AND INCLUDES
# --------------------------------------
set( fmt_source_files
	../fmt/fmt/format.cc
	../fmt/fmt/ostream.cc
	../fmt/fmt/posix.cc
	../fmt/fmt/printf.cc
)

include_directories(
	"${Intl_INCLUDE_DIRS}"
	"${PROJECT_SOURCE_DIR}/3rdParty/fmt"
	"${PROJECT_SOURCE_DIR}/include"
)

link_directories(
)


# ------------------------------
# LIBRARIES
# ------------------------------
add_library( papuga_fmt STATIC ${fmt_source_files} )



