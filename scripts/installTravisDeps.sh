#!/bin/bash

install_apt_deps(){
  npm install -g node-gyp
  sudo chown -R `whoami` ~/.npm ~/tmp/
}

echo "Installing deps via apt-get..."
install_apt_deps
echo "Done"
