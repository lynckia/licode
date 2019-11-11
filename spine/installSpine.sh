#!/usr/bin/env bash

set -e

SCRIPT=`pwd`/$0
FILENAME=`basename $SCRIPT`
ROOT=`dirname $SCRIPT`
LICODE_ROOT="$ROOT"/..
CURRENT_DIR=`pwd`
NVM_CHECK="$LICODE_ROOT"/scripts/checkNvm.sh

. $NVM_CHECK

nvm use

echo [spine] Installing node_modules for Spine

npm install --loglevel error

echo [spine] Done, node_modules installed

cd ../erizo_controller/erizoClient/

$LICODE_ROOT/node_modules/.bin/gulp erizofc

echo [spine] Done, erizofc.js compiled
