CMAKE_MINIMUM_REQUIRED(VERSION 2.6)
PROJECT( zypp-ng C CXX )

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)

IF ( ZYPP_SOURCE )

  IF ( NOT ZYPP_BUILDDIR )
    message ( FATAL_ERROR "Define ZYPP_BUILDDIR to specify the binary location" )
  ENDIF()

  #we force the cache entries to make sure FindZypp.cmake does not reset them or they are not set already by a previous build
  SET( ZYPP_INCLUDE_DIR "${ZYPP_SOURCE}" CACHE STRING "" FORCE )
  SET( ZYPP_LIBRARY "${ZYPP_BUILDDIR}/zypp/libzypp.so" CACHE STRING "" FORCE )

  SET( CMAKE_MODULE_PATH
    ${ZYPP_SOURCE}/cmake/modules/
    ${DEPTESTOMATIC_SOURCE_DIR}/cmake/modules
    ${CMAKE_MODULE_PATH}
  )
ELSE()
  SET( CMAKE_MODULE_PATH
    ${ZYPP_PREFIX}/share/cmake/Modules
    ${DEPTESTOMATIC_SOURCE_DIR}/cmake/modules
    ${CMAKE_MODULE_PATH}
  )
ENDIF()

add_subdirectory( zypp-ng )
add_subdirectory( tests )
add_subdirectory( util/progressbar )
