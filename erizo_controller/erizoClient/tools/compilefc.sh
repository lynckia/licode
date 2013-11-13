#!/bin/bash

if [ -d $LIB_DIR ]; then
	rm -rf ../build
fi

mkdir ../build

java -jar compiler.jar --js ../src/Events.js --js ../src/webrtc-stacks/FcStack.js --js ../src/webrtc-stacks/ChromeStableStack.js --js ../src/webrtc-stacks/ChromeCanaryStack.js --js ../src/Connection.js --js ../src/Stream.js --js ../src/Room.js --js ../src/utils/L.Logger.js --js ../src/utils/L.Base64.js --js ../src/views/View.js --js ../src/views/VideoPlayer.js --js ../src/views/AudioPlayer.js --js ../src/views/Bar.js --js ../src/views/Speaker.js --js_output_file ../build/erizofc.js

TARGET=../dist/erizofc.js

current_dir=`pwd`

# License
echo '/*' > $TARGET
echo '*/' >> $TARGET

# Body
echo "var io = require('socket.io-client');" >> $TARGET
cat ../build/erizofc.js >> $TARGET
echo 'module.exports = Erizo;' >> $TARGET