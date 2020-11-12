set(NICER_BUILD "${CMAKE_CURRENT_BINARY_DIR}/libdeps/nicer")
ExternalProject_Add(project_nicer
  GIT_REPOSITORY "https://github.com/lynckia/nicer.git"
  GIT_TAG "1.6"
  PREFIX ${NICER_BUILD}
  CMAKE_ARGS -DCMAKE_INSTALL_PREFIX:PATH=${NICER_BUILD} -DTHIRD_PARTY_LIB=${THIRD_PARTY_LIB}
  DOWNLOAD_DIR "${NICER_BUILD}/src"
  INSTALL_DIR "${NICER_BUILD}/out"
)

add_library(nicer STATIC IMPORTED)
set_property(TARGET nicer PROPERTY IMPORTED_LOCATION ${NICER_BUILD}/src/project_nicer-build/nicer/libnicer.a)
set_property(TARGET nicer PROPERTY INTERFACE_LOCATION ${NICER_BUILD}/include)
add_dependencies(nicer project_nicer)

add_library(nrappkit STATIC IMPORTED)
set_property(TARGET nrappkit PROPERTY IMPORTED_LOCATION ${NICER_BUILD}/src/project_nicer-build/third_party/nrappkit/lib/libnrappkit.a)
add_dependencies(nrappkit project_nicer)

set(NICE_ROOT_INCLUDE ${NICER_BUILD}/include)

list(APPEND NICER_INCLUDE "${NICE_ROOT_INCLUDE}/event"
  "${NICE_ROOT_INCLUDE}/log"
  "${NICE_ROOT_INCLUDE}/plugin"
  "${NICE_ROOT_INCLUDE}/registry"
  "${NICE_ROOT_INCLUDE}/share"
  "${NICE_ROOT_INCLUDE}/stats"
  "${NICE_ROOT_INCLUDE}/util"
  "${NICE_ROOT_INCLUDE}/util/libekr"
  "${NICE_ROOT_INCLUDE}/port/generic/include"
  "${NICE_ROOT_INCLUDE}/port/generic/include/sys"
  "${NICE_ROOT_INCLUDE}/crypto"
  "${NICE_ROOT_INCLUDE}/ice"
  "${NICE_ROOT_INCLUDE}/net"
  "${NICE_ROOT_INCLUDE}/stun")

set(ERIZO_CMAKE_CXX_FLAGS "${ERIZO_CMAKE_CXX_FLAGS} -DNO_REG_RPC=1 -DHAVE_VFPRINTF=1 -DRETSIGTYPE=void -DNEW_STDIO -DHAVE_STRDUP=1 -DHAVE_STRLCPY=1 -DHAVE_LIBM=1 -DHAVE_SYS_TIME_H=1 -DTIME_WITH_SYS_TIME_H=1 -D__UNUSED__=\"__attribute__((unused))\"")

IF(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
  list(APPEND NICER_INCLUDE "${NICE_ROOT_INCLUDE}/port/darwin/include" "${NICE_ROOT_INCLUDE}/port/darwin/include/sys")
  set(ERIZO_CMAKE_CXX_FLAGS "${ERIZO_CMAKE_CXX_FLAGS} -DDARWIN")
ELSE()
  list(APPEND NICER_INCLUDE "${NICE_ROOT_INCLUDE}/port/linux/include" "${NICE_ROOT_INCLUDE}/port/linux/include/sys")
  set(ERIZO_CMAKE_CXX_FLAGS "${ERIZO_CMAKE_CXX_FLAGS} -DLINUX -DNOLINUXIF")
ENDIF(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
