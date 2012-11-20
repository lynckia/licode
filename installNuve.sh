#!/bin/bash

cd nuveAPI

npm install amqp
npm install express
npm install mongodb
npm install mongojs

cd ../cloudHandler

npm install amqp

echo [nuve] Done, node_modules installed

cd ../nuveClient/tools

./compile.sh
./compileDist.sh

echo [nuve] Done, nuve.js compiled