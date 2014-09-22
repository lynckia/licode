#!/bin/bash

# Script designed to run from a process control like supervisor or upstart
# Does not put process in the background.

SCRIPT=`pwd`/$0
FILENAME=`basename $SCRIPT`
ROOT=`dirname $SCRIPT`/..
CURRENT_DIR=`pwd`

export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$ROOT/erizo/build/erizo:$ROOT/erizo:$ROOT/build/libdeps/build/lib
export ERIZO_HOME=$ROOT/erizo/

# Make sure nuve service has started
sleep 2

cd $ROOT/erizo_controller/erizoController

ulimit -n 4096
ulimit -c unlimited

# if you run into c++ seg faults, use this instead of the while block in dev (type run at gdb prompt)
# gdb --args node erizoController.js

while node erizoController.js; do
  echo "node erizoController.js exited unexpectedly.  Respawning." >&2
  until node erizoController.js; do
      echo "node erizoController.js crashed with exit code $?.  Respawning." >&2
      sleep 1
  done
done
