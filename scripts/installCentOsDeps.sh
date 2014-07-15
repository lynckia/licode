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
      "--cleanup")
        CLEANUP=true
        ;;
    esac
    shift
  done
}

install_apt_deps(){
  sudo yum install git make gcc openssl-devel cmake pkgconfig nodejs boost-devel boost-regex boost-thread boost-system log4cxx-devel rabbitmq-server mongodb mongodb-server curl boost-test tar xz libffi-devel npm yasm
  sudo chown -R `whoami` ~/.npm ~/tmp/
}

install_openssl(){
  if [ -d $LIB_DIR ]; then
    cd $LIB_DIR
    curl -O http://www.openssl.org/source/openssl-1.0.1g.tar.gz
    tar -zxvf openssl-1.0.1g.tar.gz
    cd openssl-1.0.1g
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
    PKG_CONFIG_PATH=${PREFIX_DIR}/lib/pkgconfig ./configure --prefix=$PREFIX_DIR 
    make -s V=0
    make install
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
  ./configure --prefix=$PREFIX_DIR
  make -s V=0
  make install
  cd $CURRENT_DIR
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

install_mediadeps(){
  sudo apt-get install yasm libvpx. libx264.
  if [ -d $LIB_DIR ]; then
    cd $LIB_DIR
    curl -O https://www.libav.org/releases/libav-9.13.tar.gz
    tar -zxf libav-9.13.tar.gz
    cd libav-9.13
    PKG_CONFIG_PATH=${PREFIX_DIR}/lib/pkgconfig CPATH=${PREFIX_DIR}/include ./configure --prefix=$PREFIX_DIR --enable-shared --enable-gpl --enable-libvpx --enable-libx264 --enable-libopus
    make -s V=0
    make install
    cd $CURRENT_DIR
  else
    mkdir -p $LIB_DIR
    install_mediadeps
  fi

}

install_mediadeps_nogpl(){
#  sudo apt-get install yasm libvpx.
  if [ -d $LIB_DIR ]; then
    cd $LIB_DIR
    curl -O https://www.libav.org/releases/libav-9.13.tar.gz
    tar -zxf libav-9.13.tar.gz
    cd libav-9.13
    PKG_CONFIG_PATH=${PREFIX_DIR}/lib/pkgconfig CPATH=${PREFIX_DIR}/include ./configure --prefix=$PREFIX_DIR --enable-shared --enable-libopus --enable-libvpx
    CPATH=${PREFIX_DIR}/include make -s V=0
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
cleanup(){  
  if [ -d $LIB_DIR ]; then
    cd $LIB_DIR
    rm -r libnice*
    rm -r libav*
    rm -r openssl*
    cd $CURRENT_DIR
  fi
}

parse_arguments $*

mkdir -p $PREFIX_DIR

pause "Installing deps via apt-get... [press Enter]"
install_apt_deps

pause "Installing glib2 library...  [press Enter]"
install_glib2

pause "Installing openssl library...  [press Enter]"
install_openssl

pause "Installing libnice library...  [press Enter]"
install_libnice

pause "Installing libsrtp library...  [press Enter]"
install_libsrtp

pause "Installing opus library...  [press Enter]"

install_opus
install_vpx

if [ "$ENABLE_GPL" = "true" ]; then
  pause "GPL libraries enabled"
  install_mediadeps
else
  pause "No GPL libraries enabled, this disables h264 transcoding, to enable gpl please use the --enable-gpl option"
  install_mediadeps_nogpl
fi

if [ "$CLEANUP" = "true" ]; then
  echo "Cleaning up..."
  cleanup
fi
