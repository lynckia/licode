#!/usr/bin/env bash

set -e

BIN_DIR="build"
OBJ_DIR="CMakeFiles"

buildAll() {
  if [ -d $BIN_DIR ]; then
    cd $BIN_DIR
    for d in */ ; do
      echo "Building $d - $*"
      cd $d
      make $*
      cd ..
    done
    cd ..
  else
    echo "Error, build directory does not exist, run generateProject.sh first"
  fi
}



buildAll $*
