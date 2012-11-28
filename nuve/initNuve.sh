#!/bin/bash

SCRIPT=`pwd`/$0
ROOT=`dirname $SCRIPT`
CURRENT_DIR=`pwd`

cd $ROOT/nuveAPI

node nuve.js &

cd $ROOT/cloudHandler

node cloudHandler.js &

cd $CURRENT_DIR
