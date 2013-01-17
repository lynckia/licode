#!/bin/bash

SCRIPT=`pwd`/$0
FILENAME=`basename $SCRIPT`
PATHNAME=`dirname $SCRIPT`
ROOT=$PATHNAME/..
BUILD_DIR=$ROOT/build
CURRENT_DIR=`pwd`

pause() {
  read -p "$*"
}
install_libsrtp(){
  cd $ROOT/third_party/srtp
  ./configure
  make
  sudo make uninstall
  sudo make install
  cd $CURRENT_DIR
}

install_erizo(){
  cd $ROOT/erizo
  ./generateProject.sh
  ./buildProject.sh
  export ERIZO_HOME=`pwd`
  cd $CURRENT_DIR
}

install_erizo_api(){
  cd $ROOT/erizoAPI
  ./build.sh
  cd $CURRENT_DIR
}

install_erizo_controller(){
  cd $ROOT/erizo_controller
  ./installErizo_controller.sh
  cd $CURRENT_DIR
}

echo 'Installing libsrtp...'
install_libsrtp
echo 'Installing erizo...'
install_erizo
echo 'Installing erizoAPI...'
install_erizo_api
echo 'Installing erizoController...'
install_erizo_controller
