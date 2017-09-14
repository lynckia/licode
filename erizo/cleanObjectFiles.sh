#!/usr/bin/env bash

set -e

BIN_DIR="build"
OBJ_DIR="CMakeFiles"

maybeRemoveObjDir() {
  if [ -d $OBJ_DIR ]; then
    rm -rf $OBJ_DIR
  fi
}

cleanAll() {
  if [ -d $BIN_DIR ]; then
    cd $BIN_DIR
    for RELEASE_DIR in */ ; do
      echo "cleaning $RELEASE_DIR"
      cd $RELEASE_DIR
      maybeRemoveObjDir
      for INTERNAL_DIR in */ ; do
        cd $INTERNAL_DIR
        maybeRemoveObjDir
        cd ..
      done
      cd ..
    done
    cd ..
  fi
}

cleanAll
