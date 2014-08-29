#!/bin/bash

SCRIPT=`pwd`/$0
ROOT=`dirname $SCRIPT`/../nuve
CURRENT_DIR=`pwd`

cd $ROOT/nuveAPI

node nuve.js