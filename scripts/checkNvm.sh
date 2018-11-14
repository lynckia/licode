#!/usr/bin/env bash

set +e

command -v nvm | grep 'nvm' &> /dev/null
if [ ! $? == 0 ]; then
  CHECK_SCRIPT_PATH="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
  export NVM_DIR=$CHECK_SCRIPT_PATH/../build/libdeps/nvm
  if [ -s "$NVM_DIR/nvm.sh" ]; then
    echo "Running nvm"
    . "$NVM_DIR/nvm.sh" &> /dev/null # This loads nvm
  else
    echo "ERROR: Missing NVM"
    exit 1
  fi
fi

