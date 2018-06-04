#!/usr/bin/env bash
set -e

SCRIPT=`pwd`/$0
FILENAME=`basename $SCRIPT`
PATHNAME=`dirname $SCRIPT`
BASE_BIN_DIR="build"


generateVersion() {
  echo "generating $1"
  BIN_DIR="$BASE_BIN_DIR/$1"
  if [ -d $BIN_DIR ]; then
    cd $BIN_DIR
  else
    mkdir -p $BIN_DIR
    cd $BIN_DIR
  fi
  cmake ../../src "-DERIZO_BUILD_TYPE=$1"
  cd $PATHNAME
}


generateVersion release
generateVersion debug
