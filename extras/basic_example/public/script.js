/* globals Erizo */
'use strict';
var serverUrl = '/';
var localStream, room, recording, recordingId;

function showLegalCase()
{
    console.log("test showLegalCase ...");
    var conf={video: true, audio: true, attributes: {name: 'LegalCase'},
        //url:"rtsp://user1:222222@111.198.38.42:8554/stream"
        url:"file:///extra_disk/pkg/test.mov"
    };
    var shareScreen = Erizo.Stream(conf);

    shareScreen.addEventListener("access-accepted", function ()
            {
            console.log("test showLegalCase access-accepted");

                    room.publish(shareScreen, {maxVideoBW: 300}, function(id, error){
                        if (id === undefined) {
                        console.log("test Error publishing stream,", error);
                        }
                        else
                        {
                        console.log("test Published stream ", id);
                        }
                        }); 

            });
    shareScreen.init();
}

function GetQueryString(name)
{
    var reg = new RegExp("(^|&)"+ name +"=([^&]*)(&|$)");
    var r = window.location.search.substr(1).match(reg);
    if(r!=null)return  unescape(r[2]); return null;
}

function getParameterByName(name) {
    name = name.replace(/[\[]/, "\\\[").replace(/[\]]/, "\\\]");
    var regex = new RegExp("[\\?&]" + name + "=([^&#]*)"),
        results = regex.exec(location.search);
    return results == null ? "" : decodeURIComponent(results[1].replace(/\+/g, " "));
}

function testConnection () {
    window.location = "/connection_test.html";
}
function startRecording () {
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

window.onload = function () {
    recording = false;
    var screen = getParameterByName("screen");
    var roomName = getParameterByName("room") || "basicExample";

    var config = {audio: true, video: true, data: false, screen: screen, videoSize: [640, 480, 640, 480], attributes: {name: GetQueryString('user')}};
    // If we want screen sharing we have to put our Chrome extension id. The default one only works in our Lynckia test servers.
    // If we are not using chrome, the creation of the stream will fail regardless.
    //if (screen){
        //config.extensionId = "okeephmleflklcdebijnponpabbmmgeo";
    //}


    var configrtsp={video: true, audio: true, attributes: {name: 'LegalCase'},
        //url:"rtsp://user1:222222@111.198.38.42:8554/stream"
        url:"file:///extra_disk/pkg/test.mov"
    };
    //var shareScreen = Erizo.Stream(conf);

    if(GetQueryString('user')=='wen') {
    //localStream = Erizo.Stream(configrtsp);
    localStream = Erizo.Stream(config);
    } else {
        localStream = Erizo.Stream(config);
    }
    var createToken = function(userName, role, roomName, callback) {

        var req = new XMLHttpRequest();
        var url = serverUrl + 'createToken/';
        var body = {username: userName, role: role, room_id:'58109e428177fdb8dd7fe20f'};

        console.log('test creating token with ', body);
        req.onreadystatechange = function () {
            if (req.readyState === 4) {
                callback(req.responseText);
            }
        };

        req.open('POST', url, true);
        req.setRequestHeader('Content-Type', 'application/json');
        req.send(JSON.stringify(body));
    };

    createToken(GetQueryString("user"), "presenter", roomName, function (response) {
            var token = response;
            console.log('test token:', token);
            room = Erizo.Room({token: token});

            localStream.addEventListener("access-accepted", function (){ 
                console.log('test localStream access-accepted');

                var subscribeToStreams = function(streams){
                for (var index in streams) {
                var stream = streams[index];
                var attributes = stream.getAttributes();
                if (localStream.getID() !== stream.getID()) {
                room.subscribe(stream);
                console.log("test subscribed stream ", attributes.name, ', id ', stream.getID());
                stream.addEventListener("bandwidth-alert", function (evt){
                    console.log("test Bandwidth Alert", evt.msg, evt.bandwidth);
                    });
                }
                else {
                console.log('test NOT subscribe ', attributes.name, ', id ', stream.getID());
                }
                }
                }

                room.addEventListener("room-connected", function (roomEvent){

                    console.log('test room-connected');

                    if(GetQueryString('user')=='wen') {
                        showLegalCase();
                    }
                    //else{
                        
                    room.publish(localStream, {maxVideoBW: 300}, function(id, error){
                    
                        if (id === undefined) {
                                console.log("test Error publishing stream,", error);
                        }
                        else {
                                console.log("test Published stream ", id);
                        }
                        }); 

                    subscribeToStreams(roomEvent.streams);
                    //subscribeToStreams(room.remoteStreams);
                    //}
                });

                room.addEventListener("stream-subscribed", function(streamEvent) {
                        var stream = streamEvent.stream;
                        var div = document.createElement('div');
                        div.setAttribute("style", "width: 320px; height: 240px;");
                        div.setAttribute("id", "test" + stream.getID());
                        document.body.appendChild(div);
                        stream.play("test" + stream.getID());
                        });
                room.addEventListener("stream-added", function (streamEvent) {
                        var streams = [];
                        streams.push(streamEvent.stream);
                        console.log('test room stream-added ', streamEvent.stream.getID());
                        subscribeToStreams(streams);
                        document.getElementById("recordButton").disabled = false;
                        });

                room.addEventListener("stream-removed", function (streamEvent) {
                        // Remove stream from DOM

                        var attributes = streamEvent.stream.getAttributes();
                        console.log("test Stream Failed ", attributes.name, ' id ', streamEvent.stream.getID());

                        var stream = streamEvent.stream;
                        if (stream.elementID !== undefined) {
                        var element = document.getElementById(stream.elementID);
                        document.body.removeChild(element);
                        }
                        });

                room.addEventListener("stream-failed", function (streamEvent){
                        var attributes = streamEvent.stream.getAttributes();
                        console.log("test Stream Failed ", attributes.name, ' id ', streamEvent.stream.getID());
                        });

                room.connect();

                localStream.play("myVideo");

                });
                localStream.init();
            });
    };
