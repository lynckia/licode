var room, publisher;
publisher = Publisher({audio: true, video: true, elementID: "pepito"});
window.onload = function () {

    N.API.init("http://chotis2.dit.upm.es:3000/", "50001b26773e74ee30000001", "clave");

    N.API.createToken("50001b77773e74ee30000002", "user", "role", function (response) {
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
};