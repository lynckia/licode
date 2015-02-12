#!/bin/bash
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$PWD/../../erizo/build/erizo
ulimit -c unlimited

node $*
