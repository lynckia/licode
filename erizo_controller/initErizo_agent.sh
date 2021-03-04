#!/usr/bin/env bash
set -e

SCRIPT=`pwd`/$0
FILENAME=`basename $SCRIPT`
ROOT=`dirname $SCRIPT`
LICODE_ROOT="$ROOT"/..
CURRENT_DIR=`pwd`
NVM_CHECK="$LICODE_ROOT"/scripts/checkNvm.sh

export LD_LIBRARY_PATH="$LICODE_ROOT/build/libdeps/build/lib"

. $NVM_CHECK

cd $ROOT/erizoAgent
nvm use
node erizoAgent.js $* &

cd $CURRENT_DIR
