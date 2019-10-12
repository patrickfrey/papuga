cmake_minimum_required( VERSION 2.8 FATAL_ERROR )

# Subdirectories:
add_subdirectory( src )

function( DeclareSerializationOutputTestCall Title FileName Format )
  set( _testprg "${CMAKE_CURRENT_BINARY_DIR}/src/testSerializationOutput" )
  add_test( PapugaSerializationOutput_${Title}_${Format}  ${_testprg}  ${CMAKE_CURRENT_SOURCE_DIR}/${FileName}  ${Format}  ${CMAKE_CURRENT_BINARY_DIR} )
endfunction( DeclareSerializationOutputTestCall )

function( DeclareSerializationOutputTest Title FileName )
  DeclareSerializationOutputTestCall( ${Title} ${FileName} xml )
  DeclareSerializationOutputTestCall( ${Title} ${FileName} json )
  DeclareSerializationOutputTestCall( ${Title} ${FileName} text )
  DeclareSerializationOutputTestCall( ${Title} ${FileName} html )
endfunction( DeclareSerializationOutputTest )

# Tests:
DeclareSerializationOutputTest( Simple simple.json )
DeclareSerializationOutputTest( Empty1 empty1.json )
DeclareSerializationOutputTest( Empty2 empty2.json )
DeclareSerializationOutputTest( Single single.json )
DeclareSerializationOutputTest( LinkList linklist.json )
DeclareSerializationOutputTest( Complex complex.json )





