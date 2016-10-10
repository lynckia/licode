cmake_minimum_required(VERSION 2.6)
find_package(PkgConfig)
#functions
function(test_lib LIB_NAME)
  if (${LIB_NAME} MATCHES "^.*-NOTFOUND")
    message(FATAL_ERROR "lib not found: " ${LIB_NAME} " check README")
    return()
  endif(${LIB_NAME} MATCHES "^.*-NOTFOUND")
endfunction(test_lib)
project (ERIZO)


set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/")
set(ERIZO_VERSION_MAJOR 0)
set(ERIZO_VERSION_MINOR 1)
set(CMAKE_CXX_FLAGS "-g -Wall -std=c++0x")

IF(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}  -fno-stack-protector")
ENDIF(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")

include_directories("${ERIZO_SOURCE_DIR}" "${THIRD_PARTY_INCLUDE}" /usr/local/include)
link_directories(${THIRD_PARTY_INCLUDE})
link_directories(/usr/local/lib)

file(GLOB_RECURSE ERIZO_SOURCES "${ERIZO_SOURCE_DIR}/*.h" "${ERIZO_SOURCE_DIR}/*.c" "${ERIZO_SOURCE_DIR}/*.cpp" "${ERIZO_SOURCE_DIR}/*.cc")

add_library(erizo STATIC ${ERIZO_SOURCES})

add_dependencies(erizo buildopenssl buildsrtp buildnice buildlibav)

# GLIB
find_package(Glib REQUIRED)
include_directories(${GLIB_INCLUDE_DIRS})

# BOOST
set (BOOST_LIBS thread regex system)
find_package(Boost COMPONENTS ${BOOST_LIBS} REQUIRED)

# GTHREAD
find_library(GTHREAD gthread-2.0 HINTS "${THIRD_PARTY_LIB}")
test_lib(${GTHREAD})

# LOG4CXX
find_library(LOG log4cxx)
test_lib(${LOG})

#message("Libs ${SRTP} ${NICE} ${GTHREAD} ${SSL} ${CRYPTO} ${AVLIBS} ${LOG}")

target_link_libraries(erizo ${Boost_LIBRARIES} ${GTHREAD} ${LOG} ${GLIB_LIBRARIES} nice srtp ssl crypto avutil avcodec avformat)
add_custom_command(TARGET erizo POST_BUILD
    COMMAND /usr/bin/libtool -static -o liberizo.a
    liberizo.a ${NICE} ${SRTP} ${AVCODEC} ${AVUTIL} ${AVFORMAT}
)
