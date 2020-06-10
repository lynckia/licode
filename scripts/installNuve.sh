#!/usr/bin/env bash

set -e

SCRIPT=`pwd`/$0
FILENAME=`basename $SCRIPT`
PATHNAME=`dirname $SCRIPT`
ROOT=$PATHNAME/..
BUILD_DIR=$ROOT/build
CURRENT_DIR=`pwd`
DB_DIR="$BUILD_DIR"/db

check_result() {
  if [ "$1" -eq 1 ]
  then
    exit 1
  fi
}

install_nuve(){
  cd $ROOT/nuve
  ./installNuve.sh
  check_result $?
  cd $CURRENT_DIR
}

create_credentials(){
  mongo $dbURL --eval "db.services.insert({name: 'superService', key: '$RANDOM', rooms: []})"
  SERVID=`mongo $dbURL --quiet --eval "db.services.findOne()._id"`
  SERVKEY=`mongo $dbURL --quiet --eval "db.services.findOne().key"`
  SERVID=`echo $SERVID| cut -d'"' -f 2`
  SERVID=`echo $SERVID| cut -d'"' -f 1`
}
add_credentials(){
  RESULT=`mongo $dbURL --quiet --eval "db.services.insert({name: 'superService', _id: ObjectId('$SERVID'), key: '$SERVKEY', rooms: []})"`
  echo $RESULT
  NOTFOUND=`echo $RESULT | grep "writeError"`
  if [[ ! -z "$NOTFOUND" ]]; then
    SERVID=""
    SERVKEY=""
  fi
}

check_credentials(){
  RESULT=`mongo $dbURL --quiet --eval "db.services.find({name: 'superService', _id: ObjectId('$SERVID'), key: '$SERVKEY', rooms: []})"`
  echo $RESULT
  if [[ -z "$RESULT" ]]; then
    add_credentials
  fi
}

get_or_create_superservice_credentials(){
  if [[ -z "$SERVID" && -z "$SERVKEY" ]]; then
    create_credentials
  else
    check_credentials
  fi
}

populate_mongo(){

  if ! pgrep mongod; then
    echo [licode] Starting mongodb
    if [ ! -d "$DB_DIR" ]; then
      mkdir -p "$DB_DIR"/db
    fi
    mongod --repair --dbpath $DB_DIR
    mongod --dbpath $DB_DIR --logpath $BUILD_DIR/mongo.log --fork
    sleep 5
  else
    echo [licode] mongodb already running
  fi

  dbURL=`grep "config.nuve.dataBaseURL" $PATHNAME/licode_default.js`

  dbURL=`echo $dbURL| cut -d'"' -f 2`
  dbURL=`echo $dbURL| cut -d'"' -f 1`

  echo [licode] Creating superservice in $dbURL
  get_or_create_superservice_credentials

  if [[ -z "$SERVID" && -z "$SERVKEY" ]]; then
    echo "We could not get or create superservice credentials"
    exit 1
  else
    echo "We got superservice credentials"
  fi


  if [ -f "$BUILD_DIR/mongo.log" ]; then
    echo "Mongo Logs: "
    cat $BUILD_DIR/mongo.log
  fi

  echo [licode] SuperService ID $SERVID
  echo [licode] SuperService KEY $SERVKEY
  cd $BUILD_DIR
  replacement=s/_auto_generated_ID_/${SERVID}/
  sed $replacement $PATHNAME/licode_default.js > $BUILD_DIR/licode_1.js
  replacement=s/_auto_generated_KEY_/${SERVKEY}/
  sed $replacement $BUILD_DIR/licode_1.js > $ROOT/licode_config.js
  rm $BUILD_DIR/licode_1.js
}

install_nuve
populate_mongo
