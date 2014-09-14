#!/bin/bash

set -e 

SCRIPT=`pwd`/$0
FILENAME=`basename $SCRIPT`
PATHNAME=`dirname $SCRIPT`
ROOT=$PATHNAME/..
BUILD_DIR=$ROOT/build
CURRENT_DIR=`pwd`
DB_DIR="$BUILD_DIR"/db
EXTRAS=$ROOT/extras

cd $EXTRAS/basic_example

npm install --loglevel error express@3.5.1

cp -r $ROOT/erizo_controller/erizoClient/dist/erizo.js $EXTRAS/basic_example/public/
cp -r $ROOT/nuve/nuveClient/dist/nuve.js $EXTRAS/basic_example/

cd $CURRENT_DIR
