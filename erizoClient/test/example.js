var room, roomId, publisher, interval, serverUrl;

roomId1 = "50001b77773e74ee30000002";
roomId2 = "5007cf98a8946ac114000116";
roomId3 = "5007cf9aa8946ac114000117";
roomId4 = "5007cf9ca8946ac114000118";

serverUrl = "http://chotis2.dit.upm.es:3000/";

publisher = Publisher({audio: true, video: true, elementID: "pepito"});

window.onload = function () {

    var room1 = document.getElementById('room1');
    var room2 = document.getElementById('room2');
    var room3 = document.getElementById('room3');
    var room4 = document.getElementById('room4');

     var createToken = function(roomId, userName, role, callback) {

        var req = new XMLHttpRequest();
        var url = serverUrl + 'createToken/' + roomId;
        var body = {username: userName, role: role};

        req.onreadystatechange = function () {
            if (req.readyState === 4) {
                callback(req.responseText);
            }
        };

        req.open('POST', url, true);

        req.setRequestHeader('Content-Type', 'application/json');
        console.log("Sending to " + url + " - " + JSON.stringify(body));
        req.send(JSON.stringify(body));
    };

    var getUsers = function(roomId, callback) {

        var req = new XMLHttpRequest();
        var url = serverUrl + 'getUsers/' + roomId;

        req.onreadystatechange = function () {
            if (req.readyState === 4) {
                callback(req.responseText);
            }
        };

        req.open('GET', url, true);

        console.log("Sending  to " + url);
        req.send();
    };

    var writeUsers = function(id, room, number) {
        room.childNodes[1].innerText = "Room " + id + " - (" + number + " users)";
    };

    var checkUsers = function() {
        getUsers(roomId1, function(users) {
            writeUsers(1, room1, JSON.parse(users).length);
            getUsers(roomId2, function(users) {
                writeUsers(2, room2, JSON.parse(users).length);
                getUsers(roomId3, function(users) {
                    writeUsers(3, room3, JSON.parse(users).length);
                    getUsers(roomId4, function(users) {
                        writeUsers(4, room4, JSON.parse(users).length);
                    });    
                });
            });
        });
    };

    interval = setInterval(checkUsers, 10000);

    checkUsers();

    room1.onmousedown = function(evt) {
        initialize(roomId1);
    };

    room2.onmousedown = function(evt) {
        initialize(roomId2);
    };

    room3.onmousedown = function(evt) {
        initialize(roomId3);
    };

    room4.onmousedown = function(evt) {
        initialize(roomId4);
    };

    var initialize = function(roomId) {
        clearInterval(interval);
        var roomcontainer = document.getElementById("roomcontainer");
        roomcontainer.setAttribute("class", "hide");
        var vidcontainer = document.getElementById("vidcontainer");
        vidcontainer.setAttribute("class", "");

        createToken(roomId, "user", "role", function (response) {
            var token = response;
            L.Logger.setLogLevel(L.Logger.DEBUG);
            //L.Logger.debug("Connected!");
            room = Room({token: token});

            publisher.addEventListener("access-accepted", function () {
                
                var subscribeToStreams = function (streams) {
                    if (!publisher.showing) {
                        publisher.show();
                    }
                    var index, stream;
                    myStreams: for (index in streams) {
                        if (streams.hasOwnProperty(index)) {
                            stream = streams[index];
                            if (publisher.stream !== undefined && publisher.stream.streamID !== stream.streamID) {
                                var div = document.createElement('div');
                                div.setAttribute("style", "width: 320px; height: 240px; position: absolute; top: 10px; left: 10px; background-color: black");
                                div.setAttribute("id", "test" + stream.streamID);

                                // Loader icon
                                var loader = document.createElement('img');
                                loader.setAttribute('style', 'width: 16px; height: 16px; position: absolute; top: 50%; left: 50%; margin-top: -8px; margin-left: -8px');
                                loader.setAttribute('src', '../assets/loader.gif');

                                div.appendChild(loader);

                                for (var i = 2; i < 5; i++) {
                                    var elem = document.getElementById('vid'+i);
                                    if (elem.childNodes.length === 1) {
                                        elem.appendChild(div);
                                        room.subscribe(stream, "test" + stream.streamID);
                                        continue myStreams;
                                    }
                                }
                                console.log("Oh, oh! There are no video tags available");
                            } else {
                                console.log("My own stream");
                            }
                        }
                    }
                };

                room.addEventListener("room-connected", function (roomEvent) {
                    // Publish my stream
                    room.publish(publisher);

                    // Subscribe to other streams
                    subscribeToStreams(roomEvent.streams);
                });

                room.addEventListener("stream-added", function (streamEvent) {
                    // Subscribe to added streams
                    var streams = [];
                    streams.push(streamEvent.stream);
                    subscribeToStreams(streams);
                });

                room.addEventListener("stream-removed", function (streamEvent) {
                    // Remove stream from DOM
                    var stream = streamEvent.stream;
                    if (stream.elementID !== undefined) {
                        console.log("Removing " + stream.elementID);
                        var element = document.getElementById(stream.elementID);
                        element.parentNode.removeChild(element);
                    }
                });

                room.connect();

            });
            publisher.init();
        });
    }
};