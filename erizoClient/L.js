/*
 * Class EventDispatcher provides event handling to sub-classes.
 * It is inherited from Publisher, Room, etc.
 */
var EventDispatcher = function(spec) {
    var that = {};
    
    // Private vars
    spec.dispatcher = {};
    spec.dispatcher.eventListeners = {};

    // Public functions

    // It adds an event listener attached to an event type.
    that.addEventListener = function(eventType, listener) {
        if (spec.dispatcher.eventListeners[eventType] === undefined) {
            spec.dispatcher.eventListeners[eventType] = [];
            spec.dispatcher.eventListeners[eventType].push(listener);
        }
    };

    // It removes an available event listener.
    that.removeEventListener = function(eventType, listener) {
        var index;
        index = spec.dispatcher.eventListeners[eventType].indexOf(listener);
        if (index !== -1) {
            spec.dispatcher.eventListeners[eventType].splice(index,1);
        }
    };

    // It dispatch a new event to the event listeners, based on the type 
    // of event. All events are intended to be LynckiaEvents.
    that.dispatchEvent = function(event) {
        var listener;
        L.Logger.debug("Event: " + event.type);
        for (listener in spec.dispatcher.eventListeners[event.type]) {
            spec.dispatcher.eventListeners[event.type][listener](event);
        }
    };

    return that;
};

/*
 * Class Publisher is used to configure local video and audio that can be 
 * published in the Room. We need to initialize the Publisher before we can
 * use it in the Room.
 * Typical initializacion of Publisher is:
 * var publisher = Publisher({audio:true, video:false, elementID:'divId'});
 * Audio and video variables has to be set to true if we want to publish them
 * in the room.
 * elementID variable indicates the DOM element's ID.
 * It dispatches events like 'access-accepted' when the user has allowed the 
 * use of his camera and/or microphone:
 * publisher.addEventListener('access-accepted', onAccepted); 
 */
var Publisher = function(spec) {
    var that = EventDispatcher(spec);
    that.room = undefined;
    that.elementID = undefined;
    
    // Public functions

    // Indicates if the publisher has audio activated
    that.hasAudio = function() {
        return spec.audio;
    };

    // Indicates if the publisher has video activated
    that.hasVideo = function() {
        return spec.video;
    };

    // Initializes the publisher and tries to retrieve a stream from local video and audio
    // We need to call this method before we can publish it in the room.
    that.init = function() {
        try {
            navigator.webkitGetUserMedia({video:spec.video,audio:spec.audio}, function(stream) {
                
                L.Logger.info("User has granted access to local media.");

                that.stream = Stream({streamID: 12345, stream: stream});

                // Draw on HTML
                if (spec.elementID !== undefined) {
                    L.HTML.addVideoToElement(that.stream.streamID, that.stream.stream, spec.elementID);
                }

                var publisherEvent = PublisherEvent({type:"access-accepted"});
                that.dispatchEvent(publisherEvent);
            }, function(error) {
                L.Logger.error("Failed to get access to local media. Error code was " + error.code + ".");
            });
            L.Logger.debug("Requested access to local media");
        } catch (e) {
            L.Logger.error("Error accessing to local media");
        }
    };

    return that;
};

/*
 * Class Stream represents a local or a remote Stream in the Room. It will handle the WebRTC stream
 * and identify the stream and where it should be drawn.
 */
var Stream = function(spec) {
    var that = {};
    that.elementID = undefined;
    that.streamID = spec.streamID;
    that.stream = spec.stream;
    return that;
};

/*
 * Class Room represents a Lynckia Room. It will handle the connection, local stream publication and 
 * remote stream subscription.
 * Typical Room initialization would be:
 * var room = Room({token:'213h8012hwduahd-321ueiwqewq'});
 * It also handles RoomEvents and StreamEvents. For example:
 * Event 'room-connected' points out that the user has been successfully connected to the room.
 * Event 'room-disconnected' shows that the user has been already disconnected.
 * Event 'stream-added' indicates that there is a new stream available in the room.
 * Event 'stream-removed' shows that a previous available stream has been removed from the room.
 */
var Room = function(spec) {
    var that = EventDispatcher(spec);
    that.roomID = 10;
    L.HTML.init();

    // It stablishes a connection to the room. Once it is done it throws a RoomEvent("room-connected")
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

    // It disconnects from the room, dispatching a new ConnectionEvent("room-disconnected")
    that.disconnect = function() {
        // 1- Disconnect from room

        var disconnectEvt = RoomEvent({type:"room-disconnected"});
        that.dispatchEvent(disconnectEvt);
    };

    // It publishes the local stream given by publisher. Once it is added it throws a 
    // StreamEvent("stream-added").
    that.publish = function(publisher) {
        spec.publisher = publisher;
        publisher.room = that;

        // 1- Publish Stream to Erizo-Controller
        var stream = publisher.stream;

        var evt = StreamEvent({type:"stream-added", stream:stream});
        that.dispatchEvent(evt);
    };

    // It unpublishes the local stream in the room, dispatching a StreamEvent("stream-removed")
    that.unpublish = function(publisher) {

        // Unpublish stream from Erizo-Controller
        var stream1 = Stream({streamID: "12312321", stream:undefined});

        var evt = StreamEvent({type:"stream-removed", stream:stream1});
        that.dispatchEvent(evt);

    };

    // It subscribe to a remote stream and draws it inside the HTML tag given by the ID='elementID'
    that.subscribe = function(stream, elementID) {
        stream.elementID = elementID;
        // Subscribe to stream stream
        L.Logger.info("Subscribing to: " + stream.streamID);
        // Notify Erizo-Controller

        // Draw on html
        L.HTML.addVideoToElement(stream.streamID, stream.stream, elementID);
    };

    // It unsubscribes from the stream, removing the HTML element.
    that.unsubscribe = function(stream) {
        // Unsubscribe from stream stream

        // Remove 

        // Notify Erizo-Controller

        // Remove from view
        L.HTML.removeVideo(stream.streamID);
    };

    return that;
};

// **** EVENTS ****

/*
 * Class LynckiaEvent represents a generic Event in the library.
 * It handles the type of event, that is important when adding
 * event listeners to EventDispatchers and dispatching new events. 
 * A LynckiaEvent can be initialized this way:
 * var event = LynckiaEvent({type: "room-connected"});
 */
var LynckiaEvent = function(spec) {
    var that = {};

    // Event type. Examples are: 'room-connected', 'stream-added', etc.
    that.type = spec.type;

    return that;
};

/*
 * Class RoomEvent represents an Event that happens in a Room. It is a
 * LynckiaEvent.
 * It is usually initialized as:
 * var roomEvent = RoomEvent({type:"room-connected", streams:[stream1, stream2]});
 * Event types:
 * 'room-connected' - points out that the user has been successfully connected to the room.
 * 'room-disconnected' - shows that the user has been already disconnected.
 */
var RoomEvent = function(spec) {
    var that = LynckiaEvent(spec);

    // A list with the streams that are published in the room.
    that.streams = spec.streams;

    return that;
};

/*
 * Class StreamEvent represents an event related to a stream. It is a LynckiaEvent.
 * It is usually initialized this way:
 * var streamEvent = StreamEvent({type:"stream-added", stream:stream1});
 * Event types:
 * 'stream-added' - indicates that there is a new stream available in the room.
 * 'stream-removed' - shows that a previous available stream has been removed from the room.
 */
var StreamEvent = function(spec) {
    var that = LynckiaEvent(spec);

    // The stream related to this event.
    that.stream = spec.stream;

    return that;
};

/*
 * Class PublisherEvent represents an event related to a publisher. It is a LynckiaEvent.
 * It usually initializes as:
 * var publisherEvent = PublisherEvent({})
 * Event types:
 * 'access-accepted' - indicates that the user has accepted to share his camera and microphone
 */
var PublisherEvent = function(spec) {
    var that = LynckiaEvent(spec);

    return that;
};