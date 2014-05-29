#!/bin/bash

echo [erizo_controller] Installing node_modules for erizo_controller

npm install --loglevel error amqp socket.io@0.9.16 winston

echo [erizo_controller] Done, node_modules installed

cd ./erizoClient/tools

./compile.sh
./compilefc.sh

echo [erizo_controller] Done, erizo.js compiled
