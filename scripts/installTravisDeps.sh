#!/bin/bash
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

install_apt_deps(){
  sudo apt-get install -qq make gcc libssl-dev cmake libsrtp0-dev libsrtp0 libnice10 libnice-dev libglib2.0-dev pkg-config libboost-regex-dev libboost-thread-dev libboost-system-dev liblog4cxx10-dev curl
  #sudo /usr/bin/npm install -g node-gyp
  ls ./build/
  sudo chown -R `whoami` ~/.npm ~/tmp/
}

install_openssl(){
  if [ -d $LIB_DIR ]; then
    cd $LIB_DIR
    curl -O http://www.openssl.org/source/openssl-1.0.1e.tar.gz
    tar -zxvf openssl-1.0.1e.tar.gz > /dev/null 2> /dev/null
    cd openssl-1.0.1e
    ./config --prefix=$PREFIX_DIR -fPIC
    make -s V=0
    sudo make install
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
    tar -zxvf libnice-0.1.4.tar.gz > /dev/null 2> /dev/null
    cd libnice-0.1.4
    ./configure --prefix=$PREFIX_DIR
    make -s V=0
    sudo make install
    cd $CURRENT_DIR
  else
    mkdir -p $LIB_DIR
    install_libnice
  fi
}

install_mediadeps(){
  sudo apt-get install yasm libvpx. libx264.
  if [ -d $LIB_DIR ]; then
    cd $LIB_DIR
    curl -O https://www.libav.org/releases/libav-9.9.tar.gz
    tar -zxvf libav-9.9.tar.gz > /dev/null 2> /dev/null
    cd libav-9.9
    ./configure --prefix=$PREFIX_DIR --enable-shared --enable-gpl --enable-libvpx --enable-libx264
    make -s V=0
    sudo make install
    cd $CURRENT_DIR
  else
    mkdir -p $LIB_DIR
    install_mediadeps
  fi

}

install_mediadeps_nogpl(){
  sudo apt-get install yasm libvpx.
  if [ -d $LIB_DIR ]; then
    cd $LIB_DIR
    curl -O https://www.libav.org/releases/libav-9.9.tar.gz
    tar -zxvf libav-9.9.tar.gz > /dev/null 2> /dev/null
    cd libav-9.9
    ./configure --prefix=$PREFIX_DIR --enable-shared --enable-libvpx
    make -s V=0
    sudo make install
    cd $CURRENT_DIR
  else
    mkdir -p $LIB_DIR
    install_mediadeps_nogpl
  fi
}


parse_arguments $*

mkdir -p $PREFIX_DIR

echo "Installing deps via apt-get... [press Enter]"
install_apt_deps

echo "Installing openssl library...  [press Enter]"
install_openssl

echo "Installing libnice library...  [press Enter]"
install_libnice

if [ "$ENABLE_GPL" = "true" ]; then
  echo "GPL libraries enabled"
  install_mediadeps
else
  echo "No GPL libraries enabled, this disables h264 transcoding, to enable gpl please use the --enable-gpl option"
  install_mediadeps_nogpl
fi
