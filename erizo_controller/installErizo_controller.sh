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
npm install --loglevel error

echo [erizo_controller] Done, node_modules installed

cd ./erizoClient/

$LICODE_ROOT/node_modules/.bin/gulp erizo

check_result $?

echo [erizo_controller] Done, erizo.js compiled
