#!/usr/bin/env bash

set -e

if [ -d $LIB_DIR ]; then
	rm -rf ../build
fi

mkdir ../build

google-closure-compiler-js ../src/Events.js ../src/webrtc-stacks/FcStack.js ../src/webrtc-stacks/ChromeStableStack.js ../src/webrtc-stacks/ChromeCanaryStack.js ../src/Connection.js ../src/Stream.js ../src/Room.js ../src/utils/L.Logger.js ../src/utils/L.Base64.js ../src/views/View.js ../src/views/VideoPlayer.js ../src/views/AudioPlayer.js ../src/views/Bar.js ../src/views/Speaker.js > ../build/erizofc.js

TARGET=../dist/erizofc.js

current_dir=`pwd`

# License
echo '/*' > $TARGET
echo '*/' >> $TARGET

# Body
echo "var io = require('socket.io-client');" >> $TARGET
cat ../build/erizofc.js >> $TARGET
echo 'module.exports = {Erizo: Erizo, L:L};' >> $TARGET
