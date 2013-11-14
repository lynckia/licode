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

  echo [licode] Starting mongodb
  if [ ! -d "$DB_DIR" ]; then
    mkdir -p "$DB_DIR"/db
  fi
  mongod --repair --dbpath $DB_DIR
  mongod --dbpath $DB_DIR --logpath $BUILD_DIR/mongo.log --fork
  sleep 5

  dbURL=`grep "config.nuve.dataBaseURL" $ROOT/licode_config.js`

  dbURL=`echo $dbURL| cut -d'"' -f 2`
  dbURL=`echo $dbURL| cut -d'"' -f 1`

  echo [licode] Creating superservice in $dbURL
  mongo $dbURL --eval "db.services.insert({name: 'superService', key: '$RANDOM', rooms: []})"
  SERVID=`mongo $dbURL --quiet --eval "db.services.findOne()._id"`
  SERVKEY=`mongo $dbURL --quiet --eval "db.services.findOne().key"`

  SERVID=`echo $SERVID| cut -d'"' -f 2`
  SERVID=`echo $SERVID| cut -d'"' -f 1`

  echo "Mongo Logs: "
  cat $BUILD_DIR/mongo.log

  echo [licode] SuperService ID $SERVID
  echo [licode] SuperService KEY $SERVKEY

  perl -pi -e "s/_auto_generated_ID_/$SERVID/s; s/_auto_generated_KEY_/$SERVKEY/s;" $ROOT/licode_config.js
  echo "updated $ROOT/licode_config.js with nuve superservice id/key"

}

install_nuve
populate_mongo
