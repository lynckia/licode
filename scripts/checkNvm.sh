#!/bin/bash
set -e 
export NVM_DIR="$HOME/.nvm"
if [ -s "$NVM_DIR/nvm.sh" ]; then
  set +e
  source "$NVM_DIR/nvm.sh" # This loads nvm
  set -e
else
  echo "ERROR: Missing NVM"
  exit 1
fi
