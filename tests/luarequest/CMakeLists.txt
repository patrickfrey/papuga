cmake_minimum_required( VERSION 2.8 FATAL_ERROR )

# Subdirectories:
add_subdirectory( src )

set( TESTDIR    "${PROJECT_SOURCE_DIR}/tests/luarequest" )
set( SCHEMADIR  "${PROJECT_SOURCE_DIR}/tests/schema/schema" )
set( SCRIPTDIR  "${TESTDIR}/scripts" )
set( TESTBIN    "${CMAKE_CURRENT_BINARY_DIR}/src/testLuaRequest" )

# Tests:
add_test( PapugaLuaRequest_Analyze ${TESTBIN} ${SCRIPTDIR} ${SCHEMADIR} ${TESTDIR}/analyze/run.txt ${TESTDIR}/analyze/expect.txt )
add_test( PapugaLuaRequest_DistCount ${TESTBIN} ${SCRIPTDIR} ${SCHEMADIR} ${TESTDIR}/distcount/run.txt ${TESTDIR}/distcount/expect.txt )

