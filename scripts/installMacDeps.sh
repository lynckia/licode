#!/usr/bin/env bash

set -e

SCRIPT=`pwd`/$0
FILENAME=`basename $SCRIPT`
PATHNAME=`dirname $SCRIPT`
ROOT=$PATHNAME/..
BUILD_DIR=$ROOT/build
CURRENT_DIR=`pwd`
NVM_CHECK="$PATHNAME"/checkNvm.sh
FAST_MAKE=''

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
      "--fast")
        FAST_MAKE='-j4'
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
    brew link pkg-config boost cmake yasm log4cxx gettext coreutils
  fi
}

copy_homebrew_to_cache(){
  mkdir cache
  tar czf cache/homebrew-cache.tar.gz --directory /usr/local/Cellar pkg-config boost cmake yasm log4cxx gettext coreutils
}

install_nvm_node() {
  if [ -d $LIB_DIR ]; then
    export NVM_DIR=$(greadlink -f "$LIB_DIR/nvm")
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
  brew install pkg-config boost cmake yasm log4cxx gettext coreutils conan
  install_nvm_node
  nvm use
  npm install
  if [ "$DISABLE_SERVICES" != "true" ]; then
    brew tap mongodb/brew
    brew install rabbitmq mongodb-community@4.0
  fi
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
      ./Configure --prefix=$PREFIX_DIR --openssldir=$PREFIX_DIR darwin64-x86_64-cc -shared -fPIC && make $FAST_MAKE -s V=0 && make install_sw
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

install_libsrtp(){
  if [ -d $LIB_DIR ]; then
    cd $LIB_DIR
    curl -o libsrtp-2.1.0.tar.gz https://codeload.github.com/cisco/libsrtp/tar.gz/v2.1.0
    tar -zxvf libsrtp-2.1.0.tar.gz
    cd libsrtp-2.1.0
    CFLAGS="-fPIC" ./configure --enable-openssl --prefix=$PREFIX_DIR --with-openssl-dir=$PREFIX_DIR
    make $FAST_MAKE -s V=0 && make uninstall && make install
    check_result $?
    cd $CURRENT_DIR
  else
    mkdir -p $LIB_DIR
    install_libsrtp
  fi
}

install_mediadeps(){
  brew install opus libvpx x264
  if [ -d $LIB_DIR ]; then
    cd $LIB_DIR
    curl -O -L https://github.com/libav/libav/archive/v11.6.tar.gz
    tar -zxvf v11.6.tar.gz
    cd libav-11.6
    curl -OL https://github.com/libav/libav/commit/4d05e9392f84702e3c833efa86e84c7f1cf5f612.patch
    patch libavcodec/libvpxenc.c 4d05e9392f84702e3c833efa86e84c7f1cf5f612.patch && \
    PKG_CONFIG_PATH=${PREFIX_DIR}/lib/pkgconfig ./configure --prefix=$PREFIX_DIR --enable-shared --enable-gpl --enable-libvpx --enable-libx264 --enable-libopus --disable-doc && \
    make $FAST_MAKE -s V=0 && \
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
    curl -O -L https://github.com/libav/libav/archive/v11.6.tar.gz
    tar -zxvf v11.6.tar.gz
    cd libav-11.6
    curl -OL https://github.com/libav/libav/commit/4d05e9392f84702e3c833efa86e84c7f1cf5f612.patch
    patch libavcodec/libvpxenc.c 4d05e9392f84702e3c833efa86e84c7f1cf5f612.patch && \
    PKG_CONFIG_PATH=${PREFIX_DIR}/lib/pkgconfig ./configure --prefix=$PREFIX_DIR --enable-shared --enable-libvpx --enable-libopus --disable-doc && \
    make $FAST_MAKE -s V=0 && \
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
