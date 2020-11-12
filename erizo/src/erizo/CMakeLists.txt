cmake_minimum_required(VERSION 2.8)

project (ERIZO)

set(ERIZO_VERSION_MAJOR 0)
set(ERIZO_VERSION_MINOR 1)
if(${ERIZO_BUILD_TYPE} STREQUAL "debug")
  message("Generating DEBUG project")
  set(CMAKE_CXX_FLAGS "-g -Wall -std=c++14 ${ERIZO_CMAKE_CXX_FLAGS}")
elseif(${ERIZO_BUILD_TYPE} STREQUAL "sanitizer")
    message("Generating SANITIZER project")
    set(CMAKE_CXX_FLAGS "-g -Wall -std=c++14 ${ERIZO_CMAKE_CXX_FLAGS} -O1 -fno-omit-frame-pointer -fsanitize=address -fno-optimize-sibling-calls")
else()
  message("Generating RELEASE project")
  set(CMAKE_CXX_FLAGS "-g -Wall -O3 -std=c++14 ${ERIZO_CMAKE_CXX_FLAGS}")
endif()


include_directories("${ERIZO_SOURCE_DIR}" "${THIRD_PARTY_INCLUDE}" "${NICER_INCLUDE}")

file(GLOB_RECURSE ERIZO_SOURCES "${ERIZO_SOURCE_DIR}/*.h" "${ERIZO_SOURCE_DIR}/*.c" "${ERIZO_SOURCE_DIR}/*.cpp" "${ERIZO_SOURCE_DIR}/*.cc")

add_library(erizo SHARED ${ERIZO_SOURCES})


target_link_libraries(erizo ${boost_LIBRARIES} ${SRTP} ${SSL} ${CRYPTO} ${LIBS} ${LOG} webrtc nicer nrappkit)
