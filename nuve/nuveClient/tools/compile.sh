#!/usr/bin/env bash

set -e

mkdir ../dist || true
mkdir ../build || true

../../node_modules/google-closure-compiler-js/cmd.js ../src/N.js ../src/N.API.js > ../build/nuve.js

./compileDist.sh
