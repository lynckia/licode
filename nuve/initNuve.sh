#!/bin/bash

SCRIPT=`pwd`/$0
ROOT=`dirname $SCRIPT`
CURRENT_DIR=`pwd`

cd $ROOT/nuveAPI
nvm use

node nuve.js &

cd $CURRENT_DIR
