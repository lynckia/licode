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

os_type()
{
case `uname` in
  Linux )
     LINUX=1
     which yum >/dev/null && { echo "CentOS"; }
     which zypper >/dev/null && { return OpenSUSE; }
     which apt-get  >/dev/null && { echo "Debian"; }
     ;;
  Darwin )
     DARWIN=1
     echo "MacOS"
     ;;
  * )
     # Handle AmgiaOS, CPM, and modified cable modems here.
     ;;
esac
}

checkOrInstallapt(){
	PKG_OK=$(dpkg-query -W --showformat='${Status}\n' $1|grep "install ok installed")
	echo Checking for $1
	if [ "" == "$PKG_OK" ]; then
	  echo -e "\tNo $1 installed"
	  sudo apt-get --force-yes install $1
	else
	  echo -e "\t$1 Installed"
	fi
}

checkOrInstallaptitude(){
	PKG_OK=$(dpkg-query -W --showformat='${Status}\n' $1|grep "install ok installed")
	echo Checking for $1
	if [ "" == "$PKG_OK" ]; then
	  echo -e "\tNo $1 installed"
	  sudo aptitude --assume-yes install $1
	else
	  echo -e "\t$1 Installed"
	fi
}

checkOrInstallnpm(){
	NPM_OK=$(npm -g ls $1)
	echo Checking for $1
	if [ "" == "$NPM_OK" ]; then
	  echo -e "\tNo $1 installed"
	  sudo npm install -g $1 $2
	else
	  echo -e "\t$1 Installed"
	fi
}

install_apt_deps(){
  echo "$(os_type) installation"
  case $(os_type) in
    CentOS )
      sudo yum install npm --enablerepo=epel
      sudo yum install cmake libglib2.0-dev pkg-config libboost-regex-dev libboost-thread-dev libboost-system-dev liblig4cxx10-dev curl --enablerepo=epel
      sudo yum install liblog4cxx10-dev --enablerepo=epel
      sudo yum install boost-regex
      sudo yum install boost-thread boost-system boost-test
      sudo yum install pkgconfig
      sudo yum install glib2-devel
      sudo yum install log4cplus-devel log4cpp-devel
      sudo yum install boost-devel
      sudo yum install yasm libvpx
      sudo yum install libtool
      sudo yum install libvpx
      sudo yum install libvpx-utils libvpx-devel
      sudo yum install log4cplus log4cpp
      sudo yum install log4cplus-devel log4cpp-devel
      sudo yum install log4cxx-devel
      sudo yum install log4cxx
      #TODO: Check JDK package
      checkOrInstallnpm node-gyp
      ;;
    Debian )
      sudo apt-get update
      checkOrInstallapt aptitude
      NODE_OK=$(which node)
      echo Checking for node
      if [ "" == "$NODE_OK" ]; then
        echo -e "\tNo node installed"
        sudo ln -s /usr/bin/nodejs /usr/bin/node
        curl https://npmjs.org/install.sh | sudo sh
      else
        echo -e "\tnode Installed"
      fi
      checkOrInstallaptitude nodejs
      checkOrInstallaptitude npm
      checkOrInstallaptitude make
      checkOrInstallaptitude gcc
      checkOrInstallaptitude g++
      checkOrInstallaptitude libssl-dev
      checkOrInstallaptitude cmake
      checkOrInstallaptitude libglib2.0-dev
      checkOrInstallaptitude pkg-config
      checkOrInstallaptitude nodejs
      checkOrInstallaptitude libboost-regex-dev
      checkOrInstallaptitude libboost-thread-dev
      checkOrInstallaptitude libboost-system-dev
      checkOrInstallaptitude liblog4cxx10-dev
      checkOrInstallaptitude yasm
      checkOrInstallaptitude libvpx-dev
      checkOrInstallaptitude libtool
      checkOrInstallaptitude automake
      checkOrInstallaptitude curl
      checkOrInstallaptitude mongodb
      checkOrInstallaptitude rabbitmq-server
      checkOrInstallaptitude libboost-test-dev
      checkOrInstallaptitude openjdk-6-jre
      checkOrInstallnpm node-gyp
      ;;
  esac
  sudo chown -R `whoami` ~/.npm ~/tmp/
}

install_apt_deps
check_proxy
