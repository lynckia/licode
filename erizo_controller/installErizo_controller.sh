#!/bin/bash

set -e

echo [erizo_controller] Installing node_modules for erizo_controller

# this is in ging master.  But I added these to package.json to version lock.
#npm install --loglevel error amqp socket.io@0.9.16 log4js node-getopt
npm install

echo [erizo_controller] Done, node_modules installed

cd ./erizoClient/tools

./compileDebug.sh
./compilefc.sh

echo [erizo_controller] Done, erizo.js compiled
