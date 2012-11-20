#!/bin/bash

cd nuveAPI

node nuve.js &

cd ../cloudHandler

node cloudHandler.js &