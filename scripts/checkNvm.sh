#!/usr/bin/env bash

oldstate="$(set +o); set -$-"                # POSIXly store all set options.

set +e

check_readlink() {
  if [ "$(uname)" == "Darwin" ]; then
    # Do something under Mac OS X platform
    READLINK="greadlink"
  elif [ "$(expr substr $(uname -s) 1 5)" == "Linux" ]; then
    # Do something under GNU/Linux platform
    READLINK="readlink"
  else
    echo "Unsupported OS"
    exit 1
  fi

}
command -v nvm | grep 'nvm' &> /dev/null
if [ ! $? == 0 ]; then
  check_readlink
  CHECK_SCRIPT=$($READLINK -f "$0")
  CHECK_SCRIPT_PATH=$(dirname "$CHECK_SCRIPT")
  LINKED_DIR=$CHECK_SCRIPT_PATH/../build/libdeps/nvm
  export NVM_DIR=$($READLINK -f "$LINKED_DIR")
  echo "Checking dir $NVM_DIR"
  if [ -s "$NVM_DIR/nvm.sh" ]; then
    echo "Running nvm"
    . "$NVM_DIR/nvm.sh" # This loads nvm
  else
    echo "ERROR: Missing NVM"
    exit 1
  fi
fi

eval "$oldstate"         # restore all options stored.