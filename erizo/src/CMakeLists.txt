cmake_minimum_required(VERSION 2.6)

project (ERIZO_ALL)
option (COMPILE_EXAMPLES "COMPILE_EXAMPLES" OFF)
option (COMPILE_TEST "COMPILE_TEST" OFF)


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

#global variable
set(THIRD_PARTY_INCLUDE "${CMAKE_CURRENT_SOURCE_DIR}/../../build/libdeps/build/include/")
set(THIRD_PARTY_LIB "${CMAKE_CURRENT_SOURCE_DIR}/../../build/libdeps/build/lib/")

#erizo
add_subdirectory("${ERIZO_ALL_SOURCES}erizo")

#examples
if(COMPILE_EXAMPLES)
  add_subdirectory("${ERIZO_ALL_SOURCES}examples")
endif(COMPILE_EXAMPLES)

#tests
if(COMPILE_TEST)
  add_subdirectory("${ERIZO_ALL_SOURCES}test")
endif(COMPILE_TEST)
