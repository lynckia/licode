cmake_minimum_required(VERSION 2.6)
find_package(PkgConfig)
#functions
function(test_lib LIB_NAME)
 if (${LIB_NAME} MATCHES "^.*-NOTFOUND")
  message(FATAL_ERROR "lib not found: " ${LIB_NAME} " check README")
  return()
 endif(${LIB_NAME} MATCHES "^.*-NOTFOUND")
endfunction(test_lib)

#include
include_directories("${ERIZO_SOURCE_DIR}" "${THIRD_PARTY_INCLUDE}" "${GMOCK_BUILD}/include")
link_directories("${GMOCK_BUILD}/lib")

set(CMAKE_CXX_FLAGS "-g -Wall -std=c++0x")

set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/")

project (ERIZO_TEST)
file(GLOB_RECURSE ERIZO_TEST_SDPS ${ERIZO_TEST_SOURCE_DIR}/*.sdp)
file(COPY ${ERIZO_TEST_SDPS} DESTINATION ${ERIZO_TEST_BINARY_DIR})
file(COPY ${ERIZO_TEST_SOURCE_DIR}/log4cxx.properties DESTINATION ${ERIZO_TEST_BINARY_DIR})
file(GLOB_RECURSE ERIZO_TEST_SOURCES ${ERIZO_TEST_SOURCE_DIR}/*.cpp ${ERIZO_TEST_SOURCE_DIR}/*.h)
add_executable(tests ${ERIZO_TEST_SOURCES})
target_link_libraries(tests erizo)

# GLIB
find_package(Glib REQUIRED)
include_directories(${GLIB_INCLUDE_DIRS})
target_link_libraries(tests ${GLIB_LIBRARIES})

# NICE
find_library(NICE nice HINTS "${THIRD_PARTY_LIB}")
test_lib(${NICE})

set (BOOST_LIBS thread regex system)
find_package(Boost COMPONENTS ${BOOST_LIBS} REQUIRED)
target_link_libraries(tests ${Boost_LIBRARIES} gtest_main gmock_main)

add_test(NAME all
         WORKING_DIRECTORY "${ERIZO_TEST_BINARY_DIR}"
         COMMAND tests)

add_custom_target(check
    tests
    WORKING_DIRECTORY "${ERIZO_TEST_BINARY_DIR}"
    COMMENT "Running tests"
)
