#!/usr/bin/env bash

set -e

runcmake() {
   cmake ../src
   echo "Done"
}
BIN_DIR="build"
if [ -d $BIN_DIR ]; then
  cd $BIN_DIR
  make lint
  RET_LINT=`echo $?`
  make check
  RET_TEST=`echo $?`
  exit $(($RET_LINT + $RET_TEST))
else
  echo "Error, build directory does not exist, run generateProject.sh first"
fi
