window.onload = function () {

    N.API.init("http://toronado.dit.upm.es:3000/", "4fe07f2475556d4402000001", "clave");

    N.API.createToken("4fe07f3875556d4402000002", "alvaro", "guay", function (response) {
        var token = response;
        console.log("Token: " + response);
        L.Logger.setLogLevel(L.Logger.DEBUG);
        //L.Logger.debug("Connected!");
        var room = Room({token: token});

        var publisher = Publisher({audio: true, video: true, elementID: "pepito"});
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
                            div.setAttribute("style", "width: 160px; height: 120px");
                            div.setAttribute("id", "test" + stream.streamID);
                            document.body.appendChild(div);
                            room.subscribe(stream, "test" + stream.streamID);
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
                var element = document.getElementById("test" + stream.streamID);
                element.getParentNode().removeChild(element);
            });

            room.connect();

        });
        publisher.init();
    });
};