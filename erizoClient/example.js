window.onload = function() {

    var room = Room({token:"dsuiahduioashdioua"});

    var publisher = Publisher({audio:true, video:true, elementID:"pepito"});
    publisher.addEventListener("access-accepted", function() {
        console.log(publisher.stream);
        room.addEventListener("room-connected", function(roomEvent) {
            // Publish my stream
            room.publish(publisher);

            // Subscribe to other streams
            subscribeToStreams(roomEvent.streams);
        });

        room.addEventListener("stream-added", function(streamEvent) {
            // Subscribe to added streams
            var streams = [];
            streams.push(streamEvent.stream);
            subscribeToStreams(streams);
        });

        var subscribeToStreams = function(streams) {
            var index, stream;
            stream_loop: for (index in streams) {
                stream = streams[index];
                if (publisher.stream !== undefined && publisher.stream.streamID === stream.streamID) {
                    continue stream_loop;
                }
                console.log(stream);
                var div = document.createElement('div');
                div.setAttribute("style", "width: 100%; height: 100%");
                div.setAttribute("id", "test"+stream.streamID);
                document.body.appendChild(div);
                room.subscribe(stream, "test"+stream.streamID);
            }
        };

        room.connect();

    });
    publisher.init();


};