cmake_minimum_required( VERSION 2.8 FATAL_ERROR )

# --------------------------------------
# PYTHON
# --------------------------------------

foreach( EXECNAME "python3.6" "python3.5" "python3.4" "python3.3" "python3.2" "python3.1" "python3" )
find_program( PYTHON_EXECUTABLE  NAMES  "${EXECNAME}" )
if ( PYTHON_EXECUTABLE )
break()
endif ( PYTHON_EXECUTABLE )		
endforeach( EXECNAME )
 
set(Python_ADDITIONAL_VERSIONS 3.6 3.5 3.4 3.3 3.2 3.1)
if (NOT PYTHON_EXECUTABLE)
find_package( PythonInterp 3 REQUIRED)
endif (NOT PYTHON_EXECUTABLE)

find_package( PythonLibs 3 REQUIRED)
if (${PYTHONLIBS_FOUND})
Message( STATUS "Python 3.x package found" )
MESSAGE( "Python 3.x executable: ${PYTHON_EXECUTABLE}" )
MESSAGE( "Python 3.x library: ${PYTHON_LIBRARY}" )
MESSAGE( "Python 3.x includes: ${PYTHON_INCLUDE_DIRS}" )
else  (${PYTHONLIBS_FOUND})
Message( FATAL_ERROR "Python 3.x package not found" )
endif (${PYTHONLIBS_FOUND})

set( PYTHON3_EXECUTABLE ${PYTHON_EXECUTABLE} )
set( PYTHON3_INCLUDE_DIRS ${PYTHON_INCLUDE_DIRS} )
set( PYTHON3_LIBRARIES  ${PYTHON_LIBRARY} )
set( PYTHON3_LIBRARY_DIRS "" )
execute_process( COMMAND  ${PYTHON3_EXECUTABLE} ${CMAKE_CURRENT_LIST_DIR}/sitepackages.py OUTPUT_VARIABLE PYTHON3_SITE_PACKAGES )
string( STRIP  ${PYTHON3_SITE_PACKAGES}  PYTHON3_SITE_PACKAGES )
MESSAGE( "Python3 site packages: ${PYTHON3_SITE_PACKAGES}" )


