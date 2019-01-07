#!/usr/bin/env bash

SCRIPT=`pwd`
PATHNAME=`dirname $SCRIPT`
ROOT=$PATHNAME/..
SCRIPTS_FOLDER=$ROOT/scripts
NVM_CHECK="$SCRIPTS_FOLDER"/checkNvm.sh

. $NVM_CHECK

ulimit -c unlimited
nvm use --delete-prefix --silent
exec node $ERIZOJS_NODE_OPTIONS $*
