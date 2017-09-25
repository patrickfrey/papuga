cmake_minimum_required( VERSION 2.8 FATAL_ERROR )

# --------------------------------------
# PYTHON
# --------------------------------------
find_program( PYTHON3_CONFIG_EXECUTABLE NAMES  "python3-config"  "python-config" )
MESSAGE( "Python-config executable:  ${PYTHON3_CONFIG_EXECUTABLE}" )

find_program( PYTHON3_EXECUTABLE NAMES  python3 "python3*" python)
MESSAGE( "Python3 executable: ${PYTHON3_EXECUTABLE}" )

execute_process( COMMAND  ${PYTHON3_CONFIG_EXECUTABLE} --includes  OUTPUT_VARIABLE  PYTHON3_INCLUDES )
string( REPLACE "-I"  ""  PYTHON3_INCLUDE_DIRS  ${PYTHON3_INCLUDES}  )
string( STRIP  ${PYTHON3_INCLUDE_DIRS}  PYTHON3_INCLUDE_DIRS )
string( REPLACE " "  ";"  PYTHON3_INCLUDE_DIRS  ${PYTHON3_INCLUDE_DIRS}  )
MESSAGE( "Python3 include dirs: ${PYTHON3_INCLUDE_DIRS}" )

execute_process( COMMAND  ${PYTHON3_CONFIG_EXECUTABLE} --ldflags  OUTPUT_VARIABLE PYTHON3_LDFLAGS )
string( REGEX MATCHALL "-L[^ ]*" PYTHON3_LINKDIRDEFS  "${PYTHON3_LDFLAGS}" )
string( REPLACE "-L"  " "  PYTHON3_LIBRARY_DIRS  "${PYTHON3_LINKDIRDEFS}"  )
string( STRIP  "${PYTHON3_LIBRARY_DIRS}"  PYTHON3_LIBRARY_DIRS )
separate_arguments( PYTHON3_LIBRARY_DIRS)
MESSAGE( "Python3 library dirs: ${PYTHON3_LIBRARY_DIRS}" )

execute_process( COMMAND  ${PYTHON3_CONFIG_EXECUTABLE} --libs OUTPUT_VARIABLE PYTHON3_LIBS )
string( REGEX MATCHALL "-l[^ ]*" PYTHON3_LIBS  "${PYTHON3_LIBS}" )
string( REPLACE "-l"  " "  PYTHON3_LIBRARIES  ${PYTHON3_LIBS}  )
string( STRIP  ${PYTHON3_LIBRARIES}  PYTHON3_LIBRARIES )
separate_arguments( PYTHON3_LIBRARIES)
MESSAGE( "Python3 libraries: ${PYTHON3_LIBRARIES}" )

execute_process( COMMAND  ${PYTHON3_EXECUTABLE} ${CMAKE_CURRENT_LIST_DIR}/sitepackages.py OUTPUT_VARIABLE PYTHON3_SITE_PACKAGES )
string( STRIP  ${PYTHON3_SITE_PACKAGES}  PYTHON3_SITE_PACKAGES )
MESSAGE( "Python3 site packages: ${PYTHON3_SITE_PACKAGES}" )


