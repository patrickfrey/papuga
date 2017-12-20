cmake_minimum_required( VERSION 2.8 FATAL_ERROR )

# --------------------------------------
# PYTHON 3.x
# --------------------------------------
find_program( PYTHON_EXECUTABLE_ROOT NAMES  "python" )
if (NOT PYTHON_EXECUTABLE_ROOT)
find_program( PYTHON_EXECUTABLE_ROOT NAMES  "python3" )
if (PYTHON_EXECUTABLE_ROOT)
string( LENGTH "${PYTHON_EXECUTABLE_ROOT}"  PYTHON_EXECUTABLE_ROOT_LEN )
math( EXPR PYTHON_EXECUTABLE_ROOT_LEN "${PYTHON_EXECUTABLE_ROOT_LEN} - 1")
string( SUBSTRING  "${PYTHON_EXECUTABLE_ROOT}"  0  ${PYTHON_EXECUTABLE_ROOT_LEN}  PYTHON_EXECUTABLE_ROOT )
endif( PYTHON_EXECUTABLE_ROOT ) 
endif( NOT PYTHON_EXECUTABLE_ROOT ) 

if (PYTHON_EXECUTABLE_ROOT)
foreach( PYVERSION "3.9" "3.8" "3.7" "3.6" "3.5" "3.4" "3.3" "3.2" "3.1" "3.0" "3" "")
execute_process( COMMAND  ${PYTHON_EXECUTABLE_ROOT}${PYVERSION}  ${CMAKE_CURRENT_LIST_DIR}/version.py
			   RESULT_VARIABLE  RET_PYVERSION 
			   ERROR_VARIABLE  STDERR_PYVERSION
			   OUTPUT_VARIABLE  STDOUT_PYVERSION 
	                   ERROR_STRIP_TRAILING_WHITESPACE
			   OUTPUT_STRIP_TRAILING_WHITESPACE )
set( OUT_PYVERSION "${STDERR_PYVERSION}${STDOUT_PYVERSION}" )
if( ${RET_PYVERSION} STREQUAL "" OR ${RET_PYVERSION} STREQUAL "0" )
# MESSAGE( STATUS "Call '${PYTHON_EXECUTABLE_ROOT}${PYVERSION}  ${CMAKE_CURRENT_LIST_DIR}/version.py'  result ${OUT_PYVERSION}" )
string( SUBSTRING  "${OUT_PYVERSION}"  0  8  PYVERSIONSTR )
if( ${PYVERSIONSTR}  STREQUAL  "Python 3" )
set( PYTHON3_EXECUTABLE  "${PYTHON_EXECUTABLE_ROOT}${PYVERSION}" )
set( PYTHON3_VERSION  "${OUT_PYVERSION}" )
break()
endif( ${PYVERSIONSTR}  STREQUAL  "Python 3" )
endif( ${RET_PYVERSION} STREQUAL "" OR ${RET_PYVERSION} STREQUAL "0")
endforeach( PYVERSION "3.9" "3.8" "3.7" "3.6" "3.5" "3.4" "3.3" "3.2" "3.1" "3.0" "3" "" )
endif( PYTHON_EXECUTABLE_ROOT ) 

if( PYTHON3_EXECUTABLE )
execute_process( COMMAND  ${PYTHON3_EXECUTABLE} ${CMAKE_CURRENT_LIST_DIR}/sitepackages.py
			   OUTPUT_VARIABLE PYTHON3_SITE_PACKAGES
			   RESULT_VARIABLE  RET_CFG )
if( ${RET_CFG} STREQUAL "" OR ${RET_CFG} STREQUAL "0" )
string( STRIP  ${PYTHON3_SITE_PACKAGES}  PYTHON3_SITE_PACKAGES )
MESSAGE( STATUS "Python 3.x site packages: ${PYTHON3_SITE_PACKAGES}" )
else( ${RET_CFG} STREQUAL "" OR ${RET_CFG} STREQUAL "0" )
MESSAGE( SEND_ERROR "Failed to call ${CMAKE_CURRENT_LIST_DIR}/sitepackages.py (${RET_CFG})")
endif( ${RET_CFG} STREQUAL "" OR ${RET_CFG} STREQUAL "0" )

set( PYTHON3_CONFIG_EXECUTABLE  "${PYTHON3_EXECUTABLE}-config" )
MESSAGE( STATUS "Python-config executable:  ${PYTHON3_CONFIG_EXECUTABLE}" )

execute_process( COMMAND  ${PYTHON3_CONFIG_EXECUTABLE} --includes  OUTPUT_VARIABLE  PYTHON3_INCLUDES  RESULT_VARIABLE  RET_CFG )
if( ${RET_CFG} STREQUAL "" OR ${RET_CFG} STREQUAL "0" )
string( REPLACE "-I"  ""  PYTHON3_INCLUDE_DIRS  ${PYTHON3_INCLUDES}  )
string( STRIP  ${PYTHON3_INCLUDE_DIRS}  PYTHON3_INCLUDE_DIRS )
string( REPLACE " "  ";"  PYTHON3_INCLUDE_DIRS  ${PYTHON3_INCLUDE_DIRS}  )
MESSAGE( STATUS "Python 3.x include dirs: ${PYTHON3_INCLUDE_DIRS}" )
else( ${RET_CFG} STREQUAL "" OR ${RET_CFG} STREQUAL "0" )
MESSAGE( SEND_ERROR "Python program ${PYTHON3_CONFIG_EXECUTABLE} failed (${RET_CFG})")
endif( ${RET_CFG} STREQUAL "" OR ${RET_CFG} STREQUAL "0" )

execute_process( COMMAND  ${PYTHON3_CONFIG_EXECUTABLE} --ldflags  OUTPUT_VARIABLE PYTHON3_LDFLAGS RESULT_VARIABLE  RET_CFG )
if( ${RET_CFG} STREQUAL "" OR ${RET_CFG} STREQUAL "0" )
string( REGEX MATCHALL "-L[^ ]*" PYTHON3_LINKDIRDEFS  "${PYTHON3_LDFLAGS}" )
string( REPLACE "-L"  " "  PYTHON3_LIBRARY_DIRS  "${PYTHON3_LINKDIRDEFS}"  )
string( STRIP  "${PYTHON3_LIBRARY_DIRS}"  PYTHON3_LIBRARY_DIRS )
separate_arguments( PYTHON3_LIBRARY_DIRS)
MESSAGE( STATUS "Python 3.x library dirs: ${PYTHON3_LIBRARY_DIRS}" )
else( ${RET_CFG} STREQUAL "" OR ${RET_CFG} STREQUAL "0" )
MESSAGE( SEND_ERROR "Python program ${PYTHON3_CONFIG_EXECUTABLE} failed (${RET_CFG})")
endif( ${RET_CFG} STREQUAL "" OR ${RET_CFG} STREQUAL "0" )

execute_process( COMMAND  ${PYTHON3_CONFIG_EXECUTABLE} --libs OUTPUT_VARIABLE PYTHON3_LIBS RESULT_VARIABLE  RET_CFG )
if( ${RET_CFG} STREQUAL "" OR ${RET_CFG} STREQUAL "0" )
string( REGEX MATCHALL "-l[^ ]*" PYTHON3_LIBS  "${PYTHON3_LIBS}" )
string( REPLACE "-l"  " "  PYTHON3_LIBRARIES  ${PYTHON3_LIBS}  )
string( STRIP  ${PYTHON3_LIBRARIES}  PYTHON3_LIBRARIES )
separate_arguments( PYTHON3_LIBRARIES)
MESSAGE( STATUS "Python 3.x libraries: ${PYTHON3_LIBRARIES}" )
else( ${RET_CFG} STREQUAL "" OR ${RET_CFG} STREQUAL "0" )
MESSAGE( SEND_ERROR "Python program ${PYTHON3_CONFIG_EXECUTABLE} failed (${RET_CFG})")
endif( ${RET_CFG} STREQUAL "" OR ${RET_CFG} STREQUAL "0" )

if( PYTHON3_VERSION )
MESSAGE( STATUS "Python 3.x version ${PYTHON3_VERSION}" )
else( PYTHON3_VERSION )
MESSAGE( STATUS "No python 3.x package found" )
endif( PYTHON3_VERSION )

else( PYTHON3_EXECUTABLE )
MESSAGE( STATUS "Unable to relocate Python 3.x interpreter" )
endif( PYTHON3_EXECUTABLE )

