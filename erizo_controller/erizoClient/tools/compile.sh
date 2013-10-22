#!/bin/bash

#java -jar compiler.jar --js ../lib/socket.io.js --js ../src/Events.js --js ../src/webrtc-stacks/FcStack.js --js ../src/webrtc-stacks/FirefoxStack.js --js ../src/webrtc-stacks/ChromeStableStack.js --js ../src/webrtc-stacks/ChromeCanaryStack.js --js ../src/Connection.js --js ../src/Stream.js --js ../src/Room.js --js ../src/utils/L.Logger.js --js ../src/utils/L.Base64.js --js ../src/views/View.js --js ../src/views/VideoPlayer.js --js ../src/views/Bar.js --js ../src/views/Speaker.js --js_output_file ../dist/erizo.js

cat ../lib/socket.io.js  ../src/Events.js  ../src/webrtc-stacks/FcStack.js  ../src/webrtc-stacks/FirefoxStack.js  ../src/webrtc-stacks/ChromeStableStack.js  ../src/webrtc-stacks/ChromeCanaryStack.js  ../src/Connection.js  ../src/Stream.js  ../src/Room.js  ../src/utils/L.Logger.js  ../src/utils/L.Base64.js  ../src/views/View.js  ../src/views/VideoPlayer.js  ../src/views/Bar.js  ../src/views/Speaker.js > ../dist/erizo.js
