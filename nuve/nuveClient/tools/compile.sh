#!/usr/bin/env bash

set -e

mkdir ../dist || true
mkdir ../build || true

google-closure-compiler-js ../src/hmac-sha1.js ../src/N.js ../src/N.Base64.js ../src/N.API.js > ../build/nuve.js

./compileDist.sh
