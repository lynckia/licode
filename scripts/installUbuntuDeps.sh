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

gcc_version=0

check_version(){
  local ubuntu_release
  ubuntu_release=$(lsb_release -rs)
  if [[ "$ubuntu_release" == "18.04" ]] || [[ "$ubuntu_release" == "20.04" ]]; then
     gcc_version=10
  elif [[ "$ubuntu_release" == "22.04" ]]; then
     gcc_version=11
  else
     gcc_version=12
  fi
}

check_sudo(){
  if [ -z `command -v sudo` ]; then
    echo 'sudo is not available, will install it.'
    apt-get update -y
    apt-get install sudo
  fi
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
  sudo apt-get update -y
  sudo apt-get install -qq python3-software-properties -y
  sudo apt-get install -qq software-properties-common -y
  sudo add-apt-repository ppa:ubuntu-toolchain-r/test -y
  sudo apt-get update -y
  check_version
  echo "Installing gcc $gcc_version"
  sudo apt-get install -qq git make gcc-$gcc_version g++-$gcc_version python3-pip libssl-dev cmake pkg-config libglib2.0-dev rabbitmq-server curl autoconf libtool automake xz-utils gnupg -y
  sudo update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-$gcc_version 60 --slave /usr/bin/g++ g++ /usr/bin/g++-$gcc_version
  echo "done"


  sudo chown -R `whoami` ~/.npm ~/tmp/ || true
}

install_mongodb(){
  local arch
  arch=$(uname -m)
  if [ "$arch" = "aarch64" ] || [ "$arch" = "arm64" ]; then
    echo "Installing mongodb-org from apt (aarch64)"
    local ubuntu_codename
    ubuntu_codename=$(lsb_release -cs)
    curl -fsSL https://www.mongodb.org/static/pgp/server-6.0.asc | sudo gpg -o /usr/share/keyrings/mongodb-server-6.0.gpg --dearmor --yes
    echo "deb [ arch=arm64,amd64 signed-by=/usr/share/keyrings/mongodb-server-6.0.gpg ] https://repo.mongodb.org/apt/ubuntu ${ubuntu_codename}/mongodb-org/6.0 multiverse" | \
      sudo tee /etc/apt/sources.list.d/mongodb-org-6.0.list
    sudo apt-get update -y
    sudo apt-get install -y mongodb-org
    return
  fi
  if [ -d $LIB_DIR ]; then
    echo "Installing mongodb-org from tar"
    sudo apt-get install -y libcurl4 openssl liblzma5
    local mongodb_tarball=mongodb-linux-x86_64-ubuntu2004-4.4.13.tgz
    wget -P $LIB_DIR "https://fastdl.mongodb.org/linux/${mongodb_tarball}"
    tar -zxvf "$LIB_DIR/${mongodb_tarball}" -C $LIB_DIR
    local mongodb_dir
    mongodb_dir=$(basename "$mongodb_tarball" .tgz)
    sudo ln -sf $LIB_DIR/${mongodb_dir}/bin/* /usr/local/bin/
  else
    mkdir -p $LIB_DIR
    install_mongodb
  fi
}

install_conan(){
  sudo pip3 install conan==1.62.0
}

install_cpplint(){
   sudo pip3 install cpplint==1.5.4
}



download_openssl() {
  OPENSSL_VERSION=$1
  OPENSSL_MAJOR="${OPENSSL_VERSION%?}"
  echo "Downloading OpenSSL from https://www.openssl.org/source/$OPENSSL_MAJOR/openssl-$OPENSSL_VERSION.tar.gz"
  curl -OL https://www.openssl.org/source/openssl-$OPENSSL_VERSION.tar.gz
  tar -zxvf openssl-$OPENSSL_VERSION.tar.gz
  DOWNLOAD_SUCCESS=$?
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
      ./config --prefix=$PREFIX_DIR --openssldir=$PREFIX_DIR -fPIC
      make $FAST_MAKE -s V=0
      make install_sw
      # OpenSSL installs into lib (e.g. aarch64) or lib64 (e.g. x86_64).
      # Erizo expects libraries under build/libdeps/build/lib.
      if [ -d "${PREFIX_DIR}/lib64" ]; then
        mkdir -p "${PREFIX_DIR}/lib"
        cp -a "${PREFIX_DIR}/lib64/." "${PREFIX_DIR}/lib/"
      fi
    else
      echo "openssl already installed"
    fi
    cd $CURRENT_DIR
  else
    mkdir -p $LIB_DIR
    install_openssl
  fi
}

install_opus(){
  [ -d $LIB_DIR ] || mkdir -p $LIB_DIR
  cd $LIB_DIR
  if [ ! -f ./opus-1.1.tar.gz ]; then
    curl -L https://github.com/xiph/opus/archive/v1.1.tar.gz -o opus-1.1.tar.gz
    tar -zxvf opus-1.1.tar.gz
    cd opus-1.1
    ./autogen.sh
    ./configure --prefix=$PREFIX_DIR
    make $FAST_MAKE -s V=0
    make install
  else
    echo "opus already installed"
  fi
  cd $CURRENT_DIR
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
  install_opus
  sudo apt-get -qq install nasm libvpx-dev libx264-dev
  if [ -d $LIB_DIR ]; then
    cd $LIB_DIR
    if [ ! -f ./${FFMPEG_TAG}.tar.gz ]; then
      download_ffmpeg
      cd FFmpeg-${FFMPEG_TAG}
      PKG_CONFIG_PATH=${PREFIX_DIR}/lib/pkgconfig ./configure \
        --prefix=$PREFIX_DIR --enable-shared --disable-static --enable-gpl \
        --enable-libvpx --enable-libx264 --enable-libopus --disable-doc --disable-programs
      make $FAST_MAKE -s V=0
      make install
    else
      echo "ffmpeg already installed"
    fi
    cd $CURRENT_DIR
  else
    mkdir -p $LIB_DIR
    install_mediadeps
  fi

}

install_mediadeps_nogpl(){
  install_opus
  sudo apt-get -qq install nasm libvpx-dev
  if [ -d $LIB_DIR ]; then
    cd $LIB_DIR
    if [ ! -f ./${FFMPEG_TAG}.tar.gz ]; then
      download_ffmpeg
      cd FFmpeg-${FFMPEG_TAG}
      PKG_CONFIG_PATH=${PREFIX_DIR}/lib/pkgconfig ./configure \
        --prefix=$PREFIX_DIR --enable-shared --disable-static \
        --enable-libvpx --enable-libopus --disable-doc --disable-programs
      make $FAST_MAKE -s V=0
      make install
    else
      echo "ffmpeg already installed"
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
    curl -o libsrtp-2.4.2.tar.gz https://codeload.github.com/cisco/libsrtp/tar.gz/v2.4.2
    tar -zxvf libsrtp-2.4.2.tar.gz
    cd libsrtp-2.4.2
    CFLAGS="-fPIC" ./configure --enable-openssl --prefix=$PREFIX_DIR --with-openssl-dir=$PREFIX_DIR
    make $FAST_MAKE -s V=0 && make uninstall && make install
    cd $CURRENT_DIR
  else
    mkdir -p $LIB_DIR
    install_libsrtp
  fi
}

install_libnice(){
  if [ -d $LIB_DIR ]; then
    cd $LIB_DIR
    if [ ! -f ./libnice-0.1.17.tar.gz ]; then
      curl -OL https://nice.freedesktop.org/releases/libnice-0.1.17.tar.gz
      tar -zxvf libnice-0.1.17.tar.gz
      cd libnice-0.1.17
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

cleanup(){
  if [ -d $LIB_DIR ]; then
    (
      cd $LIB_DIR
      shopt -s nullglob
      rm -rf libsrtp* FFmpeg* n6* openssl* opus* mongodb*.tgz libnice*
    )
  fi
}

parse_arguments $*

mkdir -p $PREFIX_DIR

check_sudo
install_apt_deps
install_mongodb
install_conan
check_proxy
install_openssl
install_libsrtp
install_opus
install_libnice
install_cpplint

if [ "$ENABLE_GPL" = "true" ]; then
  install_mediadeps
else
  install_mediadeps_nogpl
fi

if [ "$CLEANUP" = "true" ]; then
  echo "Cleaning up..."
  cleanup
fi
