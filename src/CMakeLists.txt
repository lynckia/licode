cmake_minimum_required(VERSION 2.8)

project (ERIZO_ALL)
set (COMPILE_EXAMPLES ON)
add_subdirectory(${ERIZO_ALL_SOURCES}erizo)

if(COMPILE_EXAMPLES)
  include_directories(${ERIZO_ALL_SOURCES}erizo)
  set (EXTRA_LIBS ${EXTRA_LIBS} erizo)
  add_subdirectory(${ERIZO_ALL_SOURCES}examples)
endif(COMPILE_EXAMPLES)
