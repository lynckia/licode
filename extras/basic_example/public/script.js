/* globals Erizo */
'use strict';
var serverUrl = '/';
var localStream, room, recording, recordingId;

function getParameterByName(name) {
  name = name.replace(/[\[]/, '\\\[').replace(/[\]]/, '\\\]');
  var regex = new RegExp('[\\?&]' + name + '=([^&#]*)'),
      results = regex.exec(location.search);
  return results == null ? '' : decodeURIComponent(results[1].replace(/\+/g, ' '));
}

function testConnection () {  // jshint ignore:line
  window.location = '/connection_test.html';
}

function startRecording () {  // jshint ignore:line
  if (room !== undefined){
    if (!recording){
      room.startRecording(localStream, function(id) {
        recording = true;
        recordingId = id;
      });

    } else {
      room.stopRecording(recordingId);
      recording = false;
    }
  }
}

var slideShowMode = false;

function toggleSlideShowMode() {  // jshint ignore:line
  var streams = room.remoteStreams;
  var cb = function (evt){
      console.log('SlideShowMode changed', evt);
  };
  slideShowMode = !slideShowMode;
  for (var index in streams) {
    var stream = streams[index];
    if (localStream.getID() !== stream.getID()) {
      console.log('Updating config');
      stream.updateConfiguration({slideShowMode: slideShowMode}, cb);
    }
  }
}

window.onload = function () {
  recording = false;
  var screen = getParameterByName('screen');
  var roomName = getParameterByName('room') || 'basicExampleRoom';
  console.log('Selected Room', room);
  var config = {audio: true,
                video: true,
                data: true,
                screen: screen,
                videoSize: [640, 480, 640, 480]};
  // If we want screen sharing we have to put our Chrome extension id.
  // The default one only works in our Lynckia test servers.
  // If we are not using chrome, the creation of the stream will fail regardless.
  if (screen){
    config.extensionId = 'okeephmleflklcdebijnponpabbmmgeo';
  }
  localStream = Erizo.Stream(config);
  var createToken = function(userName, role, roomName, callback) {

    var req = new XMLHttpRequest();
    var url = serverUrl + 'createToken/';
    var body = {username: userName, role: role, room:roomName};

    req.onreadystatechange = function () {
      if (req.readyState === 4) {
        callback(req.responseText);
      }
    };

    req.open('POST', url, true);
    req.setRequestHeader('Content-Type', 'application/json');
    req.send(JSON.stringify(body));
  };

  createToken('user', 'presenter', roomName, function (response) {
    var token = response;
    console.log(token);
    room = Erizo.Room({token: token});

    localStream.addEventListener('access-accepted', function () {
      var subscribeToStreams = function (streams) {
        var cb = function (evt){
            console.log('Bandwidth Alert', evt.msg, evt.bandwidth);
        };
        for (var index in streams) {
          var stream = streams[index];
          if (localStream.getID() !== stream.getID()) {
            room.subscribe(stream, {slideShowMode: slideShowMode, metadata: {type: 'subscriber'}});
            stream.addEventListener('bandwidth-alert', cb);
          }
        }
      };

      room.addEventListener('room-connected', function (roomEvent) {

        room.publish(localStream, {maxVideoBW: 300, metadata: {type: 'publisher'}});
        subscribeToStreams(roomEvent.streams);
      });

      room.addEventListener('stream-subscribed', function(streamEvent) {
        var stream = streamEvent.stream;
        var div = document.createElement('div');
        div.setAttribute('style', 'width: 320px; height: 240px;');
        div.setAttribute('id', 'test' + stream.getID());

        document.body.appendChild(div);
        stream.show('test' + stream.getID());

      });

      room.addEventListener('stream-added', function (streamEvent) {
        var streams = [];
        streams.push(streamEvent.stream);
        subscribeToStreams(streams);
        document.getElementById('recordButton').disabled = false;
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
          console.log('Stream Failed, act accordingly');
      });

      room.connect();

      localStream.show('myVideo');

    });
    localStream.init();
  });
};
