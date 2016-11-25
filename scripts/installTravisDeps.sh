#!/bin/bash

set -e

SCRIPT=`pwd`/$0
FILENAME=`basename $SCRIPT`
PATHNAME=`dirname $SCRIPT`
ROOT=$PATHNAME/..
BUILD_DIR=$ROOT/build
CURRENT_DIR=`pwd`

LIB_DIR=$BUILD_DIR/libdeps
PREFIX_DIR=$LIB_DIR/build/

parse_arguments(){
  while [ "$1" != "" ]; do
    case $1 in
      "--enable-gpl")
        ENABLE_GPL=true
        ;;
    esac
    shift
  done
}

check_result() {
  if [ "$1" -eq 1 ]
  then
    exit 1
  fi
}

install_apt_deps(){
  npm install -g node-gyp
  sudo chown -R `whoami` ~/.npm ~/tmp/ || true
}

install_openssl(){
  if [ -d $LIB_DIR ]; then
    cd $LIB_DIR
    curl -O https://www.openssl.org/source/openssl-1.0.1l.tar.gz
    tar -zxvf openssl-1.0.1l.tar.gz > /dev/null 2> /dev/null
    cd openssl-1.0.1l
    ./config --prefix=$PREFIX_DIR -fPIC && make -s V=0 && make install
    check_result $?
    cd $CURRENT_DIR
  else
    mkdir -p $LIB_DIR
    install_openssl
  fi
}

install_libnice(){
  if [ -d $LIB_DIR ]; then
    cd $LIB_DIR
    curl -O https://nice.freedesktop.org/releases/libnice-0.1.13.tar.gz
    tar -zxvf libnice-0.1.13.tar.gz
    cd libnice-0.1.13
    ./configure --prefix=$PREFIX_DIR && make -s V=0 && make install
    check_result $?
    cd $CURRENT_DIR
  else
    mkdir -p $LIB_DIR
    install_libnice
  fi
}

install_opus(){
  [ -d $LIB_DIR ] || mkdir -p $LIB_DIR
  cd $LIB_DIR
  curl -O http://downloads.xiph.org/releases/opus/opus-1.1.tar.gz
  tar -zxvf opus-1.1.tar.gz
  cd opus-1.1
  ./configure --prefix=$PREFIX_DIR && make -s V=0 && make install
  check_result $?
  cd $CURRENT_DIR
}

install_mediadeps(){
  if [ -d $LIB_DIR ]; then
    cd $LIB_DIR
    curl -O https://www.libav.org/releases/libav-11.1.tar.gz
    tar -zxvf libav-11.1.tar.gz
    cd libav-11.1
    PKG_CONFIG_PATH=${PREFIX_DIR}/lib/pkgconfig ./configure --prefix=$PREFIX_DIR --enable-shared --enable-gpl --enable-libvpx --enable-libx264 --enable-libopus && \
      make -s V=0 && \
      make install
    check_result $?
    cd $CURRENT_DIR
  else
    mkdir -p $LIB_DIR
    install_mediadeps
  fi

}

install_mediadeps_nogpl(){
  if [ -d $LIB_DIR ]; then
    cd $LIB_DIR
    curl -O https://www.libav.org/releases/libav-11.1.tar.gz
    tar -zxvf libav-11.1.tar.gz
    cd libav-11.1
    PKG_CONFIG_PATH=${PREFIX_DIR}/lib/pkgconfig ./configure --prefix=$PREFIX_DIR --enable-shared --enable-libvpx --enable-libopus && \
      make -s V=0 && \
      make install
    check_result $?
    cd $CURRENT_DIR
  else
    mkdir -p $LIB_DIR
    install_mediadeps_nogpl
  fi
}

install_libsrtp(){
  cd $ROOT/third_party/srtp
  CFLAGS="-fPIC" ./configure --prefix=$PREFIX_DIR && make -s V=0 && make install
  cd $CURRENT_DIR
}

parse_arguments $*

mkdir -p $PREFIX_DIR

echo "Installing deps via apt-get..."
install_apt_deps

echo "Installing openssl library..."
install_openssl

echo "Installing libnice library..."
install_libnice

echo "Installing libsrtp library..."
install_libsrtp

echo "Installing opus library..."
install_opus

if [ "$ENABLE_GPL" = "true" ]; then
  echo "GPL libraries enabled"
  install_mediadeps
else
  echo "No GPL libraries enabled, this disables h264 transcoding, to enable gpl please use the --enable-gpl option"
  install_mediadeps_nogpl
fi

echo "Done"
