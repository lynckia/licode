#!/bin/bash

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

npm install --loglevel error socket.io-client@0.9.16 log4js node-getopt xmlhttprequest@1.5.0

echo [spine] Done, node_modules installed

cd ../erizo_controller/erizoClient/tools

./compilefc.sh

cp ../dist/erizofc.js ../../../spine/

echo [spine] Done, erizofc.js compiled
