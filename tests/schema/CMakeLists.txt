cmake_minimum_required( VERSION 2.8 FATAL_ERROR )

# Subdirectories:
add_subdirectory( src )

set( TESTDIR    "${PROJECT_SOURCE_DIR}/tests/schema" )
set( SCHEMADIR  "${TESTDIR}/schema" )
set( TESTBIN    "${CMAKE_CURRENT_BINARY_DIR}/src/testSchema" )


# Tests:
add_test( PapugaSchema_query1 ${TESTBIN} ${SCHEMADIR}/query.psm query ${TESTDIR}/query/input.json ${TESTDIR}/query/expected.txt )
add_test( PapugaSchema_query2 ${TESTBIN} ${SCHEMADIR}/query.psm query ${TESTDIR}/query/input.xml ${TESTDIR}/query/expected.txt )

