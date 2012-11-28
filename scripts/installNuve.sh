#!/bin/bash

SCRIPT=`pwd`/$0
FILENAME=`basename $SCRIPT`
PATHNAME=`dirname $SCRIPT`
ROOT=$PATHNAME/..
BUILD_DIR=$ROOT/build
CURRENT_DIR=`pwd`
DB_DIR="$BUILD_DIR"/db

install_nuve(){
  cd $ROOT/nuve
  ./installNuve.sh
  cd $CURRENT_DIR
}

populate_mongo(){

  echo [lynckia] Starting mongodb
  if [ ! -d "$DB_DIR" ]; then
    mkdir -p "$DB_DIR"/db
  fi
  mongod --repair --dbpath $DB_DIR
  mongod --dbpath $DB_DIR > $BUILD_DIR/mongo.log &
  sleep 5

  dbURL=`grep "config.nuve.dataBaseURL" $PATHNAME/lynckia_default.js`

  dbURL=`echo $dbURL| cut -d'"' -f 2`
  dbURL=`echo $dbURL| cut -d'"' -f 1`

  echo [lynckia] Creating superservice in $dbURL
  mongo $dbURL --eval "db.services.insert({name: 'superService', key: '$RANDOM', rooms: []})"
  SERVID=`mongo $dbURL --quiet --eval "db.services.findOne()._id"`
  SERVKEY=`mongo $dbURL --quiet --eval "db.services.findOne().key"`

  SERVID=`echo $SERVID| cut -d'"' -f 2`
  SERVID=`echo $SERVID| cut -d'"' -f 1`

  echo [lynckia] SuperService ID $SERVID
  echo [lynckia] SuperService KEY $SERVKEY
  cd $BUILD_DIR
  replacement=s/_auto_generated_ID_/${SERVID}/
  sed $replacement $PATHNAME/lynckia_default.js > $BUILD_DIR/lynckia_1.js
  replacement=s/_auto_generated_KEY_/${SERVKEY}/
  sed $replacement $BUILD_DIR/lynckia_1.js > $ROOT/lynckia_config.js
  rm $BUILD_DIR/lynckia_1.js
}

install_nuve
populate_mongo
