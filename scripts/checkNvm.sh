#!/bin/bash
set -e 
CHECK_SCRIPT=$(readlink -f "$0")
CHECK_SCRIPT_PATH=$(dirname "$CHECK_SCRIPT")
LINKED_DIR=$CHECK_SCRIPT_PATH/../build/libdeps/nvm
export NVM_DIR=$(readlink -f "$LINKED_DIR")

echo 'Checking nvm dir $NVM_DIR'
set +e
command -v nvm | grep 'string' &> /dev/null
if [ ! $? == 0 ]; then
  if [ -s "$NVM_DIR/nvm.sh" ]; then
    . "$NVM_DIR/nvm.sh" # This loads nvm
  else
    set -e
    echo "ERROR: Missing NVM"
    exit 1
  fi
fi 
set -e

