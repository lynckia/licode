#!/bin/bash

echo [erizo_controller] Installing node_modules for erizo_controller

cd erizoController

npm install amqp
npm install socket.io

echo [erizo_controller] Done, node_modules installed

cd ../erizoClient/tools

./compile.sh
./compilefc.sh

echo [erizo_controller] Done, erizo.js compiled