#!/usr/bin/env bash

set -e

SCRIPT=`pwd`/$0
FILENAME=`basename $SCRIPT`
PATHNAME=`dirname $SCRIPT`
ROOT=$PATHNAME/..
BUILD_DIR=$ROOT/build
CURRENT_DIR=`pwd`
LIB_DIR=$BUILD_DIR/libdeps
PREFIX_DIR=$LIB_DIR/build/
NVM_CHECK="$PATHNAME"/checkNvm.sh
FAST_MAKE=''

if [ -d "$LIB_DIR/build/bin" ] && [ -f "$LIB_DIR/build/bin/cmake" ]; then
  printf "Using cmake from $LIB_DIR/build/bin/cmake\n"
  export PATH="$LIB_DIR/build/bin:$PATH"
  export CMAKE_CMD="$LIB_DIR/build/bin/cmake"
  export CMAKE_BIN="$LIB_DIR/build/bin/cmake"
  export CMAKE_ROOT="$LIB_DIR/build/share"
fi
if [ "$(uname)" == "Darwin" ]; then
  NUM_CORES=$(sysctl -n hw.ncpu);
elif [ "$(expr substr $(uname -s) 1 5)" == "Linux" ]; then
  NUM_CORES=$(grep -c ^processor /proc/cpuinfo);
fi

export ERIZO_HOME=$ROOT/erizo

usage()
{
cat << EOF
usage: $0 options

Compile erizo libraries:
- Erizo is the C++ core
- Erizo API is the Javascript layer of Erizo (require Erizo to be compiled)
- Erizo Controller implements the signaling, communication with clients and room management
- Spine is a node.js based Erizo client

OPTIONS:
   -h      Show this message
   -E      Compile Erizo (Only Release)
   -e      Compile Erizo (Release and Debug)
   -a      Compile Erizo API
   -c      Install Erizo node modules
   -d      Delete Erizo object files
   -f      Use 4 threads to build
   -s      Install Spine
   -t      Run Tests
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

install_deps_with_conan(){
  cd $ROOT/erizo
  echo 'Installing deps with conan...'
  cd utils/conan-include-paths
  conan export . lynckia/includes
  cd ../..
  local CONAN_ARCH_ARGS=()
  local HOST_ARCH
  HOST_ARCH=$(uname -m)
  case "$HOST_ARCH" in
    arm64|aarch64) CONAN_ARCH_ARGS+=(-s arch=armv8) ;;
    x86_64|amd64) CONAN_ARCH_ARGS+=(-s arch=x86_64) ;;
  esac
  if [ "$(uname)" == "Darwin" ]; then
    local CLANG_VERSION
    if [ -n "$CONAN_APPLE_CLANG_VERSION" ]; then
      CLANG_VERSION="$CONAN_APPLE_CLANG_VERSION"
    else
      # Get major version of clang and map to supported Conan version
      CLANG_VERSION=$(clang --version | sed -n 's/.*clang version \([0-9]*\).*/\1/p' | head -1)
      if [ -z "$CLANG_VERSION" ]; then
        CLANG_VERSION=13
      else
        # Map clang versions >= 15 to 14, < 15 to 13
        if [ "$CLANG_VERSION" -ge 15 ]; then
          CLANG_VERSION=14
        else
          CLANG_VERSION=13
        fi
      fi
    fi
    # Fix for bzip2 cmake compatibility on ARM64 macOS - use build from cache instead of rebuilding
    export CMAKE_BIN="$LIB_DIR/build/bin/cmake"
    export CMAKE_ROOT="$LIB_DIR/build/share"
    CONAN_BASE_CMD="conan install . --build IncludePathsGenerator --build=missing -c tools.cmake.cmaketoolchain:cmake_executable=$CMAKE_BIN"
    if [ "$(uname)" == "Darwin" ]; then
      $CONAN_BASE_CMD -s compiler.version="$CLANG_VERSION" "${CONAN_ARCH_ARGS[@]}" 
    else
      $CONAN_BASE_CMD -s compiler.libcxx=libstdc++11 "${CONAN_ARCH_ARGS[@]}"
    fi
  fi

  cd $CURRENT_DIR
}

install_erizo_release(){
  install_deps_with_conan
  cd $ROOT/erizo
  echo 'Installing erizo...'
  CMAKE_ROOT=$CMAKE_ROOT CMAKE_CMD=$CMAKE_CMD ./generateProject.sh -r
  ./buildProject.sh $FAST_MAKE
  if [ "$DELETE_OBJECT_FILES" == "true" ]; then
    ./cleanObjectFiles.sh
  fi
  check_result $?
  cd $CURRENT_DIR
}

install_erizo(){
  install_deps_with_conan
  echo 'Installing erizo... -CMAKE_CMD: '$CMAKE_CMD
  cd $ROOT/erizo
  CMAKE_ROOT=$CMAKE_ROOT CMAKE_CMD=$CMAKE_CMD ./generateProject.sh
  ./buildProject.sh $FAST_MAKE
  if [ "$DELETE_OBJECT_FILES" == "true" ]; then
    ./cleanObjectFiles.sh
  fi
  check_result $?
  cd $CURRENT_DIR
}

install_erizo_api_release(){
  echo 'Installing erizoAPI...'
  cd $ROOT/erizoAPI
  . $NVM_CHECK
  nvm use
  $FAST_BUILD npm install --unsafe-perm -no_debug11
  check_result $?
  cd $CURRENT_DIR
}

install_erizo_api(){
  echo 'Installing erizoAPI...'
  cd $ROOT/erizoAPI
  . $NVM_CHECK
  nvm use
  $FAST_BUILD npm install --unsafe-perm
  check_result $?
  cd $CURRENT_DIR
}

install_erizo_controller(){
  echo 'Installing erizoController...'
  cp $PATHNAME/rtp_media_config_default.js $ROOT/rtp_media_config.js
  cp $PATHNAME/bw_distributor_config_default.js $ROOT/bw_distributor_config.js
  cd $ROOT/erizo_controller
  ./installErizo_controller.sh
  check_result $?
  cd $CURRENT_DIR
}

install_spine(){
  echo 'Installing erizo_native_client...'
  cd $ROOT/spine
  ./installSpine.sh
  check_result $?
  cd $CURRENT_DIR
}

execute_tests(){
  echo 'Testing erizo...'
  cd $ROOT/erizo
  ./runTests.sh
  check_result $?
  cd $CURRENT_DIR
}

if [ "$#" -eq 0 ]
then
  install_erizo
  install_erizo_api
  install_erizo_controller
  install_spine
else
  while getopts “heEaAcstfd” OPTION
  do
    case $OPTION in
      h)
        usage
        exit 1
        ;;
      E)
        install_erizo_release
        ;;
      e)
        install_erizo
        ;;
      A)
        install_erizo_api_release
        ;;
      a)
        install_erizo_api
        ;;
      c)
        install_erizo_controller
        ;;
      s)
        install_spine
        ;;
      t)
        execute_tests
        ;;
      f)
        FAST_MAKE="-j$NUM_CORES"
        FAST_BUILD="env JOBS=$NUM_CORES"
        echo "Compiling using $NUM_CORES threads"
        ;;
      d)
        DELETE_OBJECT_FILES='true'
        ;;
      ?)
        usage
        exit
        ;;
    esac
  done
fi
