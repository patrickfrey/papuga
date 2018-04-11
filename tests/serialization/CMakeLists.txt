cmake_minimum_required( VERSION 2.8 FATAL_ERROR )

# Subdirectories:
add_subdirectory( src )

# Tests:
add_test( PapugaSerialization ${CMAKE_CURRENT_BINARY_DIR}/src/testSerialization  100  3000 )


