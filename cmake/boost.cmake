set(Boost_USE_MULTITHREADED ON)
set(BOOST_INCLUDEDIR "${CMAKE_INSTALL_PREFIX}/include/papuga")
set(BOOST_LIBRARYDIR "${CMAKE_INSTALL_PREFIX}/${LIB_INSTALL_DIR}/papuga")
find_package( Boost 1.53.0 COMPONENTS atomic QUIET)
if( Boost_ATOMIC_FOUND )
	if (WIN32)
		find_package( Boost 1.53.0 REQUIRED COMPONENTS thread-mt system date_time atomic-mt)
	else()
		find_package( Boost 1.53.0 REQUIRED COMPONENTS thread system date_time atomic)
	endif()
else()
	if (WIN32)
		find_package( Boost 1.53.0 REQUIRED COMPONENTS thread-mt system date_time)
	else()
		find_package( Boost 1.53.0 REQUIRED COMPONENTS thread system date_time)
	endif()
endif()

MESSAGE( STATUS "Boost includes: ${Boost_INCLUDE_DIRS}" )
MESSAGE( STATUS "Boost library directories: ${Boost_LIBRARY_DIRS}" )
MESSAGE( STATUS "Boost libraries: ${Boost_LIBRARIES}" )
