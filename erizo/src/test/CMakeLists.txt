cmake_minimum_required(VERSION 2.6)
#functions
function(test_lib LIB_NAME)
 if (${LIB_NAME} MATCHES "^.*-NOTFOUND")
  message(FATAL_ERROR "lib not found: " ${LIB_NAME} " check README")
  return()
 endif(${LIB_NAME} MATCHES "^.*-NOTFOUND")
endfunction(test_lib)

#include
include_directories("${ERIZO_SOURCE_DIR}" "${THIRD_PARTY_INCLUDE}")

project (ERIZO_TEST)
file(GLOB_RECURSE ERIZO_TEST_SDPS ${ERIZO_TEST_SOURCE_DIR}/*.sdp)
file(COPY ${ERIZO_TEST_SDPS} DESTINATION ${ERIZO_TEST_BINARY_DIR})
file(COPY ${ERIZO_TEST_SOURCE_DIR}/log4cxx.properties DESTINATION ${ERIZO_TEST_BINARY_DIR})
file(GLOB_RECURSE ERIZO_TEST_SOURCES ${ERIZO_TEST_SOURCE_DIR}/*.cpp ${ERIZO_TEST_SOURCE_DIR}/*.h)
add_executable(test ${ERIZO_TEST_SOURCES})
target_link_libraries(test erizo)

set (BOOST_LIBS thread regex system unit_test_framework)
find_package(Boost COMPONENTS ${BOOST_LIBS} REQUIRED)
target_link_libraries(test ${Boost_LIBRARIES})
