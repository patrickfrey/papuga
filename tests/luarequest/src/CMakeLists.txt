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

add_executable( testLuaRequest testLuaRequest.cpp )
target_link_libraries( testLuaRequest papuga_devel papuga_request_devel papuga_lua_dev ${Boost_LIBRARIES} ${Intl_LIBRARIES} )

