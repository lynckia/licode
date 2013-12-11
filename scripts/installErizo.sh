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

echo 'Installing erizo...'
install_erizo
echo 'Installing erizoAPI...'
install_erizo_api
echo 'Installing erizoController...'
install_erizo_controller
