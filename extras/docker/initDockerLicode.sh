#!/bin/bash
echo "config.erizoController.publicIP = '$PUBLIC_IP';" >> /opt/licode/licode_config.js
mongod --dbpath /opt/licode/build/db --logpath /opt/licode/build/mongo.log --fork
cd /opt/licode/scripts
./initLicode.sh
cd /opt/licode/extras/basic_example
node basicServer.js
