CMAKE_MINIMUM_REQUIRED(VERSION 2.6)

INCLUDE_DIRECTORIES ( ${CMAKE_SOURCE_DIR}/zypp-ng/include  ${CMAKE_SOURCE_DIR}/util/progressbar/include ${CMAKE_BINARY_DIR}/zypp-ng ${GLIBMM2_INCLUDE_DIRS} ${ZYPP_INCLUDE_DIR} )

add_executable( "zypp-ng-download" main.cpp )
TARGET_LINK_LIBRARIES( "zypp-ng-download" ${PROJECT_NAME} progressbar )
