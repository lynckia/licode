# Supervisor Scripts and Configuration Files

These scripts may be helpful if you want to run licode using Supervisor.

## configureSupervisor.sh

This script configure 4 supervisor services (nuve, erizo_controller, erizo_agent, and licode_basic_example) by copying config files to /etc/supervisor.  It will then reload supervisor and start the services.

## restart.sh

Because licode processes need to be started in order (or at least did at one point), use this script to stop and start supervisor services in order

## buildAll.sh

Convenient little script that stops all licode services, builds the latest code, and then starts them.