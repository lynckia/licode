#!/bin/bash

SCRIPT=`pwd`/$0
FILENAME=`basename $SCRIPT`
ROOT=`dirname $SCRIPT`
CURRENT_DIR=`pwd`

echo $LD_LIBRARY_PATH

cd $ROOT/erizoAgent
node erizoAgent.js &

cd $CURRENT_DIR
