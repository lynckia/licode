cmake_minimum_required(VERSION 2.6)

#global variable
set(THIRD_PARTY "${CMAKE_CURRENT_SOURCE_DIR}/../../build/libdeps")

set(THIRD_PARTY_BUILD "${THIRD_PARTY}/build")
set(THIRD_PARTY_LIB "${THIRD_PARTY_BUILD}/lib/")
set(THIRD_PARTY_INCLUDE "${THIRD_PARTY_BUILD}/include/")

if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
  set(SSL_ARCH "darwin64-x86_64-cc")
else()
  set(SSL_ARCH "linux-x86_64")
endif (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")

set(OPENSSL_VERSION "1.0.1g")
ExternalProject_Add(buildopenssl
  URL "https://www.openssl.org/source/old/1.0.1/openssl-${OPENSSL_VERSION}.tar.gz"
  DOWNLOAD_DIR ${THIRD_PARTY}
  SOURCE_DIR ${THIRD_PARTY}/openssl-${OPENSSL_VERSION}
  BINARY_DIR ${THIRD_PARTY}/openssl-${OPENSSL_VERSION}
  CONFIGURE_COMMAND ${THIRD_PARTY}/openssl-${OPENSSL_VERSION}/Configure ${SSL_ARCH} threads --prefix=${THIRD_PARTY_BUILD}
  BUILD_COMMAND make -s V=0 -j1 all
  INSTALL_DIR ${THIRD_PARTY_LIB}
  INSTALL_COMMAND make install_sw
)

set(SSL ${THIRD_PARTY_LIB}libssl.a)
set(CRYPTO ${THIRD_PARTY_LIB}libcrypto.a)
add_library (ssl STATIC  IMPORTED)
set_target_properties (ssl PROPERTIES
                        IMPORTED_LOCATION                   ${SSL}
                        )

add_library (crypto STATIC  IMPORTED)
set_target_properties (crypto PROPERTIES
                        IMPORTED_LOCATION                   ${CRYPTO}
                        )

set(NICE_VERSION "0.1.4")
ExternalProject_Add(buildnice
  URL "https://nice.freedesktop.org/releases/libnice-${NICE_VERSION}.tar.gz"
  DOWNLOAD_DIR ${THIRD_PARTY}
  SOURCE_DIR ${THIRD_PARTY}/libnice-${NICE_VERSION}
  BINARY_DIR ${THIRD_PARTY}/libnice-${NICE_VERSION}
  PATCH_COMMAND patch -R ./agent/conncheck.c < ${CMAKE_CURRENT_SOURCE_DIR}/libnice-014.patch0 && patch -p1 < ${CMAKE_CURRENT_SOURCE_DIR}/libnice-014.patch1
  CONFIGURE_COMMAND ${THIRD_PARTY}/libnice-${NICE_VERSION}/configure --enable-static -enable-shared=NO --prefix=${THIRD_PARTY_BUILD}
  BUILD_COMMAND make -s V=0
  INSTALL_DIR ${THIRD_PARTY_LIB}
  INSTALL_COMMAND make install
)

set(NICE ${THIRD_PARTY_LIB}libnice.a)
add_library (nice STATIC IMPORTED)
set_target_properties (nice PROPERTIES
                        IMPORTED_LOCATION                   ${NICE}
                        )


ExternalProject_Add(buildsrtp
  URL "${ERIZO_ROOT_DIR}/third_party/srtp"
  DOWNLOAD_DIR ${THIRD_PARTY}
  SOURCE_DIR ${THIRD_PARTY}/libsrtp
  BINARY_DIR ${THIRD_PARTY}/libsrtp
  CONFIGURE_COMMAND ${THIRD_PARTY}/libsrtp/configure --prefix=${THIRD_PARTY_BUILD}
  BUILD_COMMAND make -s V=0
  INSTALL_DIR ${THIRD_PARTY_LIB}
  INSTALL_COMMAND make uninstall && make install
)

set(SRTP ${THIRD_PARTY_LIB}libsrtp.a)
add_library (srtp STATIC IMPORTED)
set_target_properties (srtp PROPERTIES
                        IMPORTED_LOCATION                   ${SRTP}
                        )

set(OPUS_VERSION "1.1")
ExternalProject_Add(opus
  URL "http://downloads.xiph.org/releases/opus/opus-${OPUS_VERSION}.tar.gz"
  DOWNLOAD_DIR ${THIRD_PARTY}
  SOURCE_DIR ${THIRD_PARTY}/opus-${OPUS_VERSION}
  BINARY_DIR ${THIRD_PARTY}/opus-${OPUS_VERSION}
  CONFIGURE_COMMAND ${THIRD_PARTY}/opus-${OPUS_VERSION}/configure --prefix=${THIRD_PARTY_BUILD}
  BUILD_COMMAND make -s V=0
  INSTALL_DIR ${THIRD_PARTY_LIB}
  INSTALL_COMMAND make install
)

set(OPUS ${THIRD_PARTY_LIB}libopus.a)

option(ENABLE_GPL "ENABLE_GPL" OFF)
set(LIBAV_VERSION "11.1")

if (ENABLE_GPL)
  message(STATUS "Enabling GPL for LibAV")
  set(LIBAV_CONFIGURE_EXTRA_ARGS "--enable-gpl --enable-libx264")
endif(ENABLE_GPL)

set(LIBAV_CFLAGS "-I${THIRD_PARTY_INCLUDE}")
set(LIBAV_LDFLAGS "-L${OPUS}")

ExternalProject_Add(buildlibav
  DEPENDS opus
  URL "https://www.libav.org/releases/libav-${LIBAV_VERSION}.tar.gz"
  DOWNLOAD_DIR ${THIRD_PARTY}
  SOURCE_DIR ${THIRD_PARTY}/libav-${LIBAV_VERSION}
  BINARY_DIR ${THIRD_PARTY}/libav-${LIBAV_VERSION}
  CONFIGURE_COMMAND ${THIRD_PARTY}/libav-${LIBAV_VERSION}/configure --prefix=${THIRD_PARTY_BUILD} ${LIBAV_CONFIGURE_EXTRA_ARGS} --enable-libvpx --enable-libopus --extra-cflags=${LIBAV_CFLAGS} --extra-ldflags=${LIBAV_LDFLAGS}
  BUILD_COMMAND make -s V=0
  INSTALL_DIR ${THIRD_PARTY_LIB}
  INSTALL_COMMAND make install
)

set(AVUTIL ${THIRD_PARTY_LIB}libavutil.a)
set(AVCODEC ${THIRD_PARTY_LIB}libavcodec.a)
set(AVFORMAT ${THIRD_PARTY_LIB}libavformat.a)
# Add the library as an external, imported library
add_library (avutil STATIC IMPORTED)
set_target_properties (avutil PROPERTIES
                        IMPORTED_LOCATION                   ${AVUTIL}
                        )
add_library (avcodec STATIC IMPORTED)
set_target_properties (avcodec PROPERTIES
                        IMPORTED_LOCATION                   ${AVCODEC}
                        )

add_library (avformat STATIC IMPORTED)
set_target_properties (avformat PROPERTIES
                        IMPORTED_LOCATION                   ${AVFORMAT}
                        )
