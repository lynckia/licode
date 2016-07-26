#!/usr/bin/env bash

SCRIPT=`pwd`/$0
FILENAME=`basename $SCRIPT`
PATHNAME=`dirname $SCRIPT`
ROOT=$PATHNAME/..
BUILD_DIR=$ROOT/build
CURRENT_DIR=`pwd`
LIB_DIR=$BUILD_DIR/libdeps
PREFIX_DIR=$LIB_DIR/build/


export ERIZO_HOME=$ROOT/erizo

usage()
{
cat << EOF
usage: $0 options

Compile erizo libraries:
- Erizo is the C++ core
- Erizo API is the Javascript layer of Erizo (require Erizo to be compiled)
- Erizo Controller implements the signaling, communication with clients and room management
- Spine is a node.js based Erizo client

OPTIONS:
   -h      Show this message
   -e      Compile Erizo
   -a      Compile Erizo API
   -c      Install Erizo node modules
   -s      Install Spine
EOF
}

pause() {
  read -p "$*"
}

install_erizo(){
  echo 'Installing erizo...'
  cd $ROOT/erizo
  ./generateProject.sh
  ./buildProject.sh
  cd $CURRENT_DIR
}

install_erizo_api(){
  echo 'Installing erizoAPI...'
  cd $ROOT/erizoAPI
  npm install nan@2.3.2
  ./build.sh
  cd $CURRENT_DIR
}

install_erizo_controller(){
  echo 'Installing erizoController...'
  cd $ROOT/erizo_controller
  ./installErizo_controller.sh
  cd $CURRENT_DIR
}

install_spine(){
  echo 'Installing erizo_native_client...'
  cd $ROOT/spine
  ./installSpine.sh
  cd $CURRENT_DIR
}


if [ "$#" -eq 0 ]
then
  install_erizo
  install_erizo_api
  install_erizo_controller
  install_spine
else
  while getopts “heac” OPTION
  do
    case $OPTION in
      h)
        usage
        exit 1
        ;;
      e)
        install_erizo
        ;;
      a)
        install_erizo_api
        ;;
      c)
        install_erizo_controller
        ;;
      s)
        install_spine
        ;;
      ?)
        usage
        exit
        ;;
    esac
  done
fi
