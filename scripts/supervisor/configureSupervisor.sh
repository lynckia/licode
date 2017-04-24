#!/bin/sh

SUPERVISOR_CONFIG_DIR=/etc/supervisor.d

set -e

SCRIPT=`pwd`/$0
PATHNAME=`dirname $SCRIPT`


if [ ! -d $SUPERVISOR_CONFIG_DIR ]; then
  echo "There is no /etc/supervisor.d directory.  Maybe supervisor isn't installed."
  exit 1;
fi

echo "copying configuration files to $SUPERVISOR_CONFIG_DIR"
sudo cp -r $PATHNAME/supervisor.d/* $SUPERVISOR_CONFIG_DIR

echo "Running supervisorctl reload"
sudo supervisorctl reload

# Services need to be run in order
$PATHNAME/restart.sh

sudo supervisorctl status