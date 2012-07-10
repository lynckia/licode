var EventDispatcher = function(spec) {
    var that = {};

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
        for (listener in spec.dispatcher.eventListeners[event.type]) {
            listener(event);
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

    return that;
};

var Stream = function(spec) {
    var that = {};
    that.elementID = undefined;
    that.streamID = spec.streamID;
    that.stream = spec.stream;
};

var Room = function(spec) {
    // Ex.: Room({token:"fdfdasdsadsa"})
    var that = EventDispatcher(spec);
    that.connection = {};
    that.roomID = 10;

    that.connect = function() {
        // 1- Connect to Erizo-Controller
        var streamList = [];
        // 2- Retrieve list of streams
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

        // 1- Publish Stream to Erizo-Controller
        var stream1 = Stream({streamID: "12312321", stream:undefined});

        var evt = StreamEvent({type:"stream-added", stream:stream1});
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

        // Notify Erizo-Controller
    };

    that.unsubscribe = function(stream) {
        // Unsubscribe from stream stream

        // Remove 

        // Notify Erizo-Controller
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