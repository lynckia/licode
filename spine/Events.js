/* global */
const logger = require('./logger').logger;

const log = logger.getLogger('Events');

/*
 * Class EventDispatcher provides event handling to sub-classes.
 * It is inherited from Publisher, Room, etc.
 */
const EventDispatcher = () => {
  const that = {};
  // Private vars
  const dispatcher = {
    eventListeners: {},
  };

  // Public functions

  // It adds an event listener attached to an event type.
  that.addEventListener = (eventType, listener) => {
    if (dispatcher.eventListeners[eventType] === undefined) {
      dispatcher.eventListeners[eventType] = [];
    }
    dispatcher.eventListeners[eventType].push(listener);
  };

  // It removes an available event listener.
  that.removeEventListener = (eventType, listener) => {
    if (!dispatcher.eventListeners[eventType]) {
      return;
    }
    const index = dispatcher.eventListeners[eventType].indexOf(listener);
    if (index !== -1) {
      dispatcher.eventListeners[eventType].splice(index, 1);
    }
  };

  // It dispatch a new event to the event listeners, based on the type
  // of event. All events are intended to be LicodeEvents.
  that.dispatchEvent = (event) => {
    if (!event || !event.type) {
      throw new Error('Undefined event');
    }
    log.debug(`Event: ${event.type}`);
    const listeners = dispatcher.eventListeners[event.type] || [];
    for (let i = 0; i < listeners.length; i += 1) {
      listeners[i](event);
    }
  };

  that.on = that.addEventListener;
  that.off = that.removeEventListener;
  that.emit = that.dispatchEvent;

  return that;
};

class EventEmitter {
  constructor() {
    this.emitter = EventDispatcher();
  }
  addEventListener(eventType, listener) {
    this.emitter.addEventListener(eventType, listener);
  }
  removeEventListener(eventType, listener) {
    this.emitter.removeEventListener(eventType, listener);
  }
  dispatchEvent(evt) {
    this.emitter.dispatchEvent(evt);
  }
  on(eventType, listener) {
    this.addEventListener(eventType, listener);
  }
  off(eventType, listener) {
    this.removeEventListener(eventType, listener);
  }
  emit(evt) {
    this.dispatchEvent(evt);
  }
}


// **** EVENTS ****

/*
 * Class LicodeEvent represents a generic Event in the library.
 * It handles the type of event, that is important when adding
 * event listeners to EventDispatchers and dispatching new events.
 * A LicodeEvent can be initialized this way:
 * var event = LicodeEvent({type: "room-connected"});
 */
const LicodeEvent = (spec) => {
  const that = {};

  // Event type. Examples are: 'room-connected', 'stream-added', etc.
  that.type = spec.type;

  return that;
};

/*
 * Class ConnectionEvent represents an Event that happens in a Room. It is a
 * LicodeEvent.
 * It is usually initialized as:
 * var roomEvent = RoomEvent({type:"stream-added", streams:[stream1, stream2]});
 * Event types:
 * 'stream-added' - a stream has been added to the connection.
 * 'stream-removed' - a stream has been removed from the connection.
 * 'ice-state-change' - ICE state changed
 */
const ConnectionEvent = (spec) => {
  const that = LicodeEvent(spec);

  that.stream = spec.stream;
  that.state = spec.state;

  return that;
};

/*
 * Class RoomEvent represents an Event that happens in a Room. It is a
 * LicodeEvent.
 * It is usually initialized as:
 * var roomEvent = RoomEvent({type:"room-connected", streams:[stream1, stream2]});
 * Event types:
 * 'room-connected' - points out that the user has been successfully connected to the room.
 * 'room-disconnected' - shows that the user has been already disconnected.
 */
const RoomEvent = (spec) => {
  const that = LicodeEvent(spec);

  // A list with the streams that are published in the room.
  that.streams = spec.streams;
  that.message = spec.message;

  return that;
};

/*
 * Class StreamEvent represents an event related to a stream. It is a LicodeEvent.
 * It is usually initialized this way:
 * var streamEvent = StreamEvent({type:"stream-added", stream:stream1});
 * Event types:
 * 'stream-added' - indicates that there is a new stream available in the room.
 * 'stream-removed' - shows that a previous available stream has been removed from the room.
 */
const StreamEvent = (spec) => {
  const that = LicodeEvent(spec);

  // The stream related to this event.
  that.stream = spec.stream;

  that.msg = spec.msg;
  that.bandwidth = spec.bandwidth;
  that.attrs = spec.attrs;

  return that;
};

/*
 * Class PublisherEvent represents an event related to a publisher. It is a LicodeEvent.
 * It usually initializes as:
 * var publisherEvent = PublisherEvent({})
 * Event types:
 * 'access-accepted' - indicates that the user has accepted to share his camera and microphone
 */
const PublisherEvent = (spec) => {
  const that = LicodeEvent(spec);

  return that;
};

exports.EventEmitter = EventEmitter;
exports.EventDispatcher = EventDispatcher;
exports.ConnectionEvent = ConnectionEvent;
//exports = { EventDispatcher, EventEmitter, LicodeEvent, RoomEvent, StreamEvent, PublisherEvent,
//  ConnectionEvent };
