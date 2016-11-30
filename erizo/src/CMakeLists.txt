cmake_minimum_required(VERSION 2.6)


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
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/cmake/")
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
    ${ERIZO_ROOT_DIR}/utils/cpplint.py --filter=-legal/copyright,-build/include --linelength=120 ${ERIZO_SOURCES_FILES}
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    COMMENT "Executing lint"
)

#global variable
set(THIRD_PARTY_INCLUDE "${CMAKE_CURRENT_SOURCE_DIR}/../../build/libdeps/build/include/")
set(THIRD_PARTY_LIB "${CMAKE_CURRENT_SOURCE_DIR}/../../build/libdeps/build/lib/")

#erizo
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/erizo")

#examples
if(COMPILE_EXAMPLES)
  add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/examples")
endif(COMPILE_EXAMPLES)

#tests
set(GMOCK_BUILD "${CMAKE_CURRENT_BINARY_DIR}/libdeps/gmock")
set(GMOCK_VERSION "1.8.0")
ExternalProject_Add(gtest
  URL "https://github.com/google/googletest/archive/release-${GMOCK_VERSION}.tar.gz"
  PREFIX ${GMOCK_BUILD}
  CMAKE_ARGS -DCMAKE_INSTALL_PREFIX:PATH=${GMOCK_BUILD}
)

enable_testing()

add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/test")
