
var uid = genRandomUid(),
    serverUrl = "/",
    erizoStreamConfig = {audio: true, video: true, data: true, videoSize: [640, 480, 640, 480], attributes:{uid: uid}},
    room,
    localStream = Erizo.Stream(erizoStreamConfig);

window.onload = function () {
  createToken("user", "presenter", onCreateToken);
};

function createToken(userName, role, callback) {  // for retriving token from the server.

  var req = new XMLHttpRequest(),
      url = serverUrl + 'createToken/',
      body = {username: userName, role: role};

  req.onreadystatechange = function () {
    if (req.readyState === 4) {
      callback(req.responseText);
    }
  };

  req.open('POST', url, true);
  req.setRequestHeader('Content-Type', 'application/json');
  req.send(JSON.stringify(body));
}

function onCreateToken(token){

  room = Erizo.Room({token: token});

  room.addEventListener("room-connected", function (roomEvent) {
    printText('Connected to the room OK');
    for(var key in room.remoteStreams)
      if(room.remoteStreams[key]) room.subscribe(room.remoteStreams[key]);
    room.publish(localStream, {maxVideoBW: 300});
  });

  room.addEventListener("stream-subscribed", function(streamEvent) {
    if(streamEvent.stream.getAttributes().uid === uid)
      printText('Subscribed to Local stream OK');
    else
     printText('Subscribed to Remote stream OK');

    streamEvent.stream.show("my_subscribed_video");
  });

  room.addEventListener("stream-added", function (streamEvent) {
    if(streamEvent.stream.getAttributes().uid === uid){
      printText('Local stream published OK');
    }
    room.subscribe(streamEvent.stream);
  });

  room.addEventListener("stream-removed", function (streamEvent) { // Remove video element from DOM
    if (!streamEvent.stream.elementID) return;
    var element = document.getElementById(streamEvent.stream.elementID);
    document.body.removeChild(element);
  });

  room.addEventListener("stream-failed", function (streamEvent){
      printText('STREAM FAILED, DISCONNECTION');
      room.disconnect();
  });

  localStream.addEventListener("access-accepted", function () {
    printText('Mic and Cam OK');
    room.connect();
    localStream.show("my_local_video");
  });

  localStream.addEventListener('access-denied', function(event) {
    printText("Access to webcam and microphone rejected");
  });

  localStream.init();
}

// util functions...

function genRandomUid(){
  var result = '', i = 7, chars = '0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ';
  for (; i > 0; --i) result += chars[Math.round(Math.random() * (chars.length - 1))];
  return result;
}

function printText(text) {
  console.log('log: ', text);
  document.getElementById('messages').value += '- ' + text + '\n';
}
