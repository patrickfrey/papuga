cmake_minimum_required( VERSION 2.8 FATAL_ERROR )

# Subdirectories:
add_subdirectory( src )
set( TESTDIR  "${PROJECT_SOURCE_DIR}/tests/serialization_doc" )

# Tests:
add_test( PapugaSerialization_XML  ${CMAKE_CURRENT_BINARY_DIR}/src/testSerializationDoc  XML  "${TESTDIR}/input.xml" "${TESTDIR}/output_xml.txt" )
add_test( PapugaSerialization_JSON  ${CMAKE_CURRENT_BINARY_DIR}/src/testSerializationDoc  JSON  "${TESTDIR}/input.json" "${TESTDIR}/output_json.txt" )



