#!/bin/bash

sudo pkill node

sudo supervisorctl stop licode_basic_server
sudo supervisorctl stop erizo
sudo supervisorctl stop nuve

sudo supervisorctl start nuve
sleep 1
sudo supervisorctl start erizo
sleep 1
sudo supervisorctl start licode_basic_server