cmake_minimum_required(VERSION 2.8 FATAL_ERROR)

# --------------------------------------
# SOURCES AND INCLUDES
# --------------------------------------
set(source_files
     cJSON.c
)

include_directories( "${PROJECT_SOURCE_DIR}/3rdParty/cjson/include/cjson" )

# -------------------------------------------
# CJSON LIBRARY
# -------------------------------------------
add_library( papuga_cjson STATIC ${source_files})
set_property( TARGET papuga_cjson PROPERTY POSITION_INDEPENDENT_CODE TRUE )

