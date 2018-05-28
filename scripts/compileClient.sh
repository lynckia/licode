#!/usr/bin/env bash

set -e

SCRIPT=`pwd`/$0
PATHNAME=`dirname $SCRIPT`
ROOT=$PATHNAME/..
NVM_CHECK="$PATHNAME"/checkNvm.sh

ENVS=""

export ERIZO_HOME=$ROOT/erizo

usage()
{
cat << EOF
usage: $0 options

Compile erizo client:

If no options are provided, a complete bundle (including adapter and sockets) will made.
To build erizo without additional modules simply invoke this script with -c option.

OPTIONS:
   -h      Show this message
   -a      include adapter in the build
   -s      include socket.io in the build
   -w      watch for changes in client code and recompile it in debug mode
   -c      compile
EOF
}

pause() {
  read -p "$*"
}

check_result() {
  if [ "$1" -ne 0 ]
  then
    exit $1
  fi
}

watch_client(){
  echo 'Watching client...'
  . $NVM_CHECK
  nvm use
  cd $ROOT
  (export $ENVS && npm run watchClient)
}

compile() {
  echo 'Compiling client...'
  . $NVM_CHECK
  nvm use
  cd $ROOT
  (export $ENVS && npm run buildClient)
}

if [ "$#" -eq 0 ]
then
  echo 'Compiling client with socketio and adapter included in the bundle ...'
  ENVS="INCLUDE_ADAPTER=TRUE INCLUDE_SOCKETIO=TRUE"
  compile
else
  while getopts “hascw” OPTION
  do
    case $OPTION in
      h)
        usage
        exit 1
        ;;
      a)
        ENVS+="INCLUDE_ADAPTER=TRUE "
        ;;
      s)
        ENVS+="INCLUDE_SOCKETIO=TRUE "
        ;;
      c)
        compile
        ;;
      w)
        echo 'Remember to specify -s to include socketio and -a to include adapter before this option'
        watch_client
        ;;
      ?)
        usage
        exit
        ;;
    esac
  done
fi
