cmake_minimum_required( VERSION 2.8 FATAL_ERROR )

# Subdirectories:
add_subdirectory( src )
set( TESTDIR  "${PROJECT_SOURCE_DIR}/tests/schema" )
set( TESTBIN  "${CMAKE_CURRENT_BINARY_DIR}/src/testSchema" )


# Tests:
add_test( PapugaSchema_query1 ${TESTBIN} ${TESTDIR}/query1/schema.psm query ${TESTDIR}/query1/input.json )
add_test( PapugaSchema_query2 ${TESTBIN} ${TESTDIR}/query1/schema.psm query ${TESTDIR}/query1/input.xml )

