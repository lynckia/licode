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

pause 'Installing erizo... [press Enter]'
install_erizo
pause 'Installing erizoAPI... [press Enter]'
install_erizo_api
pause 'Installing erizoController... [press Enter]'
install_erizo_controller
