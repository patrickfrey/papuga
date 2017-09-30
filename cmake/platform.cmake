# Variables initialized with this module
set( SYSTEM_WINDOWS FALSE )
set( SYSTEM_MACOSX FALSE )
set( SYSTEM_LINUX FALSE )
set( SYSTEM_ARCHLINUX FALSE )
set( SYSTEM_DEBIAN FALSE )
set( SYSTEM_REDHAT FALSE )
set( SYSTEM_BSD FALSE )

# Detect 32/64 bit
if( CMAKE_SIZEOF_VOID_P EQUAL 8 )
    MESSAGE( "64 bits compiler detected" )
    SET( SYTEM_64BIT TRUE )
else( CMAKE_SIZEOF_VOID_P EQUAL 8 ) 
    MESSAGE( "32 bits compiler detected" )
    SET( SYTEM_32BIT TRUE )
endif( CMAKE_SIZEOF_VOID_P EQUAL 8 )

# Detect OS
if(WIN32)
    MESSAGE( "System Windows detected" )
    set( SYSTEM_WINDOWS TRUE )

elseif (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    MESSAGE( "System OSX detected" )
    set( SYSTEM_MACOSX TRUE )

elseif ( ${CMAKE_SYSTEM_NAME} MATCHES "BSD" )
    MESSAGE( "System BSD detected" )
    set( SYSTEM_BSD TRUE )

elseif ( ${CMAKE_SYSTEM_NAME} MATCHES "Linux" )
    set ( SYSTEM_LINUX TRUE )
    if ( EXISTS /etc/debian_version )
        MESSAGE( "System Linux (Debian) detected" )
        set( SYSTEM_DEBIAN TRUE )

    elseif ( EXISTS /etc/arch-release )
        MESSAGE( "System Linux (Archlinux) detected" )
        set( SYSTEM_ARCHLINUX TRUE )
    
    elseif ( EXISTS /etc/redhat-release )
        MESSAGE( "System Linux (Redhat) detected" )
        set( SYSTEM_REDHAT TRUE )

    else ( EXISTS /etc/redhat-release )
        MESSAGE( "System Linux (unknown) detected" )
    endif ( EXISTS /etc/debian_version )

endif (WIN32)


