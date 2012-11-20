#!/bin/bash

cd erizoController

npm install amqp
npm install socket.io

echo [erizo_controller] Done, node_modules installed

cd ../erizoClient/tools

./compile.sh

echo [erizo_controller] Done, erizo.js compiled