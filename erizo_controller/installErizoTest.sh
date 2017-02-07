#!/usr/bin/env bash

echo [erizo_controller] Installing erizoTest

cd erizoClient/tools

./compileDebug.sh

cp ../dist/erizo.js ../../test/public

echo [erizo_controller] Done