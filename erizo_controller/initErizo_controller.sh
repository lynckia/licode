#!/bin/bash

SCRIPT=`pwd`/$0
FILENAME=`basename $SCRIPT`
ROOT=`dirname $SCRIPT`
CURRENT_DIR=`pwd`

echo $LD_LIBRARY_PATH

cd $ROOT/erizoController
#valgrind node erizoController.js &
node erizoController.js &

cd $CURRENT_DIR
