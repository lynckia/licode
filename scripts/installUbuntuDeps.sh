#!/usr/bin/env bash

set -e

SCRIPT=`pwd`/$0
FILENAME=`basename $SCRIPT`
PATHNAME=`dirname $SCRIPT`
ROOT=$PATHNAME/..
BUILD_DIR=$ROOT/build
CURRENT_DIR=`pwd`
NVM_CHECK="$PATHNAME"/checkNvm.sh

LIB_DIR=$BUILD_DIR/libdeps
PREFIX_DIR=$LIB_DIR/build/
FAST_MAKE=''


parse_arguments(){
  while [ "$1" != "" ]; do
    case $1 in
      "--enable-gpl")
        ENABLE_GPL=true
        ;;
      "--cleanup")
        CLEANUP=true
        ;;
      "--fast")
        FAST_MAKE='-j4'
        ;;
    esac
    shift
  done
}

check_proxy(){
  if [ -z "$http_proxy" ]; then
    echo "No http proxy set, doing nothing"
  else
    echo "http proxy configured, configuring npm"
    npm config set proxy $http_proxy
  fi

  if [ -z "$https_proxy" ]; then
    echo "No https proxy set, doing nothing"
  else
    echo "https proxy configured, configuring npm"
    npm config set https-proxy $https_proxy
  fi
}

install_nvm_node() {
  if [ -d $LIB_DIR ]; then
    export NVM_DIR=$(readlink -f "$LIB_DIR/nvm")
    if [ ! -s "$NVM_DIR/nvm.sh" ]; then
      git clone https://github.com/creationix/nvm.git "$NVM_DIR"
      cd "$NVM_DIR"
      git checkout `git describe --abbrev=0 --tags --match "v[0-9]*" origin`
      cd "$CURRENT_DIR"
    fi
    . $NVM_CHECK
    nvm install
  else
    mkdir -p $LIB_DIR
    install_nvm_node
  fi
}

install_apt_deps(){
  install_nvm_node
  nvm use
  npm install
  npm install -g node-gyp
  npm install gulp@3.9.1 gulp-eslint@3 run-sequence@2.2.1 webpack-stream@4.0.0 google-closure-compiler-js@20170521.0.0 del@3.0.0 gulp-sourcemaps@2.6.4 script-loader@0.7.2 expose-loader@0.7.5
  sudo apt-get install -qq python-software-properties -y
  sudo apt-get install -qq software-properties-common -y
  sudo add-apt-repository ppa:ubuntu-toolchain-r/test -y
  sudo apt-get update -y
  sudo apt-get install -qq git make gcc-5 g++-5 libssl-dev cmake libglib2.0-dev pkg-config libboost-regex-dev libboost-thread-dev libboost-system-dev rabbitmq-server mongodb curl libboost-test-dev libtool autotools-dev automake -y
  sudo update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-5 60 --slave /usr/bin/g++ g++ /usr/bin/g++-5

  sudo chown -R `whoami` ~/.npm ~/tmp/ || true
}

download_openssl() {
  OPENSSL_VERSION=$1
  OPENSSL_MAJOR="${OPENSSL_VERSION%?}"
  echo "Downloading OpenSSL from https://www.openssl.org/source/$OPENSSL_MAJOR/openssl-$OPENSSL_VERSION.tar.gz"
  curl -OL https://www.openssl.org/source/openssl-$OPENSSL_VERSION.tar.gz
  tar -zxvf openssl-$OPENSSL_VERSION.tar.gz || DOWNLOAD_SUCCESS=$?
  if [ "$DOWNLOAD_SUCCESS" -eq 1 ]
  then
    echo "Downloading OpenSSL from https://www.openssl.org/source/old/$OPENSSL_MAJOR/openssl-$OPENSSL_VERSION.tar.gz"
    curl -OL https://www.openssl.org/source/old/$OPENSSL_MAJOR/openssl-$OPENSSL_VERSION.tar.gz
    tar -zxvf openssl-$OPENSSL_VERSION.tar.gz
  fi
}

install_openssl(){
  if [ -d $LIB_DIR ]; then
    cd $LIB_DIR
    OPENSSL_VERSION=`node -pe process.versions.openssl`
    if [ ! -f ./openssl-$OPENSSL_VERSION.tar.gz ]; then
      download_openssl $OPENSSL_VERSION
      cd openssl-$OPENSSL_VERSION
      ./config --prefix=$PREFIX_DIR --openssldir=$PREFIX_DIR -fPIC
      make $FAST_MAKE -s V=0
      make install_sw
    else
      echo "openssl already installed"
    fi
    cd $CURRENT_DIR
  else
    mkdir -p $LIB_DIR
    install_openssl
  fi
}

install_libnice(){
  if [ -d $LIB_DIR ]; then
    cd $LIB_DIR
    if [ ! -f ./libnice-0.1.4.tar.gz ]; then
      curl -OL https://nice.freedesktop.org/releases/libnice-0.1.4.tar.gz
      tar -zxvf libnice-0.1.4.tar.gz
      cd libnice-0.1.4
      patch -R ./agent/conncheck.c < $PATHNAME/patches/libnice/libnice-014.patch0
      ./configure --prefix=$PREFIX_DIR
      make $FAST_MAKE -s V=0
      make install
    else
      echo "libnice already installed"
    fi
    cd $CURRENT_DIR
  else
    mkdir -p $LIB_DIR
    install_libnice
  fi
}

install_opus(){
  [ -d $LIB_DIR ] || mkdir -p $LIB_DIR
  cd $LIB_DIR
  if [ ! -f ./opus-1.1.tar.gz ]; then
    curl -OL http://downloads.xiph.org/releases/opus/opus-1.1.tar.gz
    tar -zxvf opus-1.1.tar.gz
    cd opus-1.1
    ./configure --prefix=$PREFIX_DIR
    make $FAST_MAKE -s V=0
    make install
  else
    echo "opus already installed"
  fi
  cd $CURRENT_DIR
}

install_mediadeps(){
  install_opus
  sudo apt-get -qq install yasm libvpx. libx264.
  if [ -d $LIB_DIR ]; then
    cd $LIB_DIR
    if [ ! -f ./v11.1.tar.gz ]; then
      curl -O -L https://github.com/libav/libav/archive/v11.1.tar.gz
      tar -zxvf v11.1.tar.gz
      cd libav-11.1
      PKG_CONFIG_PATH=${PREFIX_DIR}/lib/pkgconfig ./configure --prefix=$PREFIX_DIR --enable-shared --enable-gpl --enable-libvpx --enable-libx264 --enable-libopus --disable-doc
      make $FAST_MAKE -s V=0
      make install
    else
      echo "libav already installed"
    fi
    cd $CURRENT_DIR
  else
    mkdir -p $LIB_DIR
    install_mediadeps
  fi

}

install_mediadeps_nogpl(){
  install_opus
  sudo apt-get -qq install yasm libvpx.
  if [ -d $LIB_DIR ]; then
    cd $LIB_DIR
    if [ ! -f ./v11.1.tar.gz ]; then
      curl -O -L https://github.com/libav/libav/archive/v11.1.tar.gz
      tar -zxvf v11.1.tar.gz
      cd libav-11.1
      PKG_CONFIG_PATH=${PREFIX_DIR}/lib/pkgconfig ./configure --prefix=$PREFIX_DIR --enable-shared --enable-gpl --enable-libvpx --enable-libopus --disable-doc
      make $FAST_MAKE -s V=0
      make install
    else
      echo "libav already installed"
    fi
    cd $CURRENT_DIR
  else
    mkdir -p $LIB_DIR
    install_mediadeps_nogpl
  fi
}

install_libsrtp(){
  if [ -d $LIB_DIR ]; then
    cd $LIB_DIR
    curl -o libsrtp-2.1.0.tar.gz https://codeload.github.com/cisco/libsrtp/tar.gz/v2.1.0
    tar -zxvf libsrtp-2.1.0.tar.gz
    cd libsrtp-2.1.0
    CFLAGS="-fPIC" ./configure --enable-openssl --prefix=$PREFIX_DIR --with-openssl-dir=$PREFIX_DIR
    make $FAST_MAKE -s V=0 && make uninstall && make install
    cd $CURRENT_DIR
  else
    mkdir -p $LIB_DIR
    install_libsrtp
  fi
}

install_log4cxx_10(){
  if [ -d $LIB_DIR ]; then
    cd $LIB_DIR
    curl -o apache-log4cxx-0.10.0.tar.gz http://it.apache.contactlab.it/logging/log4cxx/0.10.0/apache-log4cxx-0.10.0.tar.gz
    tar -zxvf apache-log4cxx-0.10.0.tar.gz
    cd apache-log4cxx-0.10.0
    patch src/main/include/log4cxx/Makefile.am < $CURRENT_DIR/patches/log4cxx/log4cxx-385.patch # https://issues.apache.org/jira/secure/attachment/12485439/log4cxx-385.patch (1)
    patch src/main/include/log4cxx/private/Makefile.am < $CURRENT_DIR/patches/log4cxx/log4cxx-385_1.patch # https://issues.apache.org/jira/secure/attachment/12485439/log4cxx-385.patch (2)
    patch src/main/cpp/level.cpp < $CURRENT_DIR/patches/log4cxx/level.cpp.patch # Fixes https://issues.apache.org/jira/browse/LOGCXX-394 (multithread safe) but may introduce leaks
    patch src/main/include/log4cxx/level.h < $CURRENT_DIR/patches/log4cxx/level.h.patch # Fixes https://issues.apache.org/jira/browse/LOGCXX-394 (multithread safe) but may introduce leaks
    patch src/main/include/log4cxx/helpers/simpledateformat.h < $CURRENT_DIR/patches/log4cxx/locale.patch # Fixes compilation osx https://github.com/Homebrew/legacy-homebrew/blob/56b57d583e874e6dfe7a417d329a147e4d4b064f/Library/Formula/log4cxx.rb
    patch src/main/cpp/stringhelper.cpp < $CURRENT_DIR/patches/log4cxx/locale_1.patch # Fixes compilation osx https://github.com/Homebrew/legacy-homebrew/blob/56b57d583e874e6dfe7a417d329a147e4d4b064f/Library/Formula/log4cxx.rb
    ./autogen.sh
    APR_PATH="$(cd /usr/local/Cellar/apr/*/bin && pwd)"
    APR_UTILS_PATH="$(cd /usr/local/Cellar/apr-util/*/bin/ && pwd)"
    ./configure --prefix=$PREFIX_DIR --disable-debug --disable-dependency-tracking --disable-doxygen --with-apr=$APR_PATH --with-apr-util=$APR_UTILS_PATH
    make $FAST_MAKE -s V=0 && make install
    check_result $?
    cd $CURRENT_DIR
  else
    mkdir -p $LIB_DIR
    install_log4cxx_10
  fi
}

cleanup(){
  if [ -d $LIB_DIR ]; then
    cd $LIB_DIR
    rm -r libnice*
    rm -r libsrtp*
    rm -r libav*
    rm -r v11*
    rm -r openssl*
    rm -r opus*
    cd $CURRENT_DIR
  fi
}

parse_arguments $*

mkdir -p $PREFIX_DIR

install_apt_deps
check_proxy
install_log4cxx_10
install_openssl
install_libnice
install_libsrtp

install_opus
if [ "$ENABLE_GPL" = "true" ]; then
  install_mediadeps
else
  install_mediadeps_nogpl
fi

if [ "$CLEANUP" = "true" ]; then
  echo "Cleaning up..."
  cleanup
fi
