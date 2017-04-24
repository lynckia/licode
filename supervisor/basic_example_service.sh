#!/bin/bash

# Script designed to run from a process control like supervisor or upstart
# Does not put process in the background.

SCRIPT=`pwd`/$0
FILENAME=`basename $SCRIPT`
PATHNAME=`dirname $SCRIPT`
ROOT=$PATHNAME/..
BUILD_DIR=$ROOT/build
CURRENT_DIR=`pwd`
EXTRAS=$ROOT/extras

cd $EXTRAS/basic_example
node basicServer.js