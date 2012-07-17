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
    var that = EventDispatcher(spec), connectSocket, sendMessageSocket, sendSDPSocket, removeStream, DISCONNECTED = 0, CONNECTING = 1, CONNECTED = 0; 
    that.streams = {};
    that.roomID = '';
    that.socket = {};
    that.state = DISCONNECTED;
    L.HTML.init();

    that.addEventListener("room-disconnected", function(evt) {
        var index;
        that.state = DISCONNECTED;

        // Remove all streams
        for (index in that.streams) {
            if (that.streams.hasOwnProperty(index)) {
                var stream = that.streams[index];
                removeStream(stream);
                delete that.streams[index];
                var evt = StreamEvent({type: 'stream-removed', stream: stream});
                that.dispatchEvent(evt);
            }
        }
        that.streams = {};

        // Close Peer Connection
        spec.publisher.pc.close();
        spec.publisher.pc = undefined;

        // Close socket
        try {
            that.socket.disconnect();
        } catch (error) {
            L.Logger.debug("Socket already disconnected");
        }
        that.socket = undefined;
    });

    // Private functions

    // It removes the stream from HTML and close the PeerConnection associated 
    removeStream = function(stream) {
        if (stream.stream !== undefined) {
            // Remove HTML element
            L.HTML.removeVideo(stream.streamID);
            // Close PC stream
            stream.pc.close();
        }
    }

    // It connects to the server through socket.io
    connectSocket = function (token, callback, error) {
        // Once we have connected

        //that.socket = io.connect("hpcm.dit.upm.es:8080", {reconnect: false});
        that.socket = io.connect(token.host, {reconnect: false});

        // We receive an event with a new stream in the room
        that.socket.on('onAddStream', function (id) {
            var stream = Stream({streamID: id})
            that.streams[id] = stream;
            var evt = StreamEvent({type: 'stream-added', stream: stream});
            that.dispatchEvent(evt);
        });

        // We receive an event of a stream removed from the room
        that.socket.on('onRemoveStream', function (id) {
            var stream = that.streams[id];
            delete that.streams[id];
            removeStream(stream);
            var evt = StreamEvent({type: 'stream-removed', stream: stream});
            that.dispatchEvent(evt);
        });

        // The socket has disconnected
        that.socket.on('disconnect', function (argument) {
            L.Logger.info("Socket disconnected");
            var disconnectEvt = RoomEvent({type: "room-disconnected"});
            that.dispatchEvent(disconnectEvt);
        });

        // First message with the token
        sendMessageSocket('token', token, callback, error);
    };

    // Function to send a message to the server using socket.io
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

    // It sends a SDP message to the server using socket.io
    sendSDPSocket = function (type, state, sdp, callback) {
        that.socket.emit(type, state, sdp, function (response, respCallback) {
            callback(response, respCallback);
        });
    };

    // Public functions

    // It stablishes a connection to the room. Once it is done it throws a RoomEvent("room-connected")
    that.connect = function () {

        if (that.state !== DISCONNECTED) {
            L.Logger.error("Room already connected");
        }

        // 1- Connect to Erizo-Controller
        var streamList = [];
        var token = L.Base64.decodeBase64(spec.token);
        that.state = CONNECTING;
        connectSocket(JSON.parse(token), function (response) {
            var index = 0, stream, streamList = [], streams, roomId;
            streams = response.streams;
            roomId = response.id;
            that.state = CONNECTED;

            // 2- Retrieve list of streams
            for (index in streams) {
                if (streams.hasOwnProperty(index)) {
                    stream = Stream({streamID: streams[index], stream: undefined});
                    streamList.push(stream);
                    that.streams[streams[index]] = stream;
                }
            }

            // 3 - Update RoomID
            that.roomID = roomId;

            L.Logger.info("Connected to room " + that.roomID);

            var connectEvt = RoomEvent({type: "room-connected", streams: streamList});
            that.dispatchEvent(connectEvt);
        }, function (error) {
            L.Logger.error("Not Connected! Error: " + error);
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
        if (that.socket !== undefined) {
            sendMessageSocket('unsubscribe', stream.streamID, function() {
                removeStream(stream);
            }, function() {
                L.Logger.error("Error calling unsubscribe.");
            });
        } else {
            callback();
        }

    };

    return that;
};