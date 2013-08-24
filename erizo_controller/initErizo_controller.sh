#!/bin/bash

SCRIPT=`pwd`/$0
FILENAME=`basename $SCRIPT`
ROOT=`dirname $SCRIPT`
CURRENT_DIR=`pwd`

echo $LD_LIBRARY_PATH

cd $ROOT/erizoController
forever -a -l forever.log -o erizo_output.log -e erizo_error.log erizoController.js

cd $CURRENT_DIR
