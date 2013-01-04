#!/bin/bash

SCRIPT=`pwd`/$0
FILENAME=`basename $SCRIPT`
PATHNAME=`dirname $SCRIPT`
ROOT=$PATHNAME
BUILD_DIR=$ROOT
CURRENT_DIR=`pwd`
DB_DIR="$BUILD_DIR"/db

echo $PATHNAME

dbURL=`grep "config.nuve.dataBaseURL" $PATHNAME/config_template.js`

dbURL=`echo $dbURL| cut -d'"' -f 2`
dbURL=`echo $dbURL| cut -d'"' -f 1`

echo [lynckia] Creating superservice in $dbURL
SERVID=`mongo $dbURL --quiet --eval "db.services.findOne()._id"`
if [ $? -eq 0 ]; then
 SERVKEY=`mongo $dbURL --quiet --eval "db.services.findOne().key"`
else
  mongo $dbURL --eval "db.services.insert({name: 'superService', key: '$RANDOM', rooms: []})"
fi

SERVID=`mongo $dbURL --quiet --eval "db.services.findOne()._id"`
SERVKEY=`mongo $dbURL --quiet --eval "db.services.findOne().key"`

SERVID=`echo $SERVID| cut -d'"' -f 2`
SERVID=`echo $SERVID| cut -d'"' -f 1`

echo [lynckia] SuperService ID $SERVID
echo [lynckia] SuperService KEY $SERVKEY

cd $BUILD_DIR
replacement=s/_auto_generated_ID_/${SERVID}/
sed $replacement $PATHNAME/config_template.js > $BUILD_DIR/config-1.js
replacement=s/_auto_generated_KEY_/${SERVKEY}/
sed $replacement $BUILD_DIR/config-1.js > $ROOT/config_template.js
rm $BUILD_DIR/config-1.js

mkdir -p /etc/lynckia/
cp config_template.js /etc/lynckia/nuve_config.js
