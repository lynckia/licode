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
ExternalProject_Add(ssl
  URL "https://www.openssl.org/source/old/1.0.1/openssl-${OPENSSL_VERSION}.tar.gz"
  DOWNLOAD_DIR ${THIRD_PARTY}
  SOURCE_DIR ${THIRD_PARTY}/openssl-${OPENSSL_VERSION}
  BINARY_DIR ${THIRD_PARTY}/openssl-${OPENSSL_VERSION}
  CONFIGURE_COMMAND ${THIRD_PARTY}/openssl-${OPENSSL_VERSION}/Configure ${SSL_ARCH} shared threads --prefix=${THIRD_PARTY_BUILD}
  BUILD_COMMAND make -s V=0 -j1 all
  INSTALL_DIR ${THIRD_PARTY_LIB}
  INSTALL_COMMAND make install_sw
)

set(SSL ${THIRD_PARTY_LIB}libssl.a)
set(CRYPTO ${THIRD_PARTY_LIB}libcrypto.a)

set(NICE_VERSION "0.1.4")
ExternalProject_Add(nice
  URL "https://nice.freedesktop.org/releases/libnice-${NICE_VERSION}.tar.gz"
  DOWNLOAD_DIR ${THIRD_PARTY}
  SOURCE_DIR ${THIRD_PARTY}/libnice-${NICE_VERSION}
  BINARY_DIR ${THIRD_PARTY}/libnice-${NICE_VERSION}
  PATCH_COMMAND patch -R ./agent/conncheck.c < ${CMAKE_CURRENT_SOURCE_DIR}/libnice-014.patch0 && patch -p1 < ${CMAKE_CURRENT_SOURCE_DIR}/libnice-014.patch1
  CONFIGURE_COMMAND ${THIRD_PARTY}/libnice-${NICE_VERSION}/configure --enable-static --prefix=${THIRD_PARTY_BUILD}
  BUILD_COMMAND make -s V=0
  INSTALL_DIR ${THIRD_PARTY_LIB}
  INSTALL_COMMAND make install
)

ExternalProject_Add(srtp
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

option(ENABLE_GPL "ENABLE_GPL" OFF)
set(LIBAV_VERSION "11.1")

if (ENABLE_GPL)
  message(STATUS "Enabling GPL for LibAV")
  set(LIBAV_CONFIGURE_EXTRA_ARGS "--enable-gpl --enable-libx264")
endif(ENABLE_GPL)

ExternalProject_Add(libav
  DEPENDS opus
  URL "https://www.libav.org/releases/libav-${LIBAV_VERSION}.tar.gz"
  DOWNLOAD_DIR ${THIRD_PARTY}
  SOURCE_DIR ${THIRD_PARTY}/libav-${LIBAV_VERSION}
  BINARY_DIR ${THIRD_PARTY}/libav-${LIBAV_VERSION}
  CONFIGURE_COMMAND PKG_CONFIG_PATH=${THIRD_PARTY}/lib/pkgconfig ${THIRD_PARTY}/libav-${LIBAV_VERSION}/configure --prefix=${THIRD_PARTY_BUILD} ${LIBAV_CONFIGURE_EXTRA_ARGS} --enable-libvpx --enable-libopus
  BUILD_COMMAND make -s V=0
  INSTALL_DIR ${THIRD_PARTY_LIB}
  INSTALL_COMMAND make install
)

set(LIBS ${THIRD_PARTY_LIB}libavutil.a ${THIRD_PARTY_LIB}libavcodec.a ${THIRD_PARTY_LIB}libavformat.a)
