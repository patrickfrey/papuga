cmake_minimum_required( VERSION 2.8 FATAL_ERROR )

# --------------------------------------
# PHP
# --------------------------------------
if (SYSTEM_LINUX)
	if (SYSTEM_ARCHLINUX)
		find_program( PHP7_CONFIG_EXECUTABLE  NAMES  "php-config" )
		find_program( PHP7_EXECUTABLE NAMES  "php" )
	else (SYSTEM_ARCHLINUX)
		find_program( PHP7_CONFIG_EXECUTABLE  NAMES  "php-config7.1" "php-config7.0" "php-config7" )
		find_program( PHP7_EXECUTABLE NAMES  "php7.1" "php7.0" "php7" )
	endif (SYSTEM_ARCHLINUX)
else (SYSTEM_LINUX)
	find_program( PHP7_CONFIG_EXECUTABLE  NAMES  "php-config" )
	find_program( PHP7_EXECUTABLE NAMES  "php" )
endif (SYSTEM_LINUX)

MESSAGE( STATUS "PHP 7.x php-config executable:  ${PHP7_CONFIG_EXECUTABLE}" )
MESSAGE( STATUS "PHP 7.x php executable: ${PHP7_EXECUTABLE}" )

execute_process( COMMAND  ${PHP7_CONFIG_EXECUTABLE} --includes  OUTPUT_VARIABLE  PHP7_INCLUDES )
string( REPLACE "-I"  ""  PHP7_INCLUDE_DIRS  ${PHP7_INCLUDES}  )
string( STRIP  ${PHP7_INCLUDE_DIRS}  PHP7_INCLUDE_DIRS )
string( REPLACE " "  ";"  PHP7_INCLUDE_DIRS  ${PHP7_INCLUDE_DIRS}  )
MESSAGE( STATUS "PHP 7.x include dirs: ${PHP7_INCLUDE_DIRS}" )

execute_process( COMMAND  ${PHP7_CONFIG_EXECUTABLE} --ldflags  OUTPUT_VARIABLE PHP7_LDFLAGS )
string( REGEX MATCHALL "-L[^ ]*" PHP7_LDFLAGS  "${PHP7_LDFLAGS}" )
string( REPLACE "-L"  " "  PHP7_LIBRARY_DIRS  "${PHP7_LDFLAGS}"  )
string( STRIP  "${PHP7_LIBRARY_DIRS}"  PHP7_LIBRARY_DIRS )
separate_arguments( PHP7_LIBRARY_DIRS)
MESSAGE( STATUS "PHP 7.x library dirs: ${PHP7_LIBRARY_DIRS}" )

execute_process( COMMAND  ${PHP7_CONFIG_EXECUTABLE} --libs OUTPUT_VARIABLE PHP7_LIBS )
string( REGEX MATCHALL "-l[^ ]*" PHP7_LIBS  "${PHP7_LIBS}" )
string( REPLACE "-l"  " "  PHP7_LIBRARIES  ${PHP7_LIBS}  )
string( STRIP  ${PHP7_LIBRARIES}  PHP7_LIBRARIES )
separate_arguments( PHP7_LIBRARIES)
MESSAGE( STATUS "PHP 7.x libraries: ${PHP7_LIBRARIES}" )

execute_process( COMMAND  ${PHP7_CONFIG_EXECUTABLE} --extension-dir  OUTPUT_VARIABLE  PHP7_EXTENSION_DIR )
string( STRIP  "${PHP7_EXTENSION_DIR}"  PHP7_EXTENSION_DIR )
MESSAGE( STATUS "PHP 7.x extension dir: ${PHP7_EXTENSION_DIR}" )

execute_process( COMMAND  ${PHP7_EXECUTABLE}  ${PROJECT_SOURCE_DIR}/cmake/getPhpIniFile.php  OUTPUT_VARIABLE  PHP7_INI_FILE )
string( STRIP  "${PHP7_INI_FILE}"  PHP7_INI_FILE )
MESSAGE( STATUS "PHP 7.x  .ini file: ${PHP7_INI_FILE}" )
