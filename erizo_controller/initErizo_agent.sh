#!/bin/bash

SCRIPT=`pwd`/$0
FILENAME=`basename $SCRIPT`
ROOT=`dirname $SCRIPT`
CURRENT_DIR=`pwd`

cd $ROOT/erizoAgent
nohup node erizoAgent.js &

cd $CURRENT_DIR
