#!/bin/bash

pause() {
  if [ "$UNATTENDED" == "true" ]; then
    echo "$*"
  else
    read -p "$* [press Enter]"
  fi
}

parse_arguments(){
  while [ "$1" != "" ]; do
    case $1 in
      "--unattended")
        UNATTENDED=true
        ;;
      "--disable-services")
        DISABLE_SERVICES=true
        ;;
      "--use-cache")
        CACHE=true
        ;;

    esac
    shift
  done
}

check_result() {
  if [ "$1" -eq 1 ]
  then
    exit 1
  fi
}

install_homebrew_from_cache(){
  if [ -f cache/homebrew-cache.tar.gz ]; then
    tar xzf cache/homebrew-cache.tar.gz --directory /usr/local/Cellar
    brew link glib pkg-config boost cmake yasm log4cxx gettext
  fi
}

copy_homebrew_to_cache(){
  mkdir cache
  tar czf cache/homebrew-cache.tar.gz --directory /usr/local/Cellar glib pkg-config boost cmake yasm log4cxx gettext
}

install_homebrew(){
  if [ "$CACHE" == "true" ]; then
    install_homebrew_from_cache
  fi
  which -s brew
  if [[ $? != 0 ]] ; then
    # Install Homebrew
    ruby -e "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/master/install)"
  fi
}

install_brew_deps(){
  brew install glib pkg-config boost cmake yasm log4cxx gettext
  npm install -g node-gyp
  if [ "$DISABLE_SERVICES" != "true" ]; then
    brew install rabbitmq mongodb
  fi
}

parse_arguments $*

pause "Installing homebrew..."
install_homebrew

pause "Installing deps via homebrew..."
install_brew_deps

if [ "$CACHE" == "true" ]; then
  copy_homebrew_to_cache
fi
