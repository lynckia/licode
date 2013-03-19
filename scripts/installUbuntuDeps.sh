#!/bin/bash
LIB_DIR=libdeps
CURRENT_DIR=`pwd`
pause() {
  read -p "$*"
}

install_apt_deps(){
  sudo apt-get install python-software-properties
  sudo apt-get install software-properties-common
  sudo add-apt-repository ppa:chris-lea/node.js-legacy
  sudo apt-get update
  sudo apt-get install git make gcc g++ libssl-dev cmake libnice10 libnice-dev libglib2.0-dev pkg-config nodejs nodejs-dev npm libboost-regex-dev libboost-thread-dev libboost-system-dev rabbitmq-server mongodb openjdk-6-jre
}

pause "Installing deps via apt-get... [press Enter]"
install_apt_deps

