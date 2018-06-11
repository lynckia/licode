#!/usr/bin/env bash
set -e
SCRIPT=`pwd`/$0
ROOT=`dirname $SCRIPT`
LICODE_ROOT="$ROOT"/..
NVM_CHECK="$LICODE_ROOT"/scripts/checkNvm.sh

ENVS=""

usage()
{
cat << EOF
usage: $0 options

Compile erizo client:

If no options are provided, a complete bundle (including adapter and sockets) will made.
To build erizo without additional modules simply invoke this script with -c option.

OPTIONS:
   -h      Show this message
   -a      Exclude adapter in the build
   -s      Exclude socket.io in the build
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

watch_client() {
  echo 'Watching client...'
  . $NVM_CHECK
  nvm use
  cd $ROOT
  (export $ENVS && npm run watchClient)
}

compile() {
  echo 'Compiling client...'
  echo $ENVS
  . $NVM_CHECK
  nvm use
  cd $ROOT
  (export $ENVS && npm run buildClient)
}

if [ "$#" -eq 0 ]
then
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
        ENVS+="EXCLUDE_ADAPTER=TRUE "
        ;;
      s)
        ENVS+="EXCLUDE_SOCKETIO=TRUE "
        ;;
      c)
        compile
        ;;
      w)
        watch_client
        ;;
      ?)
        usage
        exit
        ;;
    esac
  done
fi
