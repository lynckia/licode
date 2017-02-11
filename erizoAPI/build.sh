#!/usr/bin/env bash

set -e

SCRIPT=`pwd`/$0
FILENAME=`basename $SCRIPT`
PATHNAME=`dirname $SCRIPT`
ROOT=$PATHNAME/..
CURRENT_DIR=`pwd`
NVM_CHECK="$ROOT"/scripts/checkNvm.sh

. $NVM_CHECK
nvm use

echo 'linting with cpplint'
./lint.sh

if hash node-waf 2>/dev/null; then
  echo 'building with node-waf'
  rm -rf build
  node-waf configure build
else
  echo 'building with node-gyp'
  node-gyp rebuild
fi
