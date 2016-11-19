#!/bin/bash

SCRIPT=`pwd`/$0
ROOT=`dirname $SCRIPT`
CURRENT_DIR=`pwd`

cd $ROOT/nuveAPI

nohup node nuve.js &

cd $CURRENT_DIR
