cmake_minimum_required(VERSION 2.8)
#functions
function(test_lib LIB_NAME)
 if (${LIB_NAME} MATCHES "^.*-NOTFOUND")
  message(FATAL_ERROR "lib not found: " ${LIB_NAME} " check README")
  return()
 endif(${LIB_NAME} MATCHES "^.*-NOTFOUND")
endfunction(test_lib)

project (ERIZO_EXAMPLES)
file(GLOB_RECURSE ERIZO_EXAMPLES_SOURCES ${ERIZO_EXAMPLES_SOURCE_DIR}/*.cpp ${ERIZO_EXAMPLES_SOURCE_DIR}/*.h)
add_executable(hsam ${ERIZO_EXAMPLES_SOURCES})
include_directories(${ERIZO_EXAMPLES_SOURCE_DIR}/../erizo)
link_directories(${ERIZO_EXAMPLES_SOURCE_DIR}/../../build)

set (BOOST_LIBS thread regex system)
find_package(Boost COMPONENTS ${BOOST_LIBS} REQUIRED)
target_link_libraries(hsam ${Boost_LIBRARIES})
target_link_libraries(hsam ${EXTRA_LIBS})
