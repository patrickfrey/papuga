# -----------------------------------------------------------------------------------------------
# Defines the flags for compiler and linker and some build environment settings
# -----------------------------------------------------------------------------------------------
IF (CPP_LANGUAGE_VERSION STREQUAL "0x")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++0x")
ELSEIF (CPP_LANGUAGE_VERSION STREQUAL "11")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
ELSEIF (CPP_LANGUAGE_VERSION STREQUAL "14")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14")
ELSEIF (CPP_LANGUAGE_VERSION STREQUAL "17")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17")
ELSE (CPP_LANGUAGE_VERSION STREQUAL "0x")
if (HAVE_CXX11)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
endif (HAVE_CXX11)
ENDIF (CPP_LANGUAGE_VERSION STREQUAL "0x")

IF( CMAKE_CXX_FLAGS MATCHES "[-]std=c[+][+](11|14|17)" )
set( STRUS_CXX_STD_11 TRUE )
ENDIF( CMAKE_CXX_FLAGS MATCHES "[-]std=c[+][+](11|14|17)" )

IF (C_LANGUAGE_VERSION STREQUAL "99")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c99")
ELSEIF (C_LANGUAGE_VERSION STREQUAL "90")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c90")
ELSEIF (C_LANGUAGE_VERSION STREQUAL "17")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c17")
ELSE (C_LANGUAGE_VERSION STREQUAL "99")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c99")
ENDIF (C_LANGUAGE_VERSION STREQUAL "99")

MESSAGE( STATUS "Debug postfix: '${CMAKE_DEBUG_POSTFIX}'" )
IF(CMAKE_BUILD_TYPE MATCHES RELEASE)
    set( VISIBILITY_FLAGS "-fvisibility=hidden" )
ELSE(CMAKE_BUILD_TYPE MATCHES RELEASE)
IF("${CMAKE_CXX_COMPILER_ID}" MATCHES "[cC]lang")
    set( VISIBILITY_FLAGS "-fstandalone-debug" )
ELSE()
    set( VISIBILITY_FLAGS "" )
ENDIF()
ENDIF(CMAKE_BUILD_TYPE MATCHES RELEASE)

set_property(GLOBAL PROPERTY rule_launch_compile ccache)
set_property(GLOBAL PROPERTY rule_launch_link ccache)

if("${CMAKE_CXX_COMPILER_ID}" MATCHES "GNU")
set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC -Wall -Wshadow -pedantic -Wfatal-errors ${VISIBILITY_FLAGS}" )
set( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fPIC -Wall -Wshadow -pedantic -Wfatal-errors" )
endif()
if("${CMAKE_CXX_COMPILER_ID}" MATCHES "[cC]lang")
set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC -Wall -Wshadow -pedantic -Wfatal-errors ${VISIBILITY_FLAGS} -Wno-unused-private-field" )
set( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fPIC -Wall -Wshadow -pedantic -Wfatal-errors" )
endif()

if(WIN32)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /D_WIN32_WINNT=0x0504")
else()
set( CMAKE_THREAD_PREFER_PTHREAD TRUE )
find_package( Threads REQUIRED )
if( NOT APPLE )
if( CMAKE_USE_PTHREADS_INIT)
set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread" )
set( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -pthread" )
endif()
endif( NOT APPLE )
endif()

string( REGEX  MATCH  "\\-std\\=c\\+\\+[1-7]+"  SET_STD_CXX_11 "${CMAKE_CXX_FLAGS}" )
if( SET_STD_CXX_11 )
include( cmake/CXX11Features.cmake )
endif( SET_STD_CXX_11 )
