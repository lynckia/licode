#!/usr/bin/env bash

set -e

mkdir ../dist || true
mkdir ../build || true

google-closure-compiler-js ../src/N.js ../src/N.API.js > ../build/nuve.js

./compileDist.sh
