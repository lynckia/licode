#!/bin/bash

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

cd $ROOT/nuve
./initNuve.sh

sleep 5

cd $ROOT/erizo_controller
./initErizo_controller.sh
./initErizo_agent.sh

cp $ROOT/erizo_controller/erizoClient/dist/erizo.js $EXTRAS/basic_example/public/
cp $ROOT/nuve/nuveClient/dist/nuve.js $EXTRAS/basic_example/

echo [licode] Done, run basic_example/basicServer.js
