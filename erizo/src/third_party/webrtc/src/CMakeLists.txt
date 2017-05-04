cmake_minimum_required(VERSION 2.6)
find_package(PkgConfig)
#functions
function(test_lib LIB_NAME)
  if (${LIB_NAME} MATCHES "^.*-NOTFOUND")
    message(FATAL_ERROR "lib not found: " ${LIB_NAME} " check README")
    return()
  endif(${LIB_NAME} MATCHES "^.*-NOTFOUND")
endfunction(test_lib)
project (WEBRTC)

set (WEBRTC_SOURCE ${CMAKE_CURRENT_SOURCE_DIR})

set(CMAKE_CXX_FLAGS "-g -Wall -std=c++11 -DWEBRTC_POSIX")

IF(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}  -fno-stack-protector -DWEBRTC_MAC")
ELSE()
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC -DWEBRTC_LINUX")
ENDIF(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")

include_directories("${WEBRTC_SOURCE_DIR}" "${THIRD_PARTY_INCLUDE}")

file(GLOB_RECURSE WEBRTC_SOURCES "${WEBRTC_SOURCE_DIR}/*.c" "${WEBRTC_SOURCE_DIR}/*.cpp" "${WEBRTC_SOURCE_DIR}/*.cc")
file(GLOB_RECURSE WEBRTC_HEADERS "${WEBRTC_SOURCE_DIR}/*.h")

add_library(webrtc STATIC ${WEBRTC_SOURCES} )

target_include_directories(webrtc PUBLIC ${WEBRTC_SOURCE_DIR})

target_link_libraries(webrtc ${LOG})
