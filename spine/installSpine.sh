#!/bin/bash

echo [erizo_native_client] Installing node_modules for erizo_native_client

npm install --loglevel error socket.io-client@0.9.16 log4js node-getopt xmlhttprequest@1.5.0

echo [erizo_native_client] Done, node_modules installed

cd ../erizo_controller/erizoClient/tools

./compilefc.sh

cp ../dist/erizofc.js ../../../erizo_native_client/

echo [erizo_native_client] Done, erizofc.js compiled
