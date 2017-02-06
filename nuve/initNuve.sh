#!/usr/bin/env bash

set -e

SCRIPT=`pwd`/$0
FILENAME=`basename $SCRIPT`
PATHNAME=`dirname $SCRIPT`
ROOT=$PATHNAME/..
NVM_CHECK="$ROOT"/scripts/checkNvm.sh
CURRENT_DIR=`pwd`

. $NVM_CHECK

cd $PATHNAME/nuveAPI

node nuve.js &

cd $CURRENT_DIR
