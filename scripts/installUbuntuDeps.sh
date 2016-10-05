#!/bin/bash

check_proxy(){
  if [ -z "$http_proxy" ]; then
    echo "No http proxy set, doing nothing"
  else
    echo "http proxy configured, configuring npm"
    npm config set proxy $http_proxy
  fi

  if [ -z "$https_proxy" ]; then
    echo "No https proxy set, doing nothing"
  else
    echo "https proxy configured, configuring npm"
    npm config set https-proxy $https_proxy
  fi
}

install_apt_deps(){
  sudo apt-get install python-software-properties
  sudo apt-get install software-properties-common
  sudo add-apt-repository ppa:chris-lea/node.js
  sudo apt-get update
  sudo apt-get install git make gcc g++ libssl-dev cmake libglib2.0-dev pkg-config nodejs libboost-regex-dev libboost-thread-dev libboost-system-dev liblog4cxx10-dev rabbitmq-server mongodb openjdk-6-jre curl libboost-test-dev
  sudo npm install -g node-gyp
  sudo chown -R `whoami` ~/.npm ~/tmp/
}

echo "Installing deps via apt-get..."
install_apt_deps

check_proxy
