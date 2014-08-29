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


# this is in ging master.  But I added these to package.json to version lock.
#npm install --loglevel error amqp express@3 mongojs aws-lib log4js node-getopt
npm install --loglevel error

echo [nuve] Done, node_modules installed

cd ../nuveClient/tools

./compile.sh

echo [nuve] Done, nuve.js compiled

cd $CURRENT_DIR
