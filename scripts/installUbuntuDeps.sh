#!/bin/bash
SCRIPT=`pwd`/$0
FILENAME=`basename $SCRIPT`
PATHNAME=`dirname $SCRIPT`
ROOT=$PATHNAME/..
BUILD_DIR=$ROOT/build
CURRENT_DIR=`pwd`

LIB_DIR=$BUILD_DIR/libdeps
PREFIX_DIR=$LIB_DIR/build/


prepare_build_dir() {
    ACTUAL_BUILD_DIR=$ROOT/../licode_build

    if [ ! -d $ACTUAL_BUILD_DIR ]; then
      if [ -d $BUILD_DIR ]; then
        mv $BUILD_DIR $ACTUAL_BUILD_DIR
      else
        mkdir $ACTUAL_BUILD_DIR
      fi
    fi
    if [ -d $BUILD_DIR ]; then
      rm -rf $BUILD_DIR
    fi
    if [ ! -L $BUILD_DIR ]; then
      ln -s ../licode_build $BUILD_DIR
    fi
}

pause() {
  echo "$*"
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

install_apt_deps(){
  sudo apt-get install -y python-software-properties
  sudo apt-get install -y software-properties-common
  sudo add-apt-repository -y ppa:chris-lea/node.js
  sudo apt-get -y update
  sudo apt-get -y install git make gcc g++ libssl-dev cmake libglib2.0-dev pkg-config nodejs libboost-regex-dev libboost-thread-dev libboost-system-dev liblog4cxx10-dev rabbitmq-server mongodb openjdk-6-jre curl
  sudo npm install -g node-gyp
  sudo chown -R `whoami` ~/.npm ~/tmp/
}

install_openssl(){
  if [ -d $LIB_DIR ]; then
    cd $LIB_DIR
    curl -O http://www.openssl.org/source/openssl-1.0.1e.tar.gz
    tar -zxvf openssl-1.0.1e.tar.gz
    cd openssl-1.0.1e
    ./config --prefix=$PREFIX_DIR -fPIC
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
    ./configure --prefix=$PREFIX_DIR
    make -s V=0
    make install
    cd $CURRENT_DIR
  else
    mkdir -p $LIB_DIR
    install_libnice
  fi
}

install_mediadeps(){
  sudo apt-get -y install yasm libvpx. libx264.
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
  sudo apt-get -y install yasm libvpx.
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

install_libsrtp(){
  cd $ROOT/third_party/srtp
  CFLAGS="-fPIC" ./configure --prefix=$PREFIX_DIR
  make -s V=0
  make uninstall
  make install
  cd $CURRENT_DIR
}

parse_arguments $*

mkdir -p $PREFIX_DIR

prepare_build_dir

pause "Installing deps via apt-get... [press Enter]"
install_apt_deps

pause "Installing openssl library...  [press Enter]"
install_openssl

pause "Installing libnice library...  [press Enter]"
install_libnice

pause "Installing libsrtp library...  [press Enter]"
install_libsrtp

if [ "$ENABLE_GPL" = "true" ]; then
  pause "GPL libraries enabled"
  install_mediadeps
else
  pause "No GPL libraries enabled, this disables h264 transcoding, to enable gpl please use the --enable-gpl option"
  install_mediadeps_nogpl
fi
