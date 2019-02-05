#!/usr/bin/env bash
set -e

SCRIPT=`pwd`/$0
FILENAME=`basename $SCRIPT`
ROOT=`dirname $SCRIPT`
LICODE_ROOT="$ROOT"/..
CURRENT_DIR=`pwd`
NVM_CHECK="$LICODE_ROOT"/scripts/checkNvm.sh

. $NVM_CHECK

check_result() {
  if [ "$1" -ne 0 ]
  then
    echo "ERROR: Failed building ErizoClient"
    exit $1
  fi
}

echo [erizo_controller] Installing node_modules for erizo_controller

nvm use
npm install --loglevel error amqp socket.io@2.0.3 log4js@1.0.1 node-getopt uuid@3.1.0 sdp-transform@2.3.0 socket.io-client@2.0.3 webrtc-adapter@6.2.1 webpack-conditional-loader@1.0.12

echo [erizo_controller] copying default client build configuration...

cp erizoClient/default.client.build.config.js ./client.build.config.js

echo [erizo_controller] Done, node_modules installed

./compileClient.sh

check_result $?

echo [erizo_controller] Done, erizo.js compiled
