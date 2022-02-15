cmake_minimum_required(VERSION 2.6)

find_package(PkgConfig)
#functions
function(test_lib LIB_NAME)
  if (${LIB_NAME} MATCHES "^.*-NOTFOUND")
    message(FATAL_ERROR "lib not found: " ${LIB_NAME} " check README")
    return()
  endif(${LIB_NAME} MATCHES "^.*-NOTFOUND")
endfunction(test_lib)

include(ExternalProject)

project (ERIZO_ALL)

if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
  execute_process(COMMAND ${CMAKE_C_COMPILER} -dumpversion
    OUTPUT_VARIABLE GCC_VERSION)
  if (GCC_VERSION VERSION_LESS 5)
    message(FATAL_ERROR "gcc version must be >= 5 and is ${GCC_VERSION}")
  endif()

  execute_process(COMMAND ${CMAKE_CXX_COMPILER} -dumpversion
    OUTPUT_VARIABLE CXX_VERSION)
  if (CXX_VERSION VERSION_LESS 5)
    message(FATAL_ERROR "g++ version must be >= 5 and is ${CXX_VERSION}")
  endif()
endif()

option (COMPILE_EXAMPLES "COMPILE_EXAMPLES" OFF)

set(ERIZO_ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/..")


set(CMAKE_MACOSX_RPATH 1)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/cmake/" "${CMAKE_CURRENT_SOURCE_DIR}/../cmake/")
find_package (Doxygen)
if(DOXYGEN_FOUND)
  configure_file("${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile.in" "${CMAKE_CURRENT_BINARY_DIR}/Doxyfile" @ONLY)
  add_custom_target(doc
    ${DOXYGEN_EXECUTABLE} "${CMAKE_CURRENT_BINARY_DIR}/Doxyfile"
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    COMMENT "Generating API documentation with Doxygen" VERBATIM
  )
endif(DOXYGEN_FOUND)

set (ERIZO_SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/erizo)
set (ERIZO_TEST ${CMAKE_CURRENT_SOURCE_DIR}/test)

file(GLOB_RECURSE ERIZO_SOURCES_FILES ${ERIZO_SOURCE}/*.cpp ${ERIZO_SOURCE}/*.h ${ERIZO_TEST}/*.cpp)
add_custom_target(lint
    cpplint --filter=-legal/copyright,-build/include --linelength=120 ${ERIZO_SOURCES_FILES}
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    COMMENT "Executing lint"
)

#global variable
set(THIRD_PARTY_INCLUDE "${CMAKE_CURRENT_SOURCE_DIR}/../../build/libdeps/build/include/")
set(THIRD_PARTY_LIB "${CMAKE_CURRENT_SOURCE_DIR}/../../build/libdeps/build/lib/")

## Depencencies

include(${CMAKE_CURRENT_SOURCE_DIR}/../conan_paths.cmake)

# Add /usr/local/include
IF(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
  include_directories("/usr/local/include")
ENDIF(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")

include(${CMAKE_CURRENT_SOURCE_DIR}/../conan_paths.cmake)
# GLIB
find_package(glib)
include_directories(${GLIB_INCLUDE_DIRS})

# BOOST
set (BOOST_LIBS thread regex system)
find_package(Boost)
include_directories(${Boost_INCLUDE_DIRS})
# Adding  -Wno-error=attributes for libav - an update should fix it 
set(ERIZO_CMAKE_CXX_FLAGS "${ERIZO_CMAKE_CXX_FLAGS}  -Wno-error=attributes -DBOOST_THREAD_PROVIDES_FUTURE -DBOOST_THREAD_PROVIDES_FUTURE_CONTINUATION -DBOOST_THREAD_PROVIDES_FUTURE_WHEN_ALL_WHEN_ANY")
# GTHREAD
find_library(GTHREAD gthread-2.0 HINTS "${THIRD_PARTY_LIB}")
test_lib(${GTHREAD})

# SRTP
find_library(SRTP srtp2 HINTS "${THIRD_PARTY_LIB}")
test_lib(${SRTP})

# NICE
find_library(NICE nice HINTS "${THIRD_PARTY_LIB}")
test_lib(${NICE})

# OPENSSL
find_library(SSL ssl HINTS "${THIRD_PARTY_LIB}")
test_lib(${SSL})
find_library(CRYPTO crypto HINTS "${THIRD_PARTY_LIB}")
test_lib(${CRYPTO})

# Libav
find_library(AVUTIL avutil HINTS "${THIRD_PARTY_LIB}")
test_lib(${AVUTIL})
set (LIBS ${AVUTIL})
find_library(AVCODEC avcodec HINTS "${THIRD_PARTY_LIB}")
test_lib(${AVCODEC})
set (LIBS ${LIBS} ${AVCODEC})
find_library(AVFORMAT avformat HINTS "${THIRD_PARTY_LIB}")
test_lib(${AVFORMAT})
set (LIBS ${LIBS} ${AVFORMAT})

# LOG4CXX
# find_package(log4cxx)
# include_directories(${log4cxx_INCLUDE_DIRS})
find_package(apr)
find_package(apr-util)
find_package(Iconv)

set(LOG4CXX_BUILD "${CMAKE_CURRENT_BINARY_DIR}/libdeps/log4cxx")
set(LOG4CXX_VERSION "0.11.0")
ExternalProject_Add(log4cxxInstall
  URL "https://archive.apache.org/dist/logging/log4cxx/${LOG4CXX_VERSION}/apache-log4cxx-${LOG4CXX_VERSION}.tar.gz"
  PREFIX ${LOG4CXX_BUILD}
  CONFIGURE_COMMAND cd ${LOG4CXX_BUILD}/src/log4cxxInstall && ./autogen.sh && ./configure --with-apr=${apr_LIB_DIRS}/.. --with-apr-util=${apr-util_LIB_DIRS}/.. --prefix=${LOG4CXX_BUILD} --disable-doxygen --enable-silent-rules "CPPFLAGS=-fPIC -I${apr_INCLUDES} -I${apr-util_INCLUDES}" "LDFLAGS=-L${apr_LIB_DIRS} -L${apr-util_LIB_DIRS} -L${Iconv_LIB_DIRS}"
  BUILD_COMMAND cd ${LOG4CXX_BUILD}/src/log4cxxInstall && make
  INSTALL_COMMAND cd ${LOG4CXX_BUILD}/src/log4cxxInstall && make install
)
add_library(log4cxx STATIC IMPORTED)
set_target_properties(log4cxx PROPERTIES IMPORTED_LOCATION ${LOG4CXX_BUILD}/lib/liblog4cxx.a)

include_directories("${LOG4CXX_BUILD}/include")

find_package(absl)
include_directories("${absl_INCLUDE_DIRS}")

# WEBRTC
include("${CMAKE_CURRENT_SOURCE_DIR}/third_party/webrtc.cmake")

# NICER
include("${CMAKE_CURRENT_SOURCE_DIR}/third_party/nicer.cmake")

## Erizo
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/erizo")

## Examples
if(COMPILE_EXAMPLES)
  add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/examples")
endif(COMPILE_EXAMPLES)

## Tests
if(${ERIZO_BUILD_TYPE} STREQUAL "sanitizer")
  set(SANITIZER_OPTION "-DCMAKE_CXX_FLAGS=-fsanitize=address")
else()
  set(SANITIZER_OPTION "")
endif()
set(GMOCK_BUILD "${CMAKE_CURRENT_BINARY_DIR}/libdeps/gmock")
set(GMOCK_VERSION "1.8.0")
ExternalProject_Add(gtest
  URL "https://github.com/google/googletest/archive/release-${GMOCK_VERSION}.tar.gz"
  PREFIX ${GMOCK_BUILD}
  CMAKE_ARGS -DCMAKE_INSTALL_PREFIX:PATH=${GMOCK_BUILD} ${SANITIZER_OPTION}
)

enable_testing()

add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/test")
