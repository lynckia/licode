cmake_minimum_required(VERSION 2.6)
#functions
function(test_lib LIB_NAME)
 if (${LIB_NAME} MATCHES "^.*-NOTFOUND")
  message(FATAL_ERROR "lib not found: " ${LIB_NAME} " check README")
  return()
 endif(${LIB_NAME} MATCHES "^.*-NOTFOUND")
endfunction(test_lib)

project (ERIZO_TEST)
file(GLOB_RECURSE ERIZO_TEST_SOURCES ${ERIZO_TEST_SOURCE_DIR}/*.cpp ${ERIZO_TEST_SOURCE_DIR}/*.h)
add_executable(test ${ERIZO_TEST_SOURCES})
include_directories(${ERIZO_TEST_SOURCE_DIR}/../erizo)
link_directories(${ERIZO_TEST_SOURCE_DIR}/../../build)

set (BOOST_LIBS thread regex system)
find_package(Boost COMPONENTS ${BOOST_LIBS} REQUIRED)
target_link_libraries(test ${Boost_LIBRARIES})
target_link_libraries(test ${EXTRA_LIBS})
