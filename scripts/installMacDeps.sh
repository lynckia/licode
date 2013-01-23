#!/bin/bash
SCRIPT=`pwd`/$0
FILENAME=`basename $SCRIPT`
PATHNAME=`dirname $SCRIPT`
ROOT=$PATHNAME/..
BUILD_DIR=$ROOT/build
CURRENT_DIR=`pwd`

LIB_DIR=$BUILD_DIR/libdeps

pause() {
  read -p "$*"
}

install_homebrew(){
  ruby -e "$(curl -fsSkL raw.github.com/mxcl/homebrew/go)"
}

install_brew_deps(){
  brew install glib pkg-config boost cmake node mongodb rabbitmq
}

install_libnice(){
  if [ -d $LIB_DIR ]; then
    cd $LIB_DIR
    curl -O http://nice.freedesktop.org/releases/libnice-0.1.3.tar.gz
    tar -zxvf libnice-0.1.3.tar.gz
    cd libnice-0.1.3
    ./configure
    make
    sudo make install
    cd $CURRENT_DIR
  else
    mkdir -p $LIB_DIR
    install_libnice
  fi
}


pause "Installing homebrew... [press Enter]"
install_homebrew
pause "Installing deps via homebrew... [press Enter]"
install_brew_deps
pause 'Installing libnice... [press Enter]'
install_libnice
