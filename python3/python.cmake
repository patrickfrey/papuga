cmake_minimum_required( VERSION 2.8 FATAL_ERROR )

# --------------------------------------
# PYTHON
# --------------------------------------
find_program( PYTHON_CONFIG_EXECUTABLE NAMES  "python3-config"  "python-config" )
MESSAGE( "Python-config executable:  ${PYTHON_CONFIG_EXECUTABLE}" )

find_program( PYTHON_EXECUTABLE NAMES  "python3*"  python )
MESSAGE( "Python executable: ${PYTHON_EXECUTABLE}" )

execute_process( COMMAND  ${PYTHON_CONFIG_EXECUTABLE} --includes  OUTPUT_VARIABLE  PYTHON_INCLUDES )
string( REPLACE "-I"  ""  PYTHON_INCLUDE_DIRS  ${PYTHON_INCLUDES}  )
string( STRIP  ${PYTHON_INCLUDE_DIRS}  PYTHON_INCLUDE_DIRS )
string( REPLACE " "  ";"  PYTHON_INCLUDE_DIRS  ${PYTHON_INCLUDE_DIRS}  )
MESSAGE( "PYTHON include dirs: ${PYTHON_INCLUDE_DIRS}" )

execute_process( COMMAND  ${PYTHON_CONFIG_EXECUTABLE} --ldflags  OUTPUT_VARIABLE PYTHON_LDFLAGS )
string( REPLACE "-L"  ""  PYTHON_LIBRARY_DIRS  ${PYTHON_LDFLAGS}  )
string( STRIP  ${PYTHON_LIBRARY_DIRS}  PYTHON_LIBRARY_DIRS )
MESSAGE( "PYTHON library dirs: ${PYTHON_LIBRARY_DIRS}" )

execute_process( COMMAND  ${PYTHON_CONFIG_EXECUTABLE} --libs OUTPUT_VARIABLE PYTHON_LIBS )
string( REPLACE "-l"  ""  PYTHON_LIBRARIES  ${PYTHON_LIBS}  )
string( STRIP  ${PYTHON_LIBRARIES}  PYTHON_LIBRARIES )
set( PYTHON_LIBRARIES ${PYTHON_LIBRARIES} )
MESSAGE( "PYTHON libraries: ${PYTHON_LIBRARIES}" )


