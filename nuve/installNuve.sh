#!/bin/bash

set -e

SCRIPT=`pwd`/$0
FILENAME=`basename $SCRIPT`
PATHNAME=`dirname $SCRIPT`
ROOT=$PATHNAME/..
BUILD_DIR=$ROOT/build
CURRENT_DIR=`pwd`
DB_DIR="$BUILD_DIR"/db

cd $PATHNAME

cd nuveAPI

echo [nuve] Installing node_modules for nuve

# Uses ../../package.json
npm install --loglevel error

echo [nuve] Done, node_modules installed

cd ../nuveClient/tools

./compile.sh

echo [nuve] Done, nuve.js compiled

cd $CURRENT_DIR
