#!/bin/bash

echo [erizo_controller] Installing node_modules for erizo_controller

cd erizoController

npm install --loglevel debug amqp@0.2.0 socket.io@0.9.16 winston@0.7.3

echo [erizo_controller] Done, node_modules installed

cd ../erizoClient/tools

./compile.sh
./compilefc.sh

echo [erizo_controller] Done, erizo.js compiled