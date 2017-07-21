#!/usr/bin/env bash
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$PWD/../erizo
ulimit -c unlimited

cd ../erizoJS
exec node $ERIZOJS_NODE_OPTIONS $*
