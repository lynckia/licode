process.env.NODE_TLS_REJECT_UNAUTHORIZED = "0";  // We need this for testing with self signed certs

var XMLHttpRequest = require("xmlhttprequest").XMLHttpRequest,
    Erizo = require('./erizofc'),
    NativeStack = require ('./NativeStack.js');

Erizo.Connection = NativeStack.FakeConnection;


exports.ErizoSimpleNativeConnection = function (spec, callback){
    var that = {};

    //  var localStreamConfig = spec.publishConfig || {video:false, audio:false, data: true},

    var createToken = function(userName, role, callback) {

        var req = new XMLHttpRequest();
        var url = spec.serverUrl + 'createToken/';
        console.log("Url to createToken", url);
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

    var subscribeToStreams = function(streams){
        for (var index in streams){
            if (streams[index].hasVideo()){
                if (spec.subscribeConfig){
                    room.subscribe(streams[index], spec.subscribeConfig);
                }
            }
        }
    };

    var createConnection = function (){
        
        createToken("name", "presenter", function (token) {
            console.log("Getting token", token);
            
            room = Erizo.Room({token: token});
            var sendInterval;


            room.addEventListener("room-connected", function(roomEvent) {
                console.log("Connected to room");
                callback('room-connected');
                subscribeToStreams(roomEvent.streams);
                if (spec.publishConfig !== undefined){
                    console.log("Will publish with config", spec.publishConfig);
                    localStream = Erizo.Stream(spec.publishConfig);
                    room.publish(localStream, spec.publishConfig, function(id, message){
                        if (id === undefined){
                            console.log("ERROR", message);
                            callback("Error "+message);
                        }
                    });
                }
            });

            room.addEventListener("stream-added", function(roomEvent) {
                console.log('stream added', roomEvent.stream.getID());
                if (roomEvent.stream.getID()!==localStream.getID()){
                    var streams = [];
                    streams.push(roomEvent.stream)
                        subscribeToStreams(streams);
                }

            });
            room.connect();
        });

        that.sendData = function(data){
            localStream.sendData({msg:"Testing Data Connection"});
        };
    }

    that.close = function(){
        console.log("Close");
        room.disconnect();
    };

    createConnection();
    return that;
}

