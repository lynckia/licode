/* globals Erizo */
'use strict';
var serverUrl = '/';
var localStream, room;

function printText(text) {
  document.getElementById('messages').value += '- ' + text + '\n';
}

window.onload = function () {
  var config = {audio: true, video: true, data: true, videoSize: [640, 480, 640, 480]};
  localStream = Erizo.Stream(config);
  var createToken = function(userName, role, callback) {

    var req = new XMLHttpRequest();
    var url = serverUrl + 'createToken/';
    var body = {username: userName, role: role};

    req.onreadystatechange = function () {
      if (req.readyState === 4) {
        callback(req.responseText);
      }
    };

    req.open('POST', url, true);
    req.setRequestHeader('Content-Type', 'application/json');
    req.send(JSON.stringify(body));
  };

  createToken('user', 'presenter', function (response) {
    var token = response;
    console.log(token);
    room = Erizo.Room({token: token});

    localStream.addEventListener('access-accepted', function () {
      printText('Mic and Cam OK');
      var subscribeToStreams = function (streams) {
        for (var index in streams) {
          var stream = streams[index];
          room.subscribe(stream);
        }
      };

      room.addEventListener('room-connected', function () {
        printText('Connected to the room OK');
        room.publish(localStream, {maxVideoBW: 300});
      });

      room.addEventListener('stream-subscribed', function(streamEvent) {
        printText('Subscribed to your local stream OK');
        var stream = streamEvent.stream;
        stream.show('my_subscribed_video');

      });

      room.addEventListener('stream-added', function (streamEvent) {
        printText('Local stream published OK');
        var streams = [];
        streams.push(streamEvent.stream);
        subscribeToStreams(streams);
      });

      room.addEventListener('stream-removed', function (streamEvent) {
        // Remove stream from DOM
        var stream = streamEvent.stream;
        if (stream.elementID !== undefined) {
          var element = document.getElementById(stream.elementID);
          document.body.removeChild(element);
        }
      });

      room.addEventListener('stream-failed', function (){
          console.log('STREAM FAILED, DISCONNECTION');
          printText('STREAM FAILED, DISCONNECTION');
          room.disconnect();
      });

      room.connect();

      localStream.show('my_local_video');

    });
    localStream.init();
  });
};
