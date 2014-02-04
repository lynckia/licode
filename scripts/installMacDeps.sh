#!/bin/bash
SCRIPT=`pwd`/$0
FILENAME=`basename $SCRIPT`
PATHNAME=`dirname $SCRIPT`
ROOT=$PATHNAME/..
BUILD_DIR=$ROOT/build
CURRENT_DIR=`pwd`

LIB_DIR=$BUILD_DIR/libdeps
PREFIX_DIR=$LIB_DIR/build/

pause() {
  read -p "$*"
}

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

install_homebrew(){
  ruby -e "$(curl -fsSkL raw.github.com/mxcl/homebrew/go/install)"
}

install_brew_deps(){
  brew install glib pkg-config boost cmake node mongodb rabbitmq log4cxx
  npm install -g node-gyp
}

install_openssl(){
  if [ -d $LIB_DIR ]; then
    cd $LIB_DIR
    curl -O http://www.openssl.org/source/openssl-1.0.1e.tar.gz
    tar -zxvf openssl-1.0.1e.tar.gz
    cd openssl-1.0.1e
    ./Configure --prefix=$PREFIX_DIR darwin64-x86_64-cc -fPIC
    make -s V=0
    make install
    cd $CURRENT_DIR
  else
    mkdir -p $LIB_DIR
    install_openssl
  fi
}

install_libnice(){
  if [ -d $LIB_DIR ]; then
    cd $LIB_DIR
    curl -O http://nice.freedesktop.org/releases/libnice-0.1.4.tar.gz
    tar -zxvf libnice-0.1.4.tar.gz
    cd libnice-0.1.4
    patch -R ./agent/conncheck.c < $PATHNAME/libnice-014.patch0
    echo nice_agent_set_port_range >> nice/libnice.sym
    ./configure --prefix=$PREFIX_DIR
    make -s V=0
    make install
    cd $CURRENT_DIR
  else
    mkdir -p $LIB_DIR
    install_libnice
  fi
}

install_libsrtp(){
  cd $ROOT/third_party/srtp
  CFLAGS="-fPIC" ./configure --prefix=$PREFIX_DIR
  make -s V=0
  make uninstall
  make install
  cd $CURRENT_DIR
}

install_mediadeps(){
  brew install yasm libvpx x264
  if [ -d $LIB_DIR ]; then
    cd $LIB_DIR
    curl -O https://www.libav.org/releases/libav-9.9.tar.gz
    tar -zxvf libav-9.9.tar.gz
    cd libav-9.9
    ./configure --prefix=$PREFIX_DIR --enable-shared --enable-gpl --enable-libvpx --enable-libx264
    make -s V=0
    make install
    cd $CURRENT_DIR
  else
    mkdir -p $LIB_DIR
    install_mediadeps
  fi
}

install_mediadeps_nogpl(){
  brew install yasm libvpx
  if [ -d $LIB_DIR ]; then
    cd $LIB_DIR
    curl -O https://www.libav.org/releases/libav-9.9.tar.gz
    tar -zxvf libav-9.9.tar.gz
    cd libav-9.9
    ./configure --prefix=$PREFIX_DIR --enable-shared --enable-libvpx
    make -s V=0
    make install
    cd $CURRENT_DIR
  else
    mkdir -p $LIB_DIR
    install_mediadeps_nogpl
  fi
}

parse_arguments $*

mkdir -p $LIB_DIR

pause "Installing homebrew... [press Enter]"
install_homebrew

pause "Installing deps via homebrew... [press Enter]"
install_brew_deps

pause 'Installing openssl... [press Enter]'
install_openssl

pause 'Installing libnice... [press Enter]'
install_libnice

pause 'Installing libsrtp... [press Enter]'
install_libsrtp

if [ "$ENABLE_GPL" = "true" ]; then
  pause "GPL libraries enabled, installing media dependencies..."
  install_mediadeps
else
  pause "No GPL libraries enabled, this disables h264 transcoding, to enable gpl please use the --enable-gpl option"
  install_mediadeps_nogpl
fi
