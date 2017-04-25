#!/usr/bin/env bash
SCRIPT=`pwd`/$0
ROOT=/opt/licode
EXTRAS=$ROOT/extras
NVM_CHECK=$ROOT/scripts/checkNvm.sh


echo "config.erizoController.publicIP = '$PUBLIC_IP';" >> /opt/licode/licode_config.js
mongod --dbpath /opt/licode/build/db --logpath /opt/licode/build/mongo.log --fork
rabbitmq-server -detached
cd /opt/licode/scripts
./initLicode.sh

cp $ROOT/erizo_controller/erizoClient/dist/erizo.js $EXTRAS/basic_example/public/
cp $ROOT/nuve/nuveClient/dist/nuve.js $EXTRAS/basic_example/

. $NVM_CHECK
nvm use
cd /opt/licode/extras/basic_example
node basicServer.js
