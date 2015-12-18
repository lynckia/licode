#!/bin/bash

echo [erizo_controller] Installing erizoTest

cd erizoClient/tools

./compileDebug.sh

cp ../dist/erizo.js ../../test/public
cp ../dist/socket.io.js ../../test/public

echo [erizo_controller] Done