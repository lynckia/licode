#!/usr/bin/env bash

SCRIPT=`pwd`/$0
FILENAME=`basename $SCRIPT`
PATHNAME=`dirname $SCRIPT`
ROOT=$PATHNAME/..
BUILD_DIR=$ROOT/build
CURRENT_DIR=`pwd`
EXTRAS=$ROOT/extras

export PATH=$PATH:/usr/local/sbin

if ! pgrep -f rabbitmq; then
  sudo echo
  sudo rabbitmq-server > $BUILD_DIR/rabbit.log &
fi

sleep 20

cd $ROOT/nuve
./initNuve.sh

sleep 5

export ERIZO_HOME=$ROOT/erizo/
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$ROOT/build/libdeps/build/lib 

cd $ROOT/erizo_controller
./initErizo_controller.sh
./initErizo_agent.sh

echo [licode] Done, run ./scripts/initBasicExample.sh
