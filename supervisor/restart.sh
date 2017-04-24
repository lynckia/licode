#!/bin/bash

sudo pkill node

sudo supervisorctl stop licode_basic_server
sudo supervisorctl stop erizo
sudo supervisorctl start erizo_agent
sudo supervisorctl stop nuve

sudo supervisorctl start nuve
sleep 1
sudo supervisorctl start erizo
sudo supervisorctl start erizo_agent
sleep 1
sudo supervisorctl start licode_basic_server