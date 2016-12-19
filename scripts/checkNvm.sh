#!/bin/bash
set +e
command -v nvm | grep 'nvm' &> /dev/null
if [ ! $? == 0 ]; then
CHECK_SCRIPT=$(readlink -f "$0")
CHECK_SCRIPT_PATH=$(dirname "$CHECK_SCRIPT")
LINKED_DIR=$CHECK_SCRIPT_PATH/../build/libdeps/nvm
export NVM_DIR=$(readlink -f "$LINKED_DIR")
echo "Checking dir $NVM_DIR"
  if [ -s "$NVM_DIR/nvm.sh" ]; then
    echo "Running nvm"
    . "$NVM_DIR/nvm.sh" # This loads nvm
  else
    echo "ERROR: Missing NVM"
    exit 1
  fi
fi 
set -e

