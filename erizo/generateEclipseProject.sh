#!/bin/bash
BIN_DIR="build"
if [ -d $BIN_DIR ]; then
  cd $BIN_DIR
  # Set to Debug to be able to debug in Eclipse
  cmake -G"Eclipse CDT4 - Unix Makefiles" -D CMAKE_BUILD_TYPE=Debug ../src
  echo "Done"
  cd ..
else
  echo "Error, build directory does not exist, run generateProject.sh first"
fi
  
