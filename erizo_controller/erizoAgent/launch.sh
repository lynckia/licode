#!/bin/bash
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$PWD/../../erizo/build/erizo:$PWD/../../build/libdeps/build/lib
ulimit -c unlimited

exec node $*
