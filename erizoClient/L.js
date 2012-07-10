var EventDispatcher = function(spec) {
    var that = {};

    spec.dispatcher = {};
    spec.dispatcher.eventListeners = {};

    that.addEventListener = function(eventType, listener) {
        if (spec.dispatcher.eventListeners[eventType] === undefined) {
            spec.dispatcher.eventListeners[eventType] = [];
            spec.dispatcher.eventListeners[eventType].push(listener);
        }
    };

    that.removeEventListener = function(eventType, listener) {
        var index;
        index = spec.dispatcher.eventListeners[eventType].indexOf(listener);
        if (index !== -1) {
            spec.dispatcher.eventListeners[eventType].splice(index,1);
        }
    };

    that.dispatchEvent = function(event) {
        var listener;
        console.log("Event: " + event.type);
        for (listener in spec.dispatcher.eventListeners[event.type]) {
            spec.dispatcher.eventListeners[event.type][listener](event);
        }
    };

    return that;
};

var Publisher = function(spec) {
    // Ex.: Publisher({audio:true, video:false, elementID:undefined})
    var that = EventDispatcher(spec);
    that.room = undefined;
    that.elementID = undefined;
    
    that.hasAudio = function() {
        return spec.audio;
    };

    that.hasVideo = function() {
        return spec.video;
    };

    that.init = function() {
        try {
            navigator.webkitGetUserMedia({video:spec.video,audio:spec.audio}, function(stream) {
                
                console.log("User has granted access to local media.");

                that.stream = Stream({streamID: 12345, stream: stream});

                // Draw on HTML
                L.Utils.addVideoToElement(that.stream.streamID, that.stream.stream, spec.elementID);

                var publisherEvent = PublisherEvent({type:"access-accepted"});
                that.dispatchEvent(publisherEvent);
            }, function(error) {
                alert("Failed to get access to local media. Error code was " + error.code + ".");
            });
            console.log("Requested access to local media");
        } catch (e) {
            console.log(e, "getUserMedia error");
        }
    };

    return that;
};

var Stream = function(spec) {
    var that = {};
    that.elementID = undefined;
    that.streamID = spec.streamID;
    that.stream = spec.stream;
    return that;
};

var Room = function(spec) {
    // Ex.: Room({token:"fdfdasdsadsa"})
    var that = EventDispatcher(spec);
    that.roomID = 10;
    L.Utils.init();

    that.connect = function() {
        // 1- Connect to Erizo-Controller
        var streamList = [];
        // 2- Retrieve list of streams

        // 3 - Update RoomID
        var connectEvt = RoomEvent({type:"room-connected", 
                streams:streamList
            });
        that.dispatchEvent(connectEvt);
    };

    that.disconnect = function() {
        // 1- Disconnect from room

        var disconnectEvt = RoomEvent({type:"room-disconnected"});
        that.dispatchEvent(disconnectEvt);
    };

    that.publish = function(publisher) {
        spec.publisher = publisher;
        publisher.room = that;

        // 1- Publish Stream to Erizo-Controller
        var stream = publisher.stream;

        var evt = StreamEvent({type:"stream-added", stream:stream});
        that.dispatchEvent(evt);
    };

    that.unpublish = function(publisher) {

        // Unpublish stream from Erizo-Controller
        var stream1 = Stream({streamID: "12312321", stream:undefined});

        var evt = StreamEvent({type:"stream-removed", stream:stream1});
        that.dispatchEvent(evt);

    };

    that.subscribe = function(stream, elementID) {
        stream.elementID = elementID;
        // Subscribe to stream stream
        console.log("Subscribing to: " + stream.streamID);
        // Notify Erizo-Controller

        // Draw on html
        L.Utils.addVideoToElement(stream.streamID, stream.stream, elementID);
    };

    that.unsubscribe = function(stream) {
        // Unsubscribe from stream stream

        // Remove 

        // Notify Erizo-Controller

        // Remove from view
        L.Utils.removeVideo(stream.streamID);
    };

    return that;
};


var LynckiaEvent = function(spec) {
    // Ex.: LynckiaEvent({type:"room-connected"});
    var that = {};

    that.type = spec.type;

    return that;
};

var RoomEvent = function(spec) {
    // Ex.: RoomEvent({type:"room-connected", streams:[stream1, stream2]});
    var that = LynckiaEvent(spec);

    that.streams = spec.streams;

    return that;
};

var StreamEvent = function(spec) {
    // Ex.: StreamEvent({type:"stream-added", stream:stream1});
    var that = LynckiaEvent(spec);

    that.stream = spec.stream;

    return that;
};

var PublisherEvent = function(spec) {
    // Ex.: StreamEvent({type:"access-accepted", stream:stream1});
    var that = LynckiaEvent(spec);

    return that;
};