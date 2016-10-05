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

install_apt_deps(){
  sudo yum -y install git make gcc openssl-devel cmake pkgconfig nodejs boost-devel boost-regex boost-thread boost-system log4cxx-devel rabbitmq-server mongodb mongodb-server curl boost-test tar xz libffi-devel npm yasm java-1.7.0-openjdk
  sudo chown -R `whoami` ~/.npm ~/tmp/
}

install_vpx(){
  [ -d $LIB_DIR ] || mkdir -p $LIB_DIR
  cd $LIB_DIR
  curl -O https://webm.googlecode.com/files/libvpx-v1.0.0.tar.bz2
  tar -xf libvpx-v1.0.0.tar.bz2
  cd libvpx-v1.0.0
  ./configure --prefix=$PREFIX_DIR --enable-vp8 --enable-shared --enable-pic
  make -s V=0
  make install
  cd $CURRENT_DIR

}

install_glib2(){
  if [ -d $LIB_DIR ]; then
    cd $LIB_DIR
    curl -O http://ftp.gnome.org/pub/gnome/sources/glib/2.38/glib-2.38.2.tar.xz
    tar -xvf glib-2.38.2.tar.xz
    cd glib-2.38.2
    ./configure --prefix=$PREFIX_DIR
    make
    make install
    cd $CURRENT_DIR
  else
    mkdir -p $LIB_DIR
    install_libnice
  fi

}

parse_arguments $*

mkdir -p $PREFIX_DIR

install_apt_deps

check_proxy

install_glib2

install_vpx
