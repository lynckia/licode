#!/usr/bin/env bash

set -e

BIN_DIR="build"

buildAll() {
  if [ -d $BIN_DIR ]; then
    for entry in "$BIN_DIR"/*
    do
      echo "Building directory: $entry with flags $*"
      make -C $entry $*
    done
  else
    echo "Error, build directory does not exist, run generateProject.sh first"
  fi
}

buildAll $*
