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
  sudo apt-get update -y
  sudo apt-get install -qq python-software-properties -y
  sudo apt-get install -qq software-properties-common -y
  sudo add-apt-repository ppa:chris-lea/node.js -y
  sudo apt-get update -y
  sudo apt-get install -qq git make gcc g++ libssl-dev cmake libglib2.0-dev pkg-config nodejs libboost-regex-dev libboost-thread-dev libboost-system-dev liblog4cxx10-dev rabbitmq-server mongodb openjdk-6-jre curl libboost-test-dev -y
  sudo npm install -g node-gyp
  sudo chown -R `whoami` ~/.npm ~/tmp/
}

install_apt_deps
check_proxy
