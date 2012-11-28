/*
 * Class EventDispatcher provides event handling to sub-classes.
 * It is inherited from Publisher, Room, etc.
 */
var Erizo = Erizo || {};
Erizo.EventDispatcher = function (spec) {
    "use strict";
    var that = {};
    // Private vars
    spec.dispatcher = {};
    spec.dispatcher.eventListeners = {};

    // Public functions

    // It adds an event listener attached to an event type.
    that.addEventListener = function (eventType, listener) {
        if (spec.dispatcher.eventListeners[eventType] === undefined) {
            spec.dispatcher.eventListeners[eventType] = [];
        }
        spec.dispatcher.eventListeners[eventType].push(listener);
    };

    // It removes an available event listener.
    that.removeEventListener = function (eventType, listener) {
        var index;
        index = spec.dispatcher.eventListeners[eventType].indexOf(listener);
        if (index !== -1) {
            spec.dispatcher.eventListeners[eventType].splice(index, 1);
        }
    };

    // It dispatch a new event to the event listeners, based on the type 
    // of event. All events are intended to be LynckiaEvents.
    that.dispatchEvent = function (event) {
        var listener;
        L.Logger.debug("Event: " + event.type);
        for (listener in spec.dispatcher.eventListeners[event.type]) {
            if (spec.dispatcher.eventListeners[event.type].hasOwnProperty(listener)) {
                spec.dispatcher.eventListeners[event.type][listener](event);
            }
        }
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
Erizo.LynckiaEvent = function (spec) {
    "use strict";
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
Erizo.RoomEvent = function (spec) {
    "use strict";
    var that = Erizo.LynckiaEvent(spec);

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
Erizo.StreamEvent = function (spec) {
    "use strict";
    var that = Erizo.LynckiaEvent(spec);

    // The stream related to this event.
    that.stream = spec.stream;

    that.msg = spec.msg;

    return that;
};

/*
 * Class PublisherEvent represents an event related to a publisher. It is a LynckiaEvent.
 * It usually initializes as:
 * var publisherEvent = PublisherEvent({})
 * Event types:
 * 'access-accepted' - indicates that the user has accepted to share his camera and microphone
 */
Erizo.PublisherEvent = function (spec) {
    "use strict";
    var that = Erizo.LynckiaEvent(spec);

    return that;
};