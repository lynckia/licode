#!/bin/bash
FILE=../dist/erizo.js
rm $FILE
cat ../lib/roap_on_jsep.js >> $FILE
cat ../lib/erizo_on_jsep.js >> $FILE
cat ../lib/socket.io.js >> $FILE
cat ../src/Events.js >> $FILE
cat ../src/webrtc-stacks/ChromeStableStack.js >> $FILE
cat ../src/webrtc-stacks/ChromeCanaryStack.js >> $FILE
cat ../src/Connection.js >> $FILE
cat ../src/Stream.js >> $FILE
cat ../src/Room.js >> $FILE
cat ../src/utils/L.Logger.js >> $FILE
cat ../src/utils/L.Base64.js >> $FILE
cat ../src/views/View.js >> $FILE
cat ../src/views/VideoPlayer.js >> $FILE
cat ../src/views/Bar.js >> $FILE
cat ../src/views/Speaker.js >> $FILE
