# -------------------------------------------------------------------------------
# Defines the link policies
# -------------------------------------------------------------------------------
if(APPLE)
   set(CMAKE_MACOSX_RPATH ON)
   set( CMAKE_BUILD_WITH_INSTALL_RPATH FALSE )
   set( CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/${LIB_INSTALL_DIR}/papuga" )
   list( REMOVE_DUPLICATES CMAKE_INSTALL_RPATH )
   set( CMAKE_INSTALL_RPATH_USE_LINK_PATH FALSE )
else(APPLE)
    set( CMAKE_BUILD_WITH_INSTALL_RPATH FALSE ) 
    set( CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/${LIB_INSTALL_DIR}/papuga" )
    list( REMOVE_DUPLICATES CMAKE_INSTALL_RPATH )
    set( CMAKE_INSTALL_RPATH_USE_LINK_PATH FALSE )
    set( CMAKE_NO_BUILTIN_CHRPATH TRUE )
endif(APPLE)
message( STATUS "Set RPATH: ${CMAKE_INSTALL_RPATH}" ) 