cmake_minimum_required(VERSION 2.8 FATAL_ERROR)

include_directories(
	"${Boost_INCLUDE_DIRS}"
	"${Intl_INCLUDE_DIRS}"
	"${CMAKE_CURRENT_BINARY_DIR}/../../../include"
	"${PROJECT_SOURCE_DIR}/include"
)
link_directories(
	"${CMAKE_CURRENT_BINARY_DIR}/../../../src"
)

add_executable( testRequest testRequest.cpp document.cpp )
target_link_libraries( testRequest papuga_devel ${Boost_LIBRARIES} ${Intl_LIBRARIES})

