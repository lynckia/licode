#!/bin/bash

SCRIPT=`pwd`/$0
FILENAME=`basename $SCRIPT`
ROOT=`dirname $SCRIPT`/../erizo_controller
CURRENT_DIR=`pwd`

export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$ROOT/../erizo/build/erizo:$ROOT/../erizo:$ROOT/../build/libdeps/build/lib
export ERIZO_HOME=$ROOT/erizo/

cd $ROOT/erizoController
node erizoController.js
