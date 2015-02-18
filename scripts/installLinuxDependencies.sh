#!/bin/bash
SCRIPT=`pwd`/$0
FILENAME=`basename $SCRIPT`
PATHNAME=`dirname $SCRIPT`
ROOT=$PATHNAME/..
BUILD_DIR=$ROOT/build
CURRENT_DIR=`pwd`

LIB_DIR=$BUILD_DIR/libdeps
PREFIX_DIR=$LIB_DIR/build/

##LIBRARIE VERSIONS
LIBNICE='libnice-0.1.4' #http://nice.freedesktop.org/releases
OPENSSL='openssl-1.0.1g' #http://www.openssl.org/source/
#SRTP $ROOT/third_party/srtp
OPUS='opus-1.1' #http://downloads.xiph.org/releases/opus/
LIBAV='libav-11.1' #https://www.libav.org/releases/



pause() {
  read -p "$*"
}

parse_arguments(){
  while [ "$1" != "" ]; do
    case $1 in
      "--enable-gpl")
        ENABLE_GPL=true
        ;;
      "--cleanup")
        CLEANUP=true
        ;;
    esac
    shift
  done
}

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
      checkOrInstallnpm node-gyp
      checkOrInstallnpm pm2 --unsafe-perm
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
      checkOrInstallnpm node-gyp
      checkOrInstallnpm pm2 --unsafe-perm
      ;;
  esac
  sudo chown -R `whoami` ~/.npm ~/tmp/
}

install_openssl(){
  if [ -d $LIB_DIR ]; then
    cd $LIB_DIR
    if [ ! -d $OPENSSL ] || [ ! -f $PREFIX_DIR/lib/libssl.a ]
    then
      curl -O http://www.openssl.org/source/$OPENSSL.tar.gz
      tar -zxvf $OPENSSL.tar.gz
      cd $OPENSSL
      ./config --prefix=$PREFIX_DIR -fPIC
      make -s V=0
      make install
    fi
    cd $CURRENT_DIR
  else
    mkdir -p $LIB_DIR
    install_openssl
  fi
}

install_libnice(){
  if [ -d $LIB_DIR ]; then
    cd $LIB_DIR
    if [ ! -d $LIBNICE ] || [ ! -f $PREFIX_DIR/lib/libnice.a ]
    then
      curl -O http://nice.freedesktop.org/releases/$LIBNICE.tar.gz
      tar -zxvf $LIBNICE.tar.gz
      cd $LIBNICE
      if [ "$LIBNICE" == "libnice-0.1.4" ];then
        patch -R ./agent/conncheck.c < $PATHNAME/libnice-014.patch0
      fi
      ./configure --prefix=$PREFIX_DIR
      make -s V=0
      make install
    fi    
    cd $CURRENT_DIR
  else
    mkdir -p $LIB_DIR
    install_libnice
  fi
}

install_opus(){
  [ -d $LIB_DIR ] || mkdir -p $LIB_DIR
  cd $LIB_DIR
  if [ ! -d $OPUS ] || [ ! -f $PREFIX_DIR/lib/libopus.a ]
  then
    curl -O http://downloads.xiph.org/releases/opus/$OPUS.tar.gz
    tar -zxvf $OPUS.tar.gz
    cd $OPUS
    ./configure --prefix=$PREFIX_DIR
    make -s V=0
    make install
  fi
  cd $CURRENT_DIR
}

install_mediadeps(){
  install_opus
  sudo apt-get -qq install yasm libvpx. libx264.
  if [ -d $LIB_DIR ]; then
    cd $LIB_DIR
    if [ ! -d $LIBAV ] || [ ! -f $PREFIX_DIR/lib/libavcodec.a ]
    then
      curl -O https://www.libav.org/releases/$LIBAV.tar.gz
      tar -zxvf $LIBAV.tar.gz
      cd $LIBAV
      PKG_CONFIG_PATH=${PREFIX_DIR}/lib/pkgconfig ./configure --prefix=$PREFIX_DIR --enable-shared --enable-gpl --enable-libvpx --enable-libx264 --enable-libopus
      make -s V=0
      make install
    fi
    cd $CURRENT_DIR
  else
    mkdir -p $LIB_DIR
    install_mediadeps
  fi

}

install_mediadeps_nogpl(){
  install_opus
  sudo apt-get -qq install yasm libvpx.
  if [ -d $LIB_DIR ]; then
    cd $LIB_DIR
    if [ ! -d $LIBAV ] || [ ! -f $PREFIX_DIR/lib/libavcodec.a ]
    then
      curl -O https://www.libav.org/releases/$LIBAV.tar.gz
      tar -zxvf $LIBAV.tar.gz
      cd $LIBAV
      PKG_CONFIG_PATH=${PREFIX_DIR}/lib/pkgconfig ./configure --prefix=$PREFIX_DIR --enable-shared --enable-gpl --enable-libvpx --enable-libopus
      make -s V=0
      make install
    fi
    cd $CURRENT_DIR
  else
    mkdir -p $LIB_DIR
    install_mediadeps_nogpl
  fi
}

install_libsrtp(){
  cd $ROOT/third_party/srtp
  if [ ! -f $PREFIX_DIR/lib/libsrtp.a ]
  then
    CFLAGS="-fPIC" ./configure --prefix=$PREFIX_DIR
    make -s V=0
    make uninstall
    make install
  fi
  cd $CURRENT_DIR
}

cleanup(){  
  if [ -d $LIB_DIR ]; then
    cd $LIB_DIR
    rm -r libnice*
    rm -r libav*
    rm -r openssl*
    cd $CURRENT_DIR
  fi
}

parse_arguments $*

mkdir -p $PREFIX_DIR

install_apt_deps
check_proxy
install_openssl
install_libnice
install_libsrtp

install_opus
if [ "$ENABLE_GPL" = "true" ]; then
  install_mediadeps
else
  install_mediadeps_nogpl
fi

if [ "$CLEANUP" = "true" ]; then
  echo "Cleaning up..."
  cleanup
fi
