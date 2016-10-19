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
  sudo yum install python-software-properties
  sudo yum install software-properties-common
  sudo add-apt-repository ppa:chris-lea/node.js
  sudo yum update
  sudo yum install git make gcc g++ libssl-dev cmake libglib2.0-dev pkg-config nodejs libboost-regex-dev libboost-thread-dev libboost-system-dev liblog4cxx10-dev rabbitmq-server mongodb openjdk-6-jre curl libboost-test-dev
  sudo npm install -g node-gyp
  sudo chown -R `whoami` ~/.npm ~/tmp/
}

install_openssl(){
  if [ -d $LIB_DIR ]; then
    cd $LIB_DIR
    #curl -O https://www.openssl.org/source/old/1.0.1/openssl-1.0.1l.tar.gz
    tar -zxvf openssl-1.0.1l.tar.gz
    cd openssl-1.0.1l
    ./config --prefix=$PREFIX_DIR -fPIC
    make -s V=0
    make install_sw
    cd $CURRENT_DIR
  else
    mkdir -p $LIB_DIR
    install_openssl
  fi
}

install_libnice(){
  if [ -d $LIB_DIR ]; then
    cd $LIB_DIR
    #curl -O https://nice.freedesktop.org/releases/libnice-0.1.4.tar.gz
    tar -zxvf libnice-0.1.4.tar.gz
    cd libnice-0.1.4
    patch -R ./agent/conncheck.c < $PATHNAME/libnice-014.patch0
    patch -p1 < $PATHNAME/libnice-014.patch1
    ./configure --prefix=$PREFIX_DIR
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
  #curl -O http://downloads.xiph.org/releases/opus/opus-1.1.3.tar.gz
  tar -zxvf opus-1.1.3.tar.gz
  cd opus-1.1.3
  ./configure --prefix=$PREFIX_DIR
  make -s V=0
  make install
  cd $CURRENT_DIR
}

install_mediadeps(){
  sudo yum install yasm libvpx. libx264. fdk-aac-devel
  if [ -d $LIB_DIR ]; then
    cd $LIB_DIR
    git clone https://git.ffmpeg.org/ffmpeg.git ffmpeg
    cd ffmpeg
    PKG_CONFIG_PATH=${PREFIX_DIR}/lib/pkgconfig ./configure --prefix=/usr --bindir=/usr/bin --datadir=/usr/share/ffmpeg --incdir=/usr/include/ffmpeg --libdir=/usr/lib64 --mandir=/usr/share/man --arch=x86_64 --optflags='-O2 -g -pipe -Wall -Wp,-D_FORTIFY_SOURCE=2 -fexceptions -fstack-protector-strong --param=ssp-buffer-size=4 -grecord-gcc-switches -m64 -mtune=generic' --enable-bzlib --disable-crystalhd --enable-nonfree --enable-libfdk-aac --enable-libfreetype --enable-libvpx --enable-libopus --enable-libx264 --enable-avfilter --enable-avresample --enable-postproc --enable-pthreads --disable-static --enable-shared --enable-gpl --disable-debug --shlibdir=/usr/lib64 --enable-runtime-cpudetect --extra-cflags=-I/$PREFIX_DIR/include --extra-ldflags=-L/$PREFIX_DIR/lib
    make -s V=0
    make install
    cd $CURRENT_DIR
  else
    mkdir -p $LIB_DIR
    install_mediadeps
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
pause "prefix dir '$PREFIX_DIR'."

#pause "Installing deps via yum... [press Enter]"
#install_apt_deps

#check_proxy

#pause "Installing openssl library...  [press Enter]"
#install_openssl

#pause "Installing libnice library...  [press Enter]"
#install_libnice

pause "Installing libsrtp library...  [press Enter]"
install_libsrtp

#pause "Installing opus library...  [press Enter]"
#install_opus

#pause "Installing media library...  [press Enter]"
#install_mediadeps

if [ "$CLEANUP" = "true" ]; then
  echo "Cleaning up..."
  cleanup
fi
