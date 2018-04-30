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
  streams.forEach(function (stream) {
    if (localStream.getID() !== stream.getID()) {
      console.log('Updating config');
      stream.updateConfiguration({slideShowMode: slideShowMode}, cb);
    }
  });
}

window.onload = function () {
  recording = false;
  var screen = getParameterByName('screen');
  var roomName = getParameterByName('room') || 'basicExampleRoom';
  var singlePC = getParameterByName('singlePC') || false;
  var roomType = getParameterByName('type') || 'erizo';
  var mediaConfiguration = getParameterByName('mediaConfiguration') || 'default';
  var onlySubscribe = getParameterByName('onlySubscribe');
  var onlyPublish = getParameterByName('onlyPublish');
  console.log('Selected Room', roomName, 'of type', roomType);
  var config = {audio: true,
                video: true,
                data: true,
                screen: screen,
                videoSize: [640, 480, 640, 480],
                videoFrameRate: [10, 20]};
  // If we want screen sharing we have to put our Chrome extension id.
  // The default one only works in our Lynckia test servers.
  // If we are not using chrome, the creation of the stream will fail regardless.
  if (screen){
    config.extensionId = 'okeephmleflklcdebijnponpabbmmgeo';
  }
  localStream = Erizo.Stream(config);
  var createToken = function(roomData, callback) {

    var req = new XMLHttpRequest();
    var url = serverUrl + 'createToken/';

    req.onreadystatechange = function () {
      if (req.readyState === 4) {
        callback(req.responseText);
      }
    };

    req.open('POST', url, true);
    req.setRequestHeader('Content-Type', 'application/json');
    req.send(JSON.stringify(roomData));
  };

  var roomData  = {username: 'user',
                   role: 'presenter',
                   room: roomName,
                   type: roomType,
                   mediaConfiguration: mediaConfiguration};

  createToken(roomData, function (response) {
    var token = response;
    console.log(token);
    room = Erizo.Room({token: token});

    var subscribeToStreams = function (streams) {
      if (onlyPublish) {
        return;
      }
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
      var options = {metadata: {type: 'publisher'}};
      var enableSimulcast = getParameterByName('simulcast');
      if (enableSimulcast) options.simulcast = {numSpatialLayers: 2};

      if (!onlySubscribe) {
        room.publish(localStream, options);
      }
      subscribeToStreams(roomEvent.streams);
    });

    room.addEventListener('stream-subscribed', function(streamEvent) {
      var stream = streamEvent.stream;
      var div = document.createElement('div');
      div.setAttribute('style', 'width: 320px; height: 240px;float:left;');
      div.setAttribute('id', 'test' + stream.getID());

      document.getElementById('videoContainer').appendChild(div);
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
        document.getElementById('videoContainer').removeChild(element);
      }
    });

    room.addEventListener('stream-failed', function (){
        console.log('Stream Failed, act accordingly');
    });

    if (onlySubscribe) {
      room.connect({singlePC: singlePC});
    } else {
      var div = document.createElement('div');
      div.setAttribute('style', 'width: 320px; height: 240px; float:left');
      div.setAttribute('id', 'myVideo');
      document.getElementById('videoContainer').appendChild(div);

      localStream.addEventListener('access-accepted', function () {
        room.connect({singlePC: singlePC});
        localStream.show('myVideo');
      });
      localStream.init();
    }
  });
};
