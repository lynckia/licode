#!/usr/bin/env bash

set -e

mkdir ../dist || true
mkdir ../build || true

google-closure-compiler-js ../lib/xmlhttprequest.js > ../dist/xmlhttprequest.js

TARGET=../dist/nuve.js

current_dir=`pwd`

# License
echo '/*' > $TARGET
echo '*/' >> $TARGET

# Body
cat ../dist/xmlhttprequest.js >> $TARGET
cat ../build/nuve.js >> $TARGET
echo 'module.exports = N;' >> $TARGET

#cd ../npm/

#tar -czvf package.tgz package/

#cd $current_dir
