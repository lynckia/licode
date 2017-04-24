#!/bin/sh

sudo supervisorctl tail -f nuve stderr &
sudo supervisorctl tail -f erizo stderr &
sudo supervisorctl tail -f erizo_agent stderr &

sudo supervisorctl tail -f nuve &
sudo supervisorctl tail -f erizo &
sudo supervisorctl tail -f erizo_agent &
