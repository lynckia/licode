#!/usr/bin/env bash
set -e

SCRIPT=`pwd`/$0
FILENAME=`basename $SCRIPT`
PATHNAME=`dirname $SCRIPT`
BASE_BIN_DIR="build"

usage()
{
  cat << EOF
usage: $0 options
Generate Erizo projects. It will generate all builds if no option is passed.
OPTIONS:
   -h      Show this message
   -d      Generate debug
   -r      Generate release
EOF
}


generateVersion() {
  echo "generating $1"
  BIN_DIR="$BASE_BIN_DIR/$1"
  if [ -d $BIN_DIR ]; then
    cd $BIN_DIR
  else
    mkdir -p $BIN_DIR
    cd $BIN_DIR
  fi
  cmake ../../src "-DERIZO_BUILD_TYPE=$1"
  cd $PATHNAME
}

if [ "$#" -eq 0 ]
then
  generateVersion debug
  generateVersion release
else
while getopts “hdr” OPTION
  do
    case $OPTION in
      h)
        usage
        exit 1
        ;;
      d)
        generateVersion debug
        ;;
      r)
        generateVersion release
        ;;
      ?)
        usage
        exit
        ;;
    esac
  done
fi

generateVersion release
