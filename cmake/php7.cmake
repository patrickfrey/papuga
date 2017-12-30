cmake_minimum_required( VERSION 2.8 FATAL_ERROR )

# --------------------------------------
# PHP
# --------------------------------------
if (APPLE)
execute_process( COMMAND  brew  --prefix  php70
			   RESULT_VARIABLE  RET_PHP_PATH
			   OUTPUT_VARIABLE  PHP_INSTALL_PATH
			   OUTPUT_STRIP_TRAILING_WHITESPACE )
if( ${RET_PHP_PATH} STREQUAL "" OR ${RET_PHP_PATH} STREQUAL "0" )
MESSAGE( STATUS "Installation path of php7: '${PHP_INSTALL_PATH}' " )
find_program( PHP_EXECUTABLE_ROOT NAMES  "php7"
			HINTS ${PHP_INSTALL_PATH}/bin ${PHP_INSTALL_PATH}/sbin ${PHP_INSTALL_PATH}
			NO_CMAKE_ENVIRONMENT_PATH
			NO_CMAKE_PATH
			NO_SYSTEM_ENVIRONMENT_PATH
			NO_CMAKE_SYSTEM_PATH )
if( NOT PHP_EXECUTABLE_ROOT )
find_program( PHP_EXECUTABLE_ROOT NAMES  "php"
			HINTS ${PHP_INSTALL_PATH}/bin ${PHP_INSTALL_PATH}/sbin ${PHP_INSTALL_PATH}
			NO_CMAKE_ENVIRONMENT_PATH
			NO_CMAKE_PATH
			NO_SYSTEM_ENVIRONMENT_PATH
			NO_CMAKE_SYSTEM_PATH )
endif( NOT PHP_EXECUTABLE_ROOT )
else( ${RET_PHP_PATH} STREQUAL "" OR ${RET_PHP_PATH} STREQUAL "0" )
MESSAGE( STATUS "Call 'brew  --prefix  php70' returns '${RET_PHP_PATH}' result '${PHP_INSTALL_PATH}' " )
find_program( PHP_EXECUTABLE_ROOT NAMES  "php7" )
endif( ${RET_PHP_PATH} STREQUAL "" OR ${RET_PHP_PATH} STREQUAL "0" )
else (APPLE)
find_program( PHP_EXECUTABLE_ROOT NAMES  "php7" )
endif (APPLE)

if( PHP_EXECUTABLE_ROOT )
MESSAGE( STATUS "Found php7 ${PHP_EXECUTABLE_ROOT}" )
endif( PHP_EXECUTABLE_ROOT ) 

if (NOT PHP_EXECUTABLE_ROOT)
foreach( PHPVERSION "7.9" "7.8" "7.7" "7.6" "7.5" "7.4" "7.3" "7.2" "7.1" "7.0" )
find_program( PHP_EXECUTABLE_ROOT NAMES  "php${PHPVERSION}" )
if (PHP_EXECUTABLE_ROOT)
MESSAGE( STATUS "Found php7.x ${PHP_EXECUTABLE_ROOT}" )
break()
endif (PHP_EXECUTABLE_ROOT)
endforeach( PHPVERSION "7.9" "7.8" "7.7" "7.6" "7.5" "7.4" "7.3" "7.2" "7.1" "7.0" )
endif (NOT PHP_EXECUTABLE_ROOT)

if( NOT PHP_EXECUTABLE_ROOT )
find_program( PHP_EXECUTABLE_ROOT NAMES  "php" )
MESSAGE( STATUS "Found php ${PHP_EXECUTABLE_ROOT}" )
endif( NOT PHP_EXECUTABLE_ROOT ) 

if( PHP_EXECUTABLE_ROOT )
string( REGEX REPLACE "[0-9\\.]+$" "" PHP_EXECUTABLE_ROOT ${PHP_EXECUTABLE_ROOT} )
endif( PHP_EXECUTABLE_ROOT )

if (PHP_EXECUTABLE_ROOT)
foreach( PHPVERSION "7.9" "7.8" "7.7" "7.6" "7.5" "7.4" "7.3" "7.2" "7.1" "7.0" "7" "")
execute_process( COMMAND  ${PHP_EXECUTABLE_ROOT}${PHPVERSION}  ${CMAKE_CURRENT_LIST_DIR}/version.php
			   RESULT_VARIABLE  RET_PHPVERSION 
			   ERROR_VARIABLE  STDERR_PHPVERSION
			   OUTPUT_VARIABLE  STDOUT_PHPVERSION 
	                   ERROR_STRIP_TRAILING_WHITESPACE
			   OUTPUT_STRIP_TRAILING_WHITESPACE )
set( OUT_PHPVERSION "${STDERR_PHPVERSION}${STDOUT_PHPVERSION}" )
if( ${RET_PHPVERSION} STREQUAL "" OR ${RET_PHPVERSION} STREQUAL "0" )
string( SUBSTRING  "${OUT_PHPVERSION}"  0  1  PHPVERSIONSTR )
if( ${PHPVERSIONSTR}  STREQUAL  "7" )
set( PHP7_EXECUTABLE  "${PHP_EXECUTABLE_ROOT}${PHPVERSION}" )
set( PHP7_CONFIG_EXECUTABLE  "${PHP_EXECUTABLE_ROOT}-config${PHPVERSION}" )
string( SUBSTRING  "${OUT_PHPVERSION}"  0  3  PHP7_VERSION )

execute_process( COMMAND  ${PHP7_CONFIG_EXECUTABLE} --version
			   RESULT_VARIABLE  RET_CFG_PHPVERSION
			   ERROR_VARIABLE  STDERR_CFG_PHPVERSION
			   OUTPUT_VARIABLE  STDOUT_CFG_PHPVERSION 
	                   ERROR_STRIP_TRAILING_WHITESPACE
			   OUTPUT_STRIP_TRAILING_WHITESPACE )
set( OUT_CFG_PHPVERSION "${STDERR_CFG_PHPVERSION}${STDOUT_CFG_PHPVERSION}" )
MESSAGE( STATUS "Call '${PHP7_CONFIG_EXECUTABLE} --version'  error '${RET_CFG_PHPVERSION}' result '${OUT_CFG_PHPVERSION}'" )
if( ${RET_CFG_PHPVERSION} STREQUAL "" OR ${RET_CFG_PHPVERSION} STREQUAL "0" )
string( SUBSTRING  "${OUT_PHPVERSION}"  0  1  PHPCFGVERSIONSTR )
if( ${PHPCFGVERSIONSTR}  STREQUAL  "7" )
set( PHP7_VERSION  "${OUT_PHPVERSION}" )
break()
endif( ${PHPCFGVERSIONSTR}  STREQUAL  "7" )
endif( ${RET_CFG_PHPVERSION} STREQUAL "" OR ${RET_CFG_PHPVERSION} STREQUAL "0" )

endif( ${PHPVERSIONSTR}  STREQUAL  "7" )
endif( ${RET_PHPVERSION} STREQUAL "" OR ${RET_PHPVERSION} STREQUAL "0")
endforeach( PHPVERSION "7.9" "7.8" "7.7" "7.6" "7.5" "7.4" "7.3" "7.2" "7.1" "7.0" "7" "" )
endif( PHP_EXECUTABLE_ROOT ) 

if( PHP7_EXECUTABLE AND PHP7_CONFIG_EXECUTABLE )
MESSAGE( STATUS "PHP 7.x executable: ${PHP7_EXECUTABLE}" )
MESSAGE( STATUS "PHP 7.x config executable:  ${PHP7_CONFIG_EXECUTABLE}" )

execute_process( COMMAND  ${PHP7_CONFIG_EXECUTABLE} --includes
			   RESULT_VARIABLE  RET_CFG
			   OUTPUT_VARIABLE  PHP7_INCLUDES )
if( ${RET_CFG} STREQUAL "" OR ${RET_CFG} STREQUAL "0" )
string( REPLACE "-I"  ""  PHP7_INCLUDE_DIRS  ${PHP7_INCLUDES}  )
string( STRIP  ${PHP7_INCLUDE_DIRS}  PHP7_INCLUDE_DIRS )
string( REPLACE " "  ";"  PHP7_INCLUDE_DIRS  ${PHP7_INCLUDE_DIRS}  )
MESSAGE( STATUS "PHP 7.x include dirs: ${PHP7_INCLUDE_DIRS}" )
else( ${RET_CFG} STREQUAL "" OR ${RET_CFG} STREQUAL "0" )
message( STATUS "Call '${PHP7_CONFIG_EXECUTABLE} --includes' returns '${RET_CFG}' " )
endif( ${RET_CFG} STREQUAL "" OR ${RET_CFG} STREQUAL "0" )

execute_process( COMMAND  ${PHP7_CONFIG_EXECUTABLE} --ldflags 
			   RESULT_VARIABLE  RET_CFG
			   OUTPUT_VARIABLE PHP7_LDFLAGS )
if( ${RET_CFG} STREQUAL "" OR ${RET_CFG} STREQUAL "0" )
string( REGEX MATCHALL "-L[^ ]*" PHP7_LDFLAGS  "${PHP7_LDFLAGS}" )
string( REPLACE "-L"  " "  PHP7_LIBRARY_DIRS  "${PHP7_LDFLAGS}"  )
string( STRIP  "${PHP7_LIBRARY_DIRS}"  PHP7_LIBRARY_DIRS )
separate_arguments( PHP7_LIBRARY_DIRS)
MESSAGE( STATUS "PHP 7.x library dirs: ${PHP7_LIBRARY_DIRS}" )
else( ${RET_CFG} STREQUAL "" OR ${RET_CFG} STREQUAL "0" )
message( STATUS "Call '${PHP7_CONFIG_EXECUTABLE} --ldflags' returns '${RET_CFG}' " )
endif( ${RET_CFG} STREQUAL "" OR ${RET_CFG} STREQUAL "0" )

execute_process( COMMAND  ${PHP7_CONFIG_EXECUTABLE} --libs RESULT_VARIABLE  RET_CFG  OUTPUT_VARIABLE PHP7_LIBS )
if( ${RET_CFG} STREQUAL "" OR ${RET_CFG} STREQUAL "0" )
string( REGEX MATCHALL "-l[^ ]*" PHP7_LIBS  "${PHP7_LIBS}" )
string( REPLACE "-l"  " "  PHP7_LIBRARIES  ${PHP7_LIBS}  )
string( STRIP  ${PHP7_LIBRARIES}  PHP7_LIBRARIES )
separate_arguments( PHP7_LIBRARIES)
MESSAGE( STATUS "PHP 7.x libraries: ${PHP7_LIBRARIES}" )
else( ${RET_CFG} STREQUAL "" OR ${RET_CFG} STREQUAL "0" )
message( STATUS "Call '${PHP7_CONFIG_EXECUTABLE} --libs' returns '${RET_CFG}' " )
endif( ${RET_CFG} STREQUAL "" OR ${RET_CFG} STREQUAL "0" )

execute_process( COMMAND  ${PHP7_CONFIG_EXECUTABLE} --extension-dir  RESULT_VARIABLE  RET_CFG  OUTPUT_VARIABLE  PHP7_EXTENSION_DIR )
if( ${RET_CFG} STREQUAL "" OR ${RET_CFG} STREQUAL "0" )
string( STRIP  "${PHP7_EXTENSION_DIR}"  PHP7_EXTENSION_DIR )
MESSAGE( STATUS "PHP 7.x extension dir: ${PHP7_EXTENSION_DIR}" )
else( ${RET_CFG} STREQUAL "" OR ${RET_CFG} STREQUAL "0" )
message( STATUS "Call '${PHP7_CONFIG_EXECUTABLE} --extension-dir' returns '${RET_CFG}' " )
endif( ${RET_CFG} STREQUAL "" OR ${RET_CFG} STREQUAL "0" )

execute_process( COMMAND  ${PHP7_EXECUTABLE}  ${CMAKE_CURRENT_LIST_DIR}/getPhpIniFile.php
			  RESULT_VARIABLE  RET_CFG
			  OUTPUT_VARIABLE  PHP7_INI_FILE )
if( ${RET_CFG} STREQUAL "" OR ${RET_CFG} STREQUAL "0" )
string( STRIP  "${PHP7_INI_FILE}"  PHP7_INI_FILE )
MESSAGE( STATUS "PHP 7.x  .ini file: ${PHP7_INI_FILE}" )
else( ${RET_CFG} STREQUAL "" OR ${RET_CFG} STREQUAL "0" )
message( STATUS "Call '${PHP7_EXECUTABLE} ${CMAKE_CURRENT_LIST_DIR}/getPhpIniFile.php' returns '${RET_CFG}' " )
endif( ${RET_CFG} STREQUAL "" OR ${RET_CFG} STREQUAL "0" )

MESSAGE( STATUS "PHP 7.x  version: ${PHP7_VERSION}" )

else( PHP7_EXECUTABLE AND PHP7_CONFIG_EXECUTABLE )
MESSAGE( FATAL_ERROR "Unable to relocate PHP 7.x interpreter" )
endif( PHP7_EXECUTABLE AND PHP7_CONFIG_EXECUTABLE )
