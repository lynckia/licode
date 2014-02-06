var serverUrl = "/";
var localStream, room, recording, localScreenStream, initiallyPublishedStream;
var screenSharing = false;
var togglingStream = false;
var localScreenStreamId, localStreamId;


function getParameterByName(name) {
  name = name.replace(/[\[]/, "\\\[").replace(/[\]]/, "\\\]");
  var regex = new RegExp("[\\?&]" + name + "=([^&#]*)"),
      results = regex.exec(location.search);
  return results == null ? "" : decodeURIComponent(results[1].replace(/\+/g, " "));
}

function startRecording (){
  if (room!=undefined){
    if (!recording){
      room.startRecording(localStream);
      recording = true;
    }else{
      room.stopRecording(localStream);
      recording = false;
    }
  }
}

window.onload = function () {
  recording = false;
  var screen = getParameterByName("screen");

  localStream = Erizo.Stream({audio: true, video: true, data: true, videoSize: [640, 480, 640, 480]});

  var initiallyPublishedStream = localStream;

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

  createToken("user", "presenter", function (response) {
    var token = response;
    console.log(token);
    room = Erizo.Room({token: token});

    localStream.addEventListener("access-accepted", function () {
      var subscribeToStreams = function (streams) {
        for (var index in streams) {
          var stream = streams[index];
          if ((!localStream || localStream.getID() !== stream.getID()) && (!localScreenStream || localScreenStream.getID() !== stream.getID())) {
              room.subscribe(stream);
          }
        }
      };

      room.addEventListener("room-connected", function (roomEvent) {

        room.publish(initiallyPublishedStream, {maxVideoBW: 300});

        subscribeToStreams(roomEvent.streams);
      });

      room.addEventListener("stream-subscribed", function(streamEvent) {
        var stream = streamEvent.stream;
        var div = document.createElement('div');
        div.setAttribute("style", "width: 320px; height: 240px; float: left");
        div.setAttribute("id", "test" + stream.getID());

        document.body.appendChild(div);
        stream.show("test" + stream.getID());

      });

      room.addEventListener("stream-added", function (streamEvent) {
        var streams = [];
        streams.push(streamEvent.stream);
        subscribeToStreams(streams);

        if (localScreenStream && localScreenStream.getID() === streamEvent.stream.getID()) {
          localScreenStreamId = streamEvent.stream.getID()
          togglingStream = false;
          screenSharing = true;
        }

        if (localStream.getID() === streamEvent.stream.getID()) {
          localStreamId = streamEvent.stream.getID();
          togglingStream = false;
          screenSharing = false;
        }

      });

      room.addEventListener("stream-removed", function (event) {
        // Remove stream from DOM
        var stream = event.stream;

        if (stream.elementID !== undefined) {
          var element = document.getElementById(stream.elementID);
          document.body.removeChild(element);
        }

        if (localScreenStreamId=== event.stream.getID()) {
          room.publish(localStream);

          document.getElementById("myVideo").innerHTML = "My Video";
          localStream.show("myVideo") ;
        }

        if (localStreamId === event.stream.getID()) {
          getLocalScreenStream(
            function(stream) {
              room.publish(stream);
              document.getElementById("myVideo").innerHTML = "Sharing Screen";
            },
            function() {
              console.log("[debug] Access rejected to local stream")
            }
          );
        }

      });

      room.connect();

      localStream.show("myVideo");

    });


    localStream.init();
  }); // end on createToken
}; // end window.onLoad



function getLocalScreenStream(successCallback, errorCallback) {
  console.log("[licode] local screen stream")
  if(localScreenStream) {
    return successCallback(localScreenStream);
  }
  else {
    console.log("[licode] need to create a new local screen stream")

    localScreenStream = Erizo.Stream({
        audio: true, video: true, data: false, screen: true, 
        attributes: { isScreen: true }
    });


    localScreenStream.addEventListener("access-accepted", function() {
      console.log("[licode] Access accepted to local Screen Stream")
      successCallback(localScreenStream);   

    });

    localScreenStream.addEventListener("access-rejected", function() { throw new Error("Access rejected"); errorCallback } );
   
    localScreenStream.init();

    if(screen) {
      toggleScreenSharing();
    }
    
    return;

  }
}; // end getLocalScreenStream


function toggleScreenSharing() {
  if(togglingStream) {
    return;
  }
  togglingStream = true;
  if(screenSharing) {
    room.unpublish(localScreenStream);
    localScreenStream.hide();
  }
  else {
    room.unpublish(localStream);
    localStream.hide();
  }
}
