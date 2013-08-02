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

set(SSL_DIR /usr/local/ssl)
set(SSL_INCLUDE_DIR ${SSL_DIR}/include)

include_directories(${ERIZO_SOURCE_DIR} ${SSL_INCLUDE_DIR})

file(GLOB_RECURSE ERIZO_SOURCES ${ERIZO_SOURCE_DIR}/*.h ${ERIZO_SOURCE_DIR}/*.c ${ERIZO_SOURCE_DIR}/*.cpp)

add_library(erizo SHARED ${ERIZO_SOURCES})

# GLIB
find_package(Glib REQUIRED)
include_directories(${GLIB_INCLUDE_DIRS})
target_link_libraries(erizo ${GLIB_LIBRARIES})

# BOOST
set (BOOST_LIBS system thread regex)
find_package(Boost COMPONENTS ${BOOST_LIBS} REQUIRED)
target_link_libraries(erizo ${Boost_LIBRARIES})

# SRTP
find_library(SRTP srtp)
test_lib(${SRTP})

# NICE
if(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
  find_library(NICE nice)
  test_lib(${NICE})
  set (OperatingSystem "Mac OS X")
else(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
  set (NICE /usr/local/lib/libnice.so)
endif(${CMAKE_SYSTEM_NAME} MATCHES "Darwin") 

# GTHREAD
find_library(GTHREAD gthread-2.0)
test_lib(${GTHREAD})

# OPENSSL
set(SSL ${SSL_DIR}/lib/libssl.a ${SSL_DIR}/lib/libcrypto.a)
target_link_libraries(erizo ${SRTP} ${NICE} ${GTHREAD} ${SSL})
