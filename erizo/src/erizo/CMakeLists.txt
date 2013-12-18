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

add_definitions(-std=c++11)

set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/")
set(ERIZO_VERSION_MAJOR 0)
set(ERIZO_VERSION_MINOR 1)
set(CMAKE_CXX_FLAGS "-g -Wall")

set(LIB_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/../../../build/libdeps/build/lib/")

include_directories("${ERIZO_SOURCE_DIR}" "${LIB_DIRS}../include/")

file(GLOB_RECURSE ERIZO_SOURCES "${ERIZO_SOURCE_DIR}/*.h" "${ERIZO_SOURCE_DIR}/*.c" "${ERIZO_SOURCE_DIR}/*.cpp")

add_library(erizo SHARED ${ERIZO_SOURCES})

# GLIB
find_package(Glib REQUIRED)
include_directories(${GLIB_INCLUDE_DIRS})
target_link_libraries(erizo ${GLIB_LIBRARIES})

# BOOST
set (BOOST_LIBS thread regex system)
find_package(Boost COMPONENTS ${BOOST_LIBS} REQUIRED)
target_link_libraries(erizo ${Boost_LIBRARIES})

# GTHREAD
find_library(GTHREAD gthread-2.0)
test_lib(${GTHREAD})

# SRTP
find_library(SRTP srtp HINTS "${LIB_DIRS}")
test_lib(${SRTP})

# NICE
find_library(NICE nice HINTS "${LIB_DIRS}")
test_lib(${NICE})

# OPENSSL
find_library(SSL ssl HINTS "${LIB_DIRS}")
test_lib(${SSL})
find_library(CRYPTO crypto HINTS "${LIB_DIRS}")
test_lib(${CRYPTO})

# Libav
find_library(AVUTIL avutil HINTS "${LIB_DIRS}")
test_lib(${AVUTIL})
set (LIBS ${AVUTIL})
find_library(AVCODEC avcodec HINTS "${LIB_DIRS}")
test_lib(${AVCODEC})
set (LIBS ${LIBS} ${AVCODEC})
find_library(AVFORMAT avformat HINTS "${LIB_DIRS}")
test_lib(${AVFORMAT})
set (LIBS ${LIBS} ${AVFORMAT})

# LOG4CXX
find_library(LOG log4cxx)
test_lib(${LOG})

#message("Libs ${SRTP} ${NICE} ${GTHREAD} ${SSL} ${CRYPTO} ${LIBS} ${LOG}")

target_link_libraries(erizo ${SRTP} ${NICE} ${GTHREAD} ${SSL} ${CRYPTO} ${LIBS} ${LOG})
