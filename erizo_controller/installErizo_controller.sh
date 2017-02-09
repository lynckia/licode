#!/usr/bin/env bash
set -e

SCRIPT=`pwd`/$0
FILENAME=`basename $SCRIPT`
ROOT=`dirname $SCRIPT`
LICODE_ROOT="$ROOT"/..
CURRENT_DIR=`pwd`
NVM_CHECK="$LICODE_ROOT"/scripts/checkNvm.sh

. $NVM_CHECK

echo [erizo_controller] Installing node_modules for erizo_controller

nvm use
npm install --loglevel error amqp socket.io@0.9.16 log4js node-getopt

echo [erizo_controller] Done, node_modules installed

cd ./erizoClient/tools

./compile.sh
./compilefc.sh

echo [erizo_controller] Done, erizo.js compiled
