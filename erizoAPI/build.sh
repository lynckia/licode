#!/bin/bash

export LINK=g++

export ERIZO_HOME="$(pwd)/../erizo"
export LD_LIBRARY_PATH="${ERIZO_HOME}/build/erizo"

if hash node-waf 2>/dev/null; then
  echo 'building with node-waf'
  rm -rf build
  node-waf configure build
else
  echo 'building with node-gyp'
  node-gyp rebuild
fi
