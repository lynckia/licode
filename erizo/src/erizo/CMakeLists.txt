cmake_minimum_required(VERSION 2.8)
find_package(PkgConfig)
#functions
function(test_lib LIB_NAME)
 if (${LIB_NAME} MATCHES "^.*-NOTFOUND")
  message(FATAL_ERROR "lib not found: " ${LIB_NAME} " check README")
  return()
 endif(${LIB_NAME} MATCHES "^.*-NOTFOUND")
endfunction(test_lib)
project (ERIZO)

message("current source dir " ${CMAKE_CURRENT_SOURCE_DIR})
set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/")
set(ERIZO_VERSION_MAJOR 0)
set(ERIZO_VERSION_MINOR 1)
set(CMAKE_CXX_FLAGS "-g -Wall")

file(GLOB ERIZO_SOURCES ${ERIZO_SOURCE_DIR}/*.cpp ${ERIZO_SOURCE_DIR}/*.h)
add_library(erizo SHARED ${ERIZO_SOURCES})

#GLIB
find_package(Glib REQUIRED)
include_directories(${GLIB_INCLUDE_DIRS})
target_link_libraries(erizo ${GLIB_LIBRARIES})

# BOOST
set (BOOST_LIBS system thread regex)
find_package(Boost COMPONENTS ${BOOST_LIBS} REQUIRED)
target_link_libraries(erizo ${Boost_LIBRARIES})
find_library(SRTP srtp)
test_lib(${SRTP})
find_library(NICE nice)
test_lib(${NICE})
find_library(GTHREAD gthread-2.0)
test_lib(${GTHREAD})
target_link_libraries(erizo ${SRTP} ${NICE} ${GTHREAD})
