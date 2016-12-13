#!/bin/bash

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

pause() {
  if [ "$UNATTENDED" == "true" ]; then
    echo "$*"
  else
    read -p "$* [press Enter]"
  fi
}

parse_arguments(){
  while [ "$1" != "" ]; do
    case $1 in
      "--enable-gpl")
        ENABLE_GPL=true
        ;;
      "--unattended")
        UNATTENDED=true
        ;;
      "--disable-services")
        DISABLE_SERVICES=true
        ;;
      "--use-cache")
        CACHE=true
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

install_homebrew_from_cache(){
  if [ -f cache/homebrew-cache.tar.gz ]; then
    tar xzf cache/homebrew-cache.tar.gz --directory /usr/local/Cellar
    brew link glib pkg-config boost cmake yasm log4cxx gettext
  fi
}

copy_homebrew_to_cache(){
  mkdir cache
  tar czf cache/homebrew-cache.tar.gz --directory /usr/local/Cellar glib pkg-config boost cmake yasm log4cxx gettext
}

install_nvm_node() {
  curl -o- https://raw.githubusercontent.com/creationix/nvm/v0.32.1/install.sh | bash
  
  . $NVM_CHECK
  
  nvm install
  nvm use
}

install_homebrew(){
  if [ "$CACHE" == "true" ]; then
    install_homebrew_from_cache
  fi
  which -s brew
  if [[ $? != 0 ]] ; then
    # Install Homebrew
    ruby -e "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/master/install)"
  fi
}

install_brew_deps(){
  brew install glib pkg-config boost cmake yasm log4cxx gettext
  install_nvm_node
  npm install -g node-gyp
  if [ "$DISABLE_SERVICES" != "true" ]; then
    brew install rabbitmq mongodb
  fi
}

download_openssl() {
  OPENSSL_VERSION=$1
  OPENSSL_MAJOR="${OPENSSL_VERSION%?}"
  echo "Downloading OpenSSL from https://www.openssl.org/source/$OPENSSL_MAJOR/openssl-$OPENSSL_VERSION.tar.gz"
  curl -O https://www.openssl.org/source/openssl-$OPENSSL_VERSION.tar.gz
  tar -zxvf openssl-$OPENSSL_VERSION.tar.gz || DOWNLOAD_SUCCESS=$?
  if [ "$DOWNLOAD_SUCCESS" -eq 1 ]
  then
    echo "Downloading OpenSSL from https://www.openssl.org/source/old/$OPENSSL_MAJOR/openssl-$OPENSSL_VERSION.tar.gz"
    curl -O https://www.openssl.org/source/old/$OPENSSL_MAJOR/openssl-$OPENSSL_VERSION.tar.gz
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
      ./Configure --prefix=$PREFIX_DIR darwin64-x86_64-cc -shared -fPIC && make -s V=0 && make install_sw
    else
      echo "openssl already installed"
    fi
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
    curl -O https://nice.freedesktop.org/releases/libnice-0.1.7.tar.gz
    tar -zxvf libnice-0.1.7.tar.gz
    cd libnice-0.1.7
    check_result $?
    ./configure --prefix=$PREFIX_DIR && make -s V=0 && make install
    check_result $?
    cd $CURRENT_DIR
  else
    mkdir -p $LIB_DIR
    install_libnice
  fi
}

install_libsrtp(){
  cd $ROOT/third_party/srtp
  CFLAGS="-fPIC" ./configure --enable-openssl --prefix=$PREFIX_DIR
  make -s V=0 && make uninstall && make install
  check_result $?
  cd $CURRENT_DIR
}

install_mediadeps(){
  brew install opus libvpx x264
  if [ -d $LIB_DIR ]; then
    cd $LIB_DIR
    curl -O https://www.libav.org/releases/libav-11.6.tar.gz
    tar -zxvf libav-11.6.tar.gz
    cd libav-11.6
    curl -O https://github.com/libav/libav/commit/4d05e9392f84702e3c833efa86e84c7f1cf5f612.patch
    patch libavcodec/libvpxenc.c 4d05e9392f84702e3c833efa86e84c7f1cf5f612.patch && \
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
  brew install opus libvpx
  if [ -d $LIB_DIR ]; then
    cd $LIB_DIR
    curl -O https://www.libav.org/releases/libav-11.6.tar.gz
    tar -zxvf libav-11.6.tar.gz
    cd libav-11.6
    curl -O https://github.com/libav/libav/commit/4d05e9392f84702e3c833efa86e84c7f1cf5f612.patch
    patch libavcodec/libvpxenc.c 4d05e9392f84702e3c833efa86e84c7f1cf5f612.patch && \
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

parse_arguments $*

mkdir -p $LIB_DIR

pause "Installing homebrew..."
install_homebrew

pause "Installing deps via homebrew..."
install_brew_deps

pause 'Installing openssl...'
install_openssl

pause 'Installing liblibnice...'
install_libnice

pause 'Installing libsrtp...'
install_libsrtp

if [ "$ENABLE_GPL" = "true" ]; then
  pause "GPL libraries enabled, installing media dependencies..."
  install_mediadeps
else
  pause "No GPL libraries enabled, this disables h264 transcoding, to enable gpl please use the --enable-gpl option"
  install_mediadeps_nogpl
fi

if [ "$CACHE" == "true" ]; then
  copy_homebrew_to_cache
fi
