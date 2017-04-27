cmake_minimum_required(VERSION 2.6)

project (ERIZO)

set(ERIZO_VERSION_MAJOR 0)
set(ERIZO_VERSION_MINOR 1)
set(CMAKE_CXX_FLAGS "-g -Wall -std=c++11 ${ERIZO_CMAKE_CXX_FLAGS}")

include_directories("${ERIZO_SOURCE_DIR}" "${THIRD_PARTY_INCLUDE}" "${NICER_INCLUDE}")

file(GLOB_RECURSE ERIZO_SOURCES "${ERIZO_SOURCE_DIR}/*.h" "${ERIZO_SOURCE_DIR}/*.c" "${ERIZO_SOURCE_DIR}/*.cpp" "${ERIZO_SOURCE_DIR}/*.cc")

add_library(erizo SHARED ${ERIZO_SOURCES})

#message("Libs ${SRTP} ${NICE} ${GTHREAD} ${SSL} ${CRYPTO} ${LIBS} ${LOG}")

target_link_libraries(erizo ${GLIB_LIBRARIES} ${Boost_LIBRARIES} ${SRTP} ${NICE} ${GTHREAD} ${SSL} ${CRYPTO} ${LIBS} ${LOG} webrtc nicer nrappkit)
