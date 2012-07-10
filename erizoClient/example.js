var room = Room({token:"dsuiahduioashdioua"});

var publisher = Publisher({audio:true, video:false, elementID:undefined});

room.addEventListener("room-connected", function(roomEvent) {
    // Publish my stream
    room.publish(publisher);

    // Subscribe to other streams
    subscribeToStreams(roomEvent.streams);
});

room.addEventListener("stream-added", function(streamEvent) {
    // Subscribe to added streams
    subscribeToStreams([streamEvent.stream]);
});

var subscribeToStreams = function(streams) {
    var stream;
    for (stream in streams) {
        if (publisher.stream.streamID === stream.streamID) {
            continue;
        }
        room.subscribe(stream);
    }
};

room.connect();