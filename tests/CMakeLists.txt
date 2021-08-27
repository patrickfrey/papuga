cmake_minimum_required( VERSION 2.8 FATAL_ERROR )

# Subdirectories:
add_subdirectory( variant )
add_subdirectory( output )
add_subdirectory( serialization )
add_subdirectory( serialization_doc )
add_subdirectory( requestParser )
add_subdirectory( schema )
IF (WITH_LUA STREQUAL "YES")
add_subdirectory( luarequest )
ENDIF (WITH_LUA STREQUAL "YES")


