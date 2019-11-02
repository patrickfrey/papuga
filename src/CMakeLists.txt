cmake_minimum_required( VERSION 2.8 FATAL_ERROR )

# --------------------------------------
# SOURCES AND INCLUDES
# --------------------------------------
set( source_files
	allocator.c
	typedefs.c
	serialization.c
	serialization_json.cpp
	serialization_xml.cpp
	callResult.c
	callArgs.c
	errors.c
	encoding.c
	uriEncode.cpp
	${CMAKE_CURRENT_BINARY_DIR}/internationalization.c
	stack.c
	errors.cpp
	valueVariant.cpp
	valueVariant.c
	valueVariant_markup_base.cpp
	valueVariant_markup.cpp
	serialization.cpp
)

set( source_files_request
	request.cpp
	requestHandler.cpp
	requestResult.cpp
	requestAutomaton.cpp
	requestParser.c
	requestParser.cpp
	requestParser_json.cpp
	requestParser_xml.cpp
	requestParser_utils.c
	requestParser_tostring.cpp
	schemaDescription.cpp
)

include_directories(
	"${Intl_INCLUDE_DIRS}"
	"${PROJECT_SOURCE_DIR}/include"
	"${PROJECT_SOURCE_DIR}/3rdParty/textwolf/include"
	"${PROJECT_SOURCE_DIR}/3rdParty/cjson/include"
	"${PAPUGA_INCLUDE_DIRS}"
)

link_directories(
	"${PROJECT_SOURCE_DIR}/3rdParty/cjson/src"
)

# ------------------------------
# LIBRARIES
# ------------------------------
add_library( papuga_devel STATIC ${source_files} )
target_link_libraries( papuga_devel papuga_cjson ${Intl_LIBRARIES} )

add_library( papuga_request_devel STATIC ${source_files_request} )
target_link_libraries( papuga_request_devel papuga_devel papuga_cjson ${Intl_LIBRARIES} )

add_library( papuga_doc_gen SHARED libpapuga_doc_gen.cpp )
target_link_libraries( papuga_doc_gen papuga_gen_utils ${Intl_LIBRARIES} )

set_target_properties(
    papuga_doc_gen
    PROPERTIES
    DEBUG_POSTFIX "${CMAKE_DEBUG_POSTFIX}"
    SOVERSION "${PAPUGA_MAJOR_VERSION}.${PAPUGA_MINOR_VERSION}"
    VERSION ${PAPUGA_VERSION}
)

# -------------------------------------------
# DOC GENERATOR
# -------------------------------------------
add_executable( papugaDoc papugaDoc.cpp )
target_link_libraries( papugaDoc papuga_doc_gen ${CMAKE_DL_LIBS} ${Intl_LIBRARIES} papuga_gen_utils )

# ------------------------------
# INSTALLATION
# ------------------------------
install( TARGETS papugaDoc
           RUNTIME DESTINATION bin )

install( TARGETS papuga_doc_gen
           LIBRARY DESTINATION ${LIB_INSTALL_DIR}/papuga )

install( TARGETS papuga_request_devel
           ARCHIVE DESTINATION ${LIB_INSTALL_DIR}/papuga )

install( TARGETS papuga_devel
           ARCHIVE DESTINATION ${LIB_INSTALL_DIR}/papuga )

