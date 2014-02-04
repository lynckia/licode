cmake_minimum_required(VERSION 2.8)

project (ERIZO_ALL)
set (COMPILE_EXAMPLES OFF)
add_subdirectory("${ERIZO_ALL_SOURCES}erizo")
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

if(COMPILE_EXAMPLES)
  include_directories("${ERIZO_ALL_SOURCES}erizo")
  set (EXTRA_LIBS ${EXTRA_LIBS} erizo)
  add_subdirectory("${ERIZO_ALL_SOURCES}examples")
endif(COMPILE_EXAMPLES)
