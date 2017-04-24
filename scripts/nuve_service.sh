#!/bin/bash

# Script designed to run from a process control like supervisor or upstart
# Does not put process in the background.

SCRIPT=`pwd`/$0
ROOT=`dirname $SCRIPT`/../nuve
CURRENT_DIR=`pwd`

cd $ROOT/nuveAPI

node nuve.js