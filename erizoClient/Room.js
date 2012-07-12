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
var Room = function (spec) {
    "use strict";
    var that = EventDispatcher(spec), connectSocket, sendMessageSocket, sendSDPSocket, streams = {};
    that.roomID = 10;
    that.socket = {};
    L.HTML.init();

    // Private functions

    connectSocket = function (token, callback, error) {
        that.socket = io.connect(token.host, {reconnect: false});

        that.socket.on('onAddStream', function (id) {
            var stream = Stream({streamID: id})
            streams[id] = stream;
            var evt = StreamEvent({type: 'stream-added', stream: stream});
            that.dispatchEvent(evt);
        });

        that.socket.on('onRemoveStream', function (id) {
            var stream = streams[id];
            delete streams[id];
            L.HTML.removeVideo(id);
            var evt = StreamEvent({type: 'stream-removed', stream: stream});
            that.dispatchEvent(evt);
        });

        that.socket.on('disconnect', function (argument) {
            L.Logger.info("Socket disconnected");
        });

        sendMessageSocket('token', token, callback, error);
    };

    sendMessageSocket = function (type, msg, callback, error) {
        that.socket.emit(type, msg, function (respType, msg) {
            if (respType === "success") {
                if (callback !== undefined) {
                    callback(msg);
                }
            } else {
                if (error !== undefined) {7
                    error(msg);
                }
            }
        });
    };

    sendSDPSocket = function (type, state, sdp, callback) {
        that.socket.emit(type, state, sdp, function (response, respCallback) {
            callback(response, respCallback);
        });
    };

    // Public functions

    // It stablishes a connection to the room. Once it is done it throws a RoomEvent("room-connected")
    that.connect = function () {
        // 1- Connect to Erizo-Controller
        var streamList = [];
        var token = L.Base64.decodeBase64(spec.token);
        connectSocket(JSON.parse(token), function (streams) {
            var index = 0, stream, streamList = [];
            // 2- Retrieve list of streams
            // 3 - Update RoomID
            L.Logger.info("Connected!");
            for (index in streams) {
                if (streams.hasOwnProperty(index)) {
                    stream = Stream({streamID: streams[index], stream: undefined});
                    streamList.push(stream);
                    streams[streams[index]] = stream;
                }
            }
            var connectEvt = RoomEvent({type: "room-connected", streams: streamList});
            that.dispatchEvent(connectEvt);
        }, function (error) {
            L.Logger.error("Not Connected! " + error);
        });
    };

    // It disconnects from the room, dispatching a new ConnectionEvent("room-disconnected")
    that.disconnect = function () {
        // 1- Disconnect from room

        var disconnectEvt = RoomEvent({type: "room-disconnected"});
        that.dispatchEvent(disconnectEvt);
    };

    // It publishes the local stream given by publisher. Once it is added it throws a 
    // StreamEvent("stream-added").
    that.publish = function (publisher) {
        spec.publisher = publisher;
        publisher.room = that;

        // 1- Publish Stream to Erizo-Controller
        publisher.pc = new RoapConnection("STUN stun.l.google.com:19302", function(offer){
            sendSDPSocket('publish', 'offer', offer, function (answer, id) {
                publisher.pc.onsignalingmessage = function (ok) {
                    publisher.pc.onsignalingmessage = function() {};
                    sendSDPSocket('publish', 'ok', ok);
                    L.Logger.info('Stream published ' + ok);
                    publisher.stream.streamID = id;
                };
                publisher.pc.processSignalingMessage(answer);
            });
        });
        publisher.pc.addStream(publisher.stream.stream);
    };

    // It unpublishes the local stream in the room, dispatching a StreamEvent("stream-removed")
    that.unpublish = function (publisher) {

        // Unpublish stream from Erizo-Controller
        sendMessageSocket('unpublish', undefined);

    };

    // It subscribe to a remote stream and draws it inside the HTML tag given by the ID='elementID'
    that.subscribe = function (stream, elementID) {
        stream.elementID = elementID;

        // 1- Subscribe to Stream
        stream.pc = new RoapConnection("STUN stun.l.google.com:19302", function (offer){
            sendSDPSocket('subscribe', stream.streamID, offer, function (answer) {
                stream.pc.processSignalingMessage(answer);

            });
        });

        stream.pc.onaddstream = function (evt) {
            // Draw on html
            L.Logger.info('Stream subscribed');
            stream.stream = evt.stream;
            L.HTML.addVideoToElement(stream.streamID, stream.stream, elementID);
        };

        // Subscribe to stream stream
        L.Logger.info("Subscribing to: " + stream.streamID);
    };

    // It unsubscribes from the stream, removing the HTML element.
    that.unsubscribe = function (stream) {
        // Unsubscribe from stream stream

        // Remove 

        // Notify Erizo-Controller

        // Remove from view
        L.HTML.removeVideo(stream.streamID);
    };

    return that;
};