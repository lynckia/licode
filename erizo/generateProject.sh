#!/bin/bash
runcmake() {
   cmake ../src
   echo "Done"
}
BIN_DIR="build"
if [ -d $BIN_DIR ]; then
  cd $BIN_DIR
  runcmake
else
  mkdir $BIN_DIR
  cd $BIN_DIR
  runcmake
fi

