#!/usr/bin/env bash

SCRIPT=`pwd`
PATHNAME=`dirname $SCRIPT`
ROOT=$PATHNAME/..
SCRIPTS_FOLDER=$ROOT/scripts
NVM_CHECK="$SCRIPTS_FOLDER"/checkNvm.sh

ulimit -c unlimited
exec $NVM_CHECK node $ERIZOJS_NODE_OPTIONS $*
