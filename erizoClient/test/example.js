var room, publisher;
publisher = Publisher({audio: true, video: true, elementID: "pepito"});
var vid_index = 2;
window.onload = function () {

    N.API.init("http://chotis2.dit.upm.es:3000/", "50001b26773e74ee30000001", "clave");

    N.API.createToken("50001b77773e74ee30000002", "alvaro", "guay", function (response) {
        var token = response;
        console.log("Token: " + response);
        L.Logger.setLogLevel(L.Logger.DEBUG);
        //L.Logger.debug("Connected!");
        room = Room({token: token});

        publisher.addEventListener("access-accepted", function () {
            console.log(publisher.stream);

            var subscribeToStreams = function (streams) {
                var index, stream;
                for (index in streams) {
                    if (streams.hasOwnProperty(index)) {
                        stream = streams[index];
                        if (publisher.stream !== undefined && publisher.stream.streamID !== stream.streamID) {
                            console.log(publisher.stream.streamID, stream.streamID);
                            var div = document.createElement('div');
                            div.setAttribute("style", "width: 320px; height: 240px; position: absolute; top: 10px; left: 10px;");
                            div.setAttribute("id", "test" + stream.streamID);
                            console.log('vid'+vid_index);
                            document.getElementById('vid'+vid_index).appendChild(div);
                            room.subscribe(stream, "test" + stream.streamID);
                            vid_index = vid_index + 1;
                        } else {
                            console.log("Soy yo!");
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