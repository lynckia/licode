#!/usr/bin/env bash

set -e

SCRIPT=`pwd`/$0
FILENAME=`basename $SCRIPT`
PATHNAME=`dirname $SCRIPT`
ROOT=$PATHNAME/..
CURRENT_DIR=`pwd`
NVM_CHECK="$ROOT"/scripts/checkNvm.sh

. "$NVM_CHECK"
nvm use

echo 'linting with cpplint'
./lint.sh

if hash node-waf 2>/dev/null; then
  echo 'building with node-waf'
  rm -rf build
  node-waf configure build
else
  echo 'building with node-gyp'
  NODE_GYP="$PATHNAME/node_modules/.bin/node-gyp"
  if [ ! -x "$NODE_GYP" ]; then
    echo "node-gyp not found at $NODE_GYP; install erizoAPI dependencies first." >&2
    exit 1
  fi
  "$NODE_GYP" rebuild
fi
