cmake_minimum_required( VERSION 2.8 FATAL_ERROR )

# Subdirectories:
add_subdirectory( src )

# Tests:
add_test( PapugaLuaRequest ${CMAKE_CURRENT_BINARY_DIR}/src/testLuaRequest )


