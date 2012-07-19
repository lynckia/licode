#!/bin/bash

TARGET=../npm/package/lib/nuve.js

current_dir=`pwd`

# License
echo '/*' > $TARGET
echo '*/' >> $TARGET

# Body
echo 'var XMLHttpRequest = require("./../vendor/xmlhttprequest").XMLHttpRequest;' >> $TARGET
cat ../build/nuve.js >> $TARGET
echo 'module.exports = N;' >> $TARGET

#cd ../npm/

#tar -czvf package.tgz package/

#cd $current_dir