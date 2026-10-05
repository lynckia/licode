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

set_brew_prefix(){
  if command -v brew >/dev/null 2>&1; then
    BREW_PREFIX=$(brew --prefix)
  elif [ "$(uname -m)" = "arm64" ]; then
    BREW_PREFIX=/opt/homebrew
  else
    BREW_PREFIX=/usr/local
  fi
  export PATH="$BREW_PREFIX/bin:$PATH"
  export PKG_CONFIG_PATH="${PKG_CONFIG_PATH:+$PKG_CONFIG_PATH:}$BREW_PREFIX/lib/pkgconfig"
  if [ "$(uname)" = "Darwin" ] && [ "$(sysctl -n sysctl.proc_translated 2>/dev/null)" = "1" ]; then
    echo "WARNING: this shell is running under Rosetta (x86_64). Prefer a native arm64 terminal for Apple Silicon builds."
  fi
}

install_homebrew_from_cache(){
  set_brew_prefix
  if [ -f cache/homebrew-cache.tar.gz ]; then
    tar xzf cache/homebrew-cache.tar.gz --directory "$BREW_PREFIX/Cellar"
    brew link pkg-config cmake nasm gettext coreutils
  fi
}

copy_homebrew_to_cache(){
  set_brew_prefix
  mkdir cache
  tar czf cache/homebrew-cache.tar.gz --directory "$BREW_PREFIX/Cellar" pkg-config cmake nasm gettext
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

install_cmake() {
  mkdir -p "$PREFIX_DIR"
  if [ ! -f ./cmake.tar.gz ]; then
    echo "Downloading CMake 3.31.12 source..."
    curl -L -o "$LIB_DIR/cmake.tar.gz" https://github.com/Kitware/CMake/releases/download/v3.31.12/cmake-3.31.12.tar.gz
    
    echo "Extracting CMake..."
    tar -xzf "$LIB_DIR/cmake.tar.gz" -C "$LIB_DIR"
    
    cd $LIB_DIR/cmake-3.31.12
    ./configure --prefix=$PREFIX_DIR && make $FAST_MAKE -s V=0 && make install
    
    cd $CURRENT_DIR
    rm "$LIB_DIR/cmake.tar.gz"
    rm -rf $LIB_DIR/cmake-3.31.12
  fi
  export CMAKE_CMD="$PREFIX_DIR/bin/cmake"
  export CMAKE_BIN="$PREFIX_DIR/bin/cmake"
}

install_conan(){
  pip install conan==1.62.0
}

install_cpplint(){
   pip install cpplint==1.5.4
}
install_brew_deps(){
  set_brew_prefix
  brew install pkg-config glib nasm gettext coreutils
  install_nvm_node
  nvm use
  npm install
  if [ "$DISABLE_SERVICES" != "true" ]; then
    brew tap mongodb/brew
    brew install rabbitmq mongodb-community@8.0
  fi
  install_cmake
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
    OPENSSL_VERSION=${OPENSSL_VERSION%+*}
    if [ ! -f ./openssl-$OPENSSL_VERSION.tar.gz ]; then
      download_openssl $OPENSSL_VERSION
      cd openssl-$OPENSSL_VERSION
      case "$(uname -m)" in
        arm64) OPENSSL_TARGET=darwin64-arm64-cc ;;
        *)     OPENSSL_TARGET=darwin64-x86_64-cc ;;
      esac
      ./Configure --prefix=$PREFIX_DIR --openssldir=$PREFIX_DIR $OPENSSL_TARGET -shared -fPIC && make $FAST_MAKE -s V=0 && make install_sw
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
    curl -OL https://nice.freedesktop.org/releases/libnice-0.1.17.tar.gz
    tar -zxvf libnice-0.1.17.tar.gz
    cd libnice-0.1.17
    check_result $?
    ./configure --prefix=$PREFIX_DIR && make $FAST_MAKE -s V=0 && make install
    check_result $?
    cd $CURRENT_DIR
  else
    mkdir -p $LIB_DIR
    install_libnice
  fi
}

install_libsrtp(){
  if [ -d $LIB_DIR ]; then
    cd $LIB_DIR
    curl -o libsrtp-2.4.2.tar.gz https://codeload.github.com/cisco/libsrtp/tar.gz/v2.4.2
    tar -zxvf libsrtp-2.4.2.tar.gz
    cd libsrtp-2.4.2
    CFLAGS="-fPIC" ./configure --enable-openssl --prefix=$PREFIX_DIR --with-openssl-dir=$PREFIX_DIR
    make $FAST_MAKE -s V=0 && make uninstall && make install
    check_result $?
    cd $CURRENT_DIR
  else
    mkdir -p $LIB_DIR
    install_libsrtp
  fi
}

FFMPEG_VERSION=6.1.1
FFMPEG_TAG=n${FFMPEG_VERSION}

download_ffmpeg(){
  if [ ! -f ./${FFMPEG_TAG}.tar.gz ]; then
    curl -L -o ${FFMPEG_TAG}.tar.gz https://github.com/FFmpeg/FFmpeg/archive/refs/tags/${FFMPEG_TAG}.tar.gz
  fi
  tar -zxf ${FFMPEG_TAG}.tar.gz
}

install_mediadeps(){
  set_brew_prefix
  brew install opus libvpx x264
  if [ -d $LIB_DIR ]; then
    cd $LIB_DIR
    download_ffmpeg
    cd FFmpeg-${FFMPEG_TAG}
    PKG_CONFIG_PATH=${PREFIX_DIR}/lib/pkgconfig:$BREW_PREFIX/lib/pkgconfig ./configure \
      --prefix=$PREFIX_DIR --enable-shared --disable-static --enable-gpl \
      --enable-libvpx --enable-libx264 --enable-libopus --disable-doc --disable-programs && \
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
  set_brew_prefix
  brew install opus libvpx
  if [ -d $LIB_DIR ]; then
    cd $LIB_DIR
    download_ffmpeg
    cd FFmpeg-${FFMPEG_TAG}
    PKG_CONFIG_PATH=${PREFIX_DIR}/lib/pkgconfig:$BREW_PREFIX/lib/pkgconfig ./configure \
      --prefix=$PREFIX_DIR --enable-shared --disable-static \
      --enable-libvpx --enable-libopus --disable-doc --disable-programs && \
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

# install_homebrew
set_brew_prefix

# pause "Installing deps via homebrew..."
install_brew_deps

pause "Installing conan..."
install_conan

pause 'Installing openssl...'
install_openssl

pause 'Installing libsrtp...'
install_libsrtp

pause 'Installing libnice...'
install_libnice

install_cpplint

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
