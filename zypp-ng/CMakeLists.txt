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
  include/repo/refreshrepo.h
)

set(PRIVATE_HEADER
  include/core/private/base_p.h
  include/core/private/task_p.h
  include/core/private/context_p.h
  include/repo/private/refreshrepo_p.h
)

set(SOURCES
  src/core/base.cpp
  src/core/task.cpp
  src/core/context.cpp
  src/repo/refreshrepo.cpp
)

add_library( ${PROJECT_NAME} SHARED ${HEADER} ${PRIVATE_HEADER} ${SOURCES} )

TARGET_LINK_LIBRARIES( ${PROJECT_NAME} ${ZYPP_LIBRARY} )
TARGET_LINK_LIBRARIES( ${PROJECT_NAME} ${GLIBMM2_LIBRARIES} )

GENERATE_EXPORT_HEADER(
  zypp-ng
  PREFIX_NAME LIB
)
