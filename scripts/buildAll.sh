set -e

SCRIPT=`pwd`/$0
FILENAME=`basename $SCRIPT`
PATHNAME=`dirname $SCRIPT`
ROOT=$PATHNAME/..
BUILD_DIR=$ROOT/build
CURRENT_DIR=`pwd`

# Restart everything
sudo supervisorctl stop nuve
sudo supervisorctl stop erizo
sudo supervisorctl stop licode_basic_server


pause() {
  read -p "$*"
}

install_erizo(){
  cd $ROOT/erizo
  ./generateProject.sh
  ./buildProject.sh
  export ERIZO_HOME=`pwd`
  cd $CURRENT_DIR
}

install_erizo_api(){
  cd $ROOT/erizoAPI
  ./build.sh
  cd $CURRENT_DIR
}


install_erizo_controller(){
  cd $ROOT/erizo_controller

  cd erizoClient/tools

  sudo ./compile.sh
  sudo ./compilefc.sh

  echo [erizo_controller] Done, erizo.js compiled
  cd $CURRENT_DIR
}

# Build Erizo 
install_erizo
install_erizo_api
install_erizo_controller

# Build Nuve
DB_DIR="$BUILD_DIR"/db

cd $ROOT

cd nuve/nuveClient/tools

./compile.sh

echo [nuve] Done, nuve.js compiled

cd $CURRENT_DIR

echo "Killing exising node processes"
sudo pkill node || echo "" # Just in case

echo "Starting supervisor services"
sudo supervisorctl start nuve
sudo supervisorctl start erizo
sudo supervisorctl start licode_basic_server

