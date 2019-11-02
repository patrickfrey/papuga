cmake_minimum_required( VERSION 2.8 FATAL_ERROR )

# Subdirectories:
add_subdirectory( src )
set( TESTDIR  "${PROJECT_SOURCE_DIR}/tests/requestParser" )

# Tests:
add_test( PapugaRequestParserXML_UTF8        ${CMAKE_CURRENT_BINARY_DIR}/src/testRequestParser  XML   doc  "${TESTDIR}/input.xml" )
add_test( PapugaRequestParserJSON_UTF8      ${CMAKE_CURRENT_BINARY_DIR}/src/testRequestParser  JSON doc  "${TESTDIR}/input.json" )
add_test( PapugaRequestParserXML_UTF16      ${CMAKE_CURRENT_BINARY_DIR}/src/testRequestParser  XML   doc  "${TESTDIR}/input.UTF-16.xml" )
add_test( PapugaRequestParserJSON_UCS4BE ${CMAKE_CURRENT_BINARY_DIR}/src/testRequestParser  JSON doc  "${TESTDIR}/input.UCS-4BE.json" )
