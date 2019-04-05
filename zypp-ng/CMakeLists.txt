INCLUDE(ZyppCommon)
INCLUDE(GenerateExportHeader)

FIND_PACKAGE( Zypp REQUIRED )
IF( ZYPP_FOUND )
  INCLUDE_DIRECTORIES(${ZYPP_INCLUDE_DIR})
ELSE( ZYPP_FOUND )
  MESSAGE( FATAL_ERROR "libzypp not found" )
ENDIF( ZYPP_FOUND )

include(FindPkgConfig)

#look for glibmm
pkg_check_modules ( GLIBMM2 glibmm-2.58 REQUIRED )
INCLUDE_DIRECTORIES ( ${GLIBMM2_INCLUDE_DIRS} ${CMAKE_SOURCE_DIR}/zypp-ng/include )
LINK_DIRECTORIES ( ${GLIBMM2_LIBRARY_DIRS} )

#boost asio
SET(Boost_USE_STATIC_LIBS OFF)
SET(Boost_USE_MULTITHREAD ON)
FIND_PACKAGE(Boost 1.66.0 REQUIRED COMPONENTS system thread regex)
IF(Boost_FOUND)
  INCLUDE_DIRECTORIES(${Boost_INCLUDE_DIRS})
  LINK_DIRECTORIES(${Boost_LIBRARY_DIRS})
ENDIF(Boost_FOUND)

#hide symbols by default
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_C_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN 1)

set(HEADER
  include/core/base.h
  include/core/zyppglobal.h
  include/core/idstring.h
  include/core/task.h
  include/core/signals.h
  include/core/context.h
  include/core/url.h
  include/repo/refreshrepo.h
  include/media/curl/curl.h
  include/media/curl/request.h
  include/media/curl/httprequesterror.h
)

set(PRIVATE_HEADER
  include/core/private/base_p.h
  include/core/private/task_p.h
  include/core/private/context_p.h
  include/repo/private/refreshrepo_p.h
  include/media/curl/private/curl_p.h
  include/media/curl/private/request_p.h
)

set(SOURCES
  src/core/base.cpp
  src/core/task.cpp
  src/core/context.cpp
  src/repo/refreshrepo.cpp
  src/media/curl/curl.cpp
  src/media/curl/request.cpp
  src/media/curl/httprequesterror.cpp
)

add_library( ${PROJECT_NAME} SHARED ${HEADER} ${PRIVATE_HEADER} ${SOURCES} )

TARGET_LINK_LIBRARIES( ${PROJECT_NAME} ${ZYPP_LIBRARY} )
TARGET_LINK_LIBRARIES( ${PROJECT_NAME} ${GLIBMM2_LIBRARIES} ${Boost_SYSTEM_LIBRARY} ${Boost_THREAD_LIBRARY} ${Boost_REGEX_LIBRARY} )

GENERATE_EXPORT_HEADER(
  zypp-ng
  PREFIX_NAME LIB
)
