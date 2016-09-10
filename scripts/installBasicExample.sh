#!/bin/bash

#open port to be accessed from other machine, on firewall.
firewall-cmd --add-port=80/tcp
firewall-cmd --add-port=8080/tcp
firewall-cmd --add-port=3000/tcp
firewall-cmd --add-port=3001/tcp
firewall-cmd --add-port=3004/tcp

/sbin/iptables -I INPUT -p tcp --dport 80 -j ACCEPT
/sbin/iptables -I INPUT -p tcp --dport 8080 -j ACCEPT
/sbin/iptables -I INPUT -p tcp --dport 3000 -j ACCEPT
/sbin/iptables -I INPUT -p tcp --dport 3001 -j ACCEPT
/sbin/iptables -I INPUT -p tcp --dport 3004 -j ACCEPT

SCRIPT=`pwd`/$0
FILENAME=`basename $SCRIPT`
PATHNAME=`dirname $SCRIPT`
ROOT=$PATHNAME/..
BUILD_DIR=$ROOT/build
CURRENT_DIR=`pwd`
DB_DIR="$BUILD_DIR"/db
EXTRAS=$ROOT/extras

cd $EXTRAS/basic_example

cp -r ${ROOT}/erizo_controller/erizoClient/dist/assets public/

npm install --loglevel error express body-parser morgan errorhandler
cd $CURRENT_DIR
