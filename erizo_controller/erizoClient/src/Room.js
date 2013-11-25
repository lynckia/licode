/*global L, io, console*/
/*
 * Class Room represents a Licode Room. It will handle the connection, local stream publication and
 * remote stream subscription.
 * Typical Room initialization would be:
 * var room = Erizo.Room({token:'213h8012hwduahd-321ueiwqewq'});
 * It also handles RoomEvents and StreamEvents. For example:
 * Event 'room-connected' points out that the user has been successfully connected to the room.
 * Event 'room-disconnected' shows that the user has been already disconnected.
 * Event 'stream-added' indicates that there is a new stream available in the room.
 * Event 'stream-removed' shows that a previous available stream has been removed from the room.
 */
var Erizo = Erizo || {};

Erizo.Room = function (spec) {
    "use strict";

    var that = Erizo.EventDispatcher(spec),
        connectSocket,
        sendMessageSocket,
        sendSDPSocket,
        sendDataSocket,
        removeStream,
        DISCONNECTED = 0,
        CONNECTING = 1,
        CONNECTED = 2,
        recordingUrl;

    that.remoteStreams = {};
    that.localStreams = {};
    that.roomID = '';
    that.socket = {};
    that.state = DISCONNECTED;
    that.p2p = false;

    that.addEventListener("room-disconnected", function (evt) {
        var index, stream, evt2;
        that.state = DISCONNECTED;

        // Remove all streams
        for (index in that.remoteStreams) {
            if (that.remoteStreams.hasOwnProperty(index)) {
                stream = that.remoteStreams[index];
                removeStream(stream);
                delete that.remoteStreams[index];
                evt2 = Erizo.StreamEvent({type: 'stream-removed', stream: stream});
                that.dispatchEvent(evt2);
            }
        }
        that.remoteStreams = {};

        // Close Peer Connections
        for (index in that.localStreams) {
            if (that.localStreams.hasOwnProperty(index)) {
                stream = that.localStreams[index];
                stream.pc.close();
                delete that.localStreams[index];
            }
        }

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
    removeStream = function (stream) {
        if (stream.stream !== undefined) {

            // Remove HTML element
            stream.hide();

            // Close PC stream
            stream.pc.close();
            if (stream.local) {
                stream.stream.stop();
            }
        }
    };

    sendDataSocket = function (stream, msg) {
        if (stream.local) {
            sendMessageSocket("sendDataStream", {id: stream.getID(), msg: msg});
        } else {
            L.Logger.error("You can not send data through a remote stream");
        }
    };

    // It connects to the server through socket.io
    connectSocket = function (token, callback, error) {
        // Once we have connected

        var host = 'http://' + token.host;

        delete io.sockets[host];

        that.socket = io.connect(token.host, {reconnect: false, secure: false});

        // We receive an event with a new stream in the room.
        // type can be "media" or "data"
        that.socket.on('onAddStream', function (arg) {
            var stream = Erizo.Stream({streamID: arg.id, local: false, audio: arg.audio, video: arg.video, data: arg.data, screen: arg.screen, attributes: arg.attributes}),
                evt;
            that.remoteStreams[arg.id] = stream;
            evt = Erizo.StreamEvent({type: 'stream-added', stream: stream});
            that.dispatchEvent(evt);
        });


        that.socket.on('onSubscribeP2P', function (arg) {
            var myStream = that.localStreams[arg.streamId];

            myStream.pc = Erizo.Connection({callback: function (offer) {
                sendSDPSocket('publish', {state: 'p2pSignaling', streamId: arg.streamId, subsSocket: arg.subsSocket}, offer, function (answer, id) {
                    if (answer === 'error') {
                        if (callbackError) callbackError(answer);
                    }
                    myStream.pc.onsignalingmessage = function (ok) {
                        myStream.pc.onsignalingmessage = function () {};
                    };

                    myStream.pc.processSignalingMessage(answer);
                });
            }, stunServerUrl: that.stunServerUrl, turnServer: that.turnServer});

            myStream.pc.addStream(myStream.stream);

        });

        that.socket.on('onPublishP2P', function (arg, callback) {
            var myStream = that.remoteStreams[arg.streamId];

            myStream.pc = Erizo.Connection({callback: function (offer) {}, stunServerUrl: that.stunServerUrl, turnServer: that.turnServer, maxAudioBW: spec.maxAudioBW, maxVideoBW: spec.maxVideoBW});

            myStream.pc.onsignalingmessage = function (answer) {
                myStream.pc.onsignalingmessage = function () {};
                callback(answer);
            };

            myStream.pc.processSignalingMessage(arg.sdp);

            myStream.pc.onaddstream = function (evt) {
                // Draw on html
                L.Logger.info('Stream subscribed');
                myStream.stream = evt.stream;
                var evt2 = Erizo.StreamEvent({type: 'stream-subscribed', stream: myStream});
                that.dispatchEvent(evt2);
            };

        });

        // We receive an event of new data in one of the streams
        that.socket.on('onDataStream', function (arg) {
            var stream = that.remoteStreams[arg.id],
                evt = Erizo.StreamEvent({type: 'stream-data', msg: arg.msg, stream: stream});
            stream.dispatchEvent(evt);
        });

        // We receive an event of a stream removed from the room
        that.socket.on('onRemoveStream', function (arg) {
            var stream = that.remoteStreams[arg.id],
                evt;
            delete that.remoteStreams[arg.id];
            removeStream(stream);
            evt = Erizo.StreamEvent({type: 'stream-removed', stream: stream});
            that.dispatchEvent(evt);
        });

        // The socket has disconnected
        that.socket.on('disconnect', function (argument) {
            L.Logger.info("Socket disconnected");
            if (that.state !== DISCONNECTED) {
                var disconnectEvt = Erizo.RoomEvent({type: "room-disconnected"});
                that.dispatchEvent(disconnectEvt);
            }
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
                if (error !== undefined) {
                    error(msg);
                }
            }
        });
    };

    // It sends a SDP message to the server using socket.io
    sendSDPSocket = function (type, options, sdp, callback) {
        that.socket.emit(type, options, sdp, function (response, respCallback) {
            if (callback !== undefined) {
                callback(response, respCallback);
            }
        });
    };

    // Public functions

    // It stablishes a connection to the room. Once it is done it throws a RoomEvent("room-connected")
    that.connect = function () {
        var streamList = [],
            token = L.Base64.decodeBase64(spec.token);

        if (that.state !== DISCONNECTED) {
            L.Logger.error("Room already connected");
        }

        // 1- Connect to Erizo-Controller
        that.state = CONNECTING;
        connectSocket(JSON.parse(token), function (response) {
            var index = 0, stream, streamList = [], streams, roomId, arg, connectEvt;
            streams = response.streams;
            that.p2p = response.p2p;
            roomId = response.id;
            that.stunServerUrl = response.stunServerUrl;
            that.turnServer = response.turnServer;
            that.state = CONNECTED;
            spec.defaultVideoBW = response.defaultVideoBW;
            spec.maxVideoBW = response.maxVideoBW;

            // 2- Retrieve list of streams
            for (index in streams) {
                if (streams.hasOwnProperty(index)) {
                    arg = streams[index];
                    stream = Erizo.Stream({streamID: arg.id, local: false, audio: arg.audio, video: arg.video, data: arg.data, screen: arg.screen, attributes: arg.attributes});
                    streamList.push(stream);
                    that.remoteStreams[arg.id] = stream;
                }
            }

            // 3 - Update RoomID
            that.roomID = roomId;

            L.Logger.info("Connected to room " + that.roomID);

            connectEvt = Erizo.RoomEvent({type: "room-connected", streams: streamList});
            that.dispatchEvent(connectEvt);
        }, function (error) {
            L.Logger.error("Not Connected! Error: " + error);
        });
    };

    // It disconnects from the room, dispatching a new RoomEvent("room-disconnected")
    that.disconnect = function () {
        // 1- Disconnect from room
        var disconnectEvt = Erizo.RoomEvent({type: "room-disconnected"});
        that.dispatchEvent(disconnectEvt);
    };

    // It publishes the stream provided as argument. Once it is added it throws a
    // StreamEvent("stream-added").
    that.publish = function (stream, options, callback, callbackError) {
        options = options || {};

        var maxVideoBW;
        options.maxVideoBW = options.maxVideoBW || spec.defaultVideoBW;
        if (options.maxVideoBW > spec.maxVideoBW) {
            options.maxVideoBW = spec.maxVideoBW;
        }

        // 1- If the stream is not local we do nothing.
        if (stream.local && that.localStreams[stream.getID()] === undefined) {

            // 2- Publish Media Stream to Erizo-Controller
            if (stream.hasAudio() || stream.hasVideo() || stream.hasScreen()) {
                if (stream.url !== undefined) {
                    sendSDPSocket('publish', {state: 'url', data: stream.hasData(), audio: stream.hasAudio(), video: stream.hasVideo(), attributes: stream.getAttributes()}, stream.url, function (answer, id) {

                        if (answer === 'success') {
                            L.Logger.info('Stream published');
                            stream.getID = function () {
                                return id;
                            };
                            stream.sendData = function (msg) {
                                sendDataSocket(stream, msg);
                            };
                            that.localStreams[id] = stream;
                            stream.room = that;
                            if (callback)
                                callback();
                        } else {
                            L.Logger.info('Error when publishing the stream', answer);
                            // Unauth -1052488119
                            // Network -5
                            if (callbackError)
                                callbackError(answer);
                        }
                    });

                } else if (that.p2p) {
                    // We save them now to be used when actually publishing in P2P mode.
                    spec.maxAudioBW = options.maxAudioBW;
                    spec.maxVideoBW = options.maxVideoBW;
                    sendSDPSocket('publish', {state: 'p2p', data: stream.hasData(), audio: stream.hasAudio(), video: stream.hasVideo(), screen: stream.hasScreen(), attributes: stream.getAttributes()}, undefined, function (answer, id) {
                        if (answer === 'error') {
                            if (callbackError)
                                callbackError(answer);
                        }
                        L.Logger.info('Stream published');
                        stream.getID = function () {
                            return id;
                        };
                        if (stream.hasData()) {
                            stream.sendData = function (msg) {
                                sendDataSocket(stream, msg);
                            };
                        }
                        that.localStreams[id] = stream;
                        stream.room = that;
                    });

                } else {

                    stream.pc = Erizo.Connection({callback: function (offer) {
                        sendSDPSocket('publish', {state: 'offer', data: stream.hasData(), audio: stream.hasAudio(), video: stream.hasVideo(), attributes: stream.getAttributes()}, offer, function (answer, id) {
                            if (answer === 'error') {
                                if (callbackError)
                                    callbackError(answer);
                                return;
                            }
                            stream.pc.onsignalingmessage = function (ok) {
                                stream.pc.onsignalingmessage = function () {};
                                sendSDPSocket('publish', {state: 'ok', streamId: id, data: stream.hasData(), audio: stream.hasAudio(), video: stream.hasVideo(), screen: stream.hasScreen(), attributes: stream.getAttributes()}, ok);
                                L.Logger.info('Stream published');
                                stream.getID = function () {
                                    return id;
                                };
                                if (stream.hasData()) {
                                    stream.sendData = function (msg) {
                                        sendDataSocket(stream, msg);
                                    };
                                }
                                that.localStreams[id] = stream;
                                stream.room = that;
                            };
                            stream.pc.processSignalingMessage(answer);
                        });
                    }, stunServerUrl: that.stunServerUrl, turnServer: that.turnServer, maxAudioBW: options.maxAudioBW, maxVideoBW: options.maxVideoBW});

                    stream.pc.addStream(stream.stream);
                }
            } else if (stream.hasData()) {
                // 3- Publish Data Stream
                sendSDPSocket('publish', {state: 'data', data: stream.hasData(), audio: false, video: false, screen: false, attributes: stream.getAttributes()}, undefined, function (answer, id) {
                    if (answer === 'error') {
                        if (callbackError)
                            callbackError(answer);
                        return;
                    }
                    L.Logger.info('Stream published');
                    stream.getID = function () {
                        return id;
                    };
                    stream.sendData = function (msg) {
                        sendDataSocket(stream, msg);
                    };
                    that.localStreams[id] = stream;
                    stream.room = that;
                });
            }
        }
    };

    that.startRecording = function (stream){
      recordingUrl = "/tmp/recording" + stream.getID() + ".mkv";
      L.Logger.debug("Start Recording " + recordingUrl);
      sendMessageSocket('startRecorder',{to:stream.getID(), url: recordingUrl});
    }

    that.stopRecording = function (stream){
      sendMessageSocket('stopRecorder',{to:stream.getID(),url:recordingUrl});
    }

    // It unpublishes the local stream in the room, dispatching a StreamEvent("stream-removed")
    that.unpublish = function (stream) {

        // Unpublish stream from Erizo-Controller
        if (stream.local) {
            // Media stream
            sendMessageSocket('unpublish', stream.getID());
            stream.room = undefined;
            if ((stream.hasAudio() || stream.hasVideo() || stream.hasScreen()) && stream.url === undefined) {
                stream.pc.close();
                stream.pc = undefined;
            }
            delete that.localStreams[stream.getID()];

            stream.getID = function () {};
            stream.sendData = function (msg) {};
        }
    };

    // It subscribe to a remote stream and draws it inside the HTML tag given by the ID='elementID'
    that.subscribe = function (stream, options, callbackError) {

        options = options || {};

        if (!stream.local) {

            if (stream.hasVideo() || stream.hasAudio() || stream.hasScreen()) {
                // 1- Subscribe to Stream

                if (that.p2p) {
                    sendSDPSocket('subscribe', {streamId: stream.getID()});
                } else {
                    stream.pc = Erizo.Connection({callback: function (offer) {
                        sendSDPSocket('subscribe', {streamId: stream.getID(), audio: options.audio, video: options.video, data: options.data}, offer, function (answer) {
                            if (answer === 'error') {
                                if (callbackError)
                                    callbackError(answer);
                                return;
                            }
                            // For compatibility with only audio in Firefox
                            if (answer.match(/a=sendrecv\\r\\na=mid:video/)) {
                                answer = answer.replace(/a=sendrecv\\r\\na=mid:video/, 'a=recvonly\\r\\na=mid:video');
                                answer = answer.split('a=ssrc:55543')[0] + '"}';
                            }
                            
                            stream.pc.processSignalingMessage(answer);
                        });
                    }, audio: stream.hasAudio(), video: stream.hasVideo(), videostunServerUrl: that.stunServerUrl, turnServer: that.turnServer});

                    stream.pc.onaddstream = function (evt) {
                        // Draw on html
                        L.Logger.info('Stream subscribed');
                        stream.stream = evt.stream;
                        var evt2 = Erizo.StreamEvent({type: 'stream-subscribed', stream: stream});
                        that.dispatchEvent(evt2);
                    };
                }
            } else if (stream.hasData() && options.data !== false) {
                sendSDPSocket('subscribe', {streamId: stream.getID(), data: options.data}, undefined, function (answer) {
                    if (answer === 'error') {
                        if (callbackError)
                            callbackError(answer);
                        return;
                    }
                    L.Logger.info('Stream subscribed');
                    var evt = Erizo.StreamEvent({type: 'stream-subscribed', stream: stream});
                    that.dispatchEvent(evt);
                });
            } else {
                L.Logger.info("Subscribing to anything");
                return;
            }

            // Subscribe to stream stream
            L.Logger.info("Subscribing to: " + stream.getID());
        }
    };

    // It unsubscribes from the stream, removing the HTML element.
    that.unsubscribe = function (stream, callbackError) {

        // Unsubscribe from stream stream
        if (that.socket !== undefined) {
            if (!stream.local) {
                sendMessageSocket('unsubscribe', stream.getID(), function () {
                    if (answer === 'error') {
                        if (callbackError)
                            callbackError(answer);
                        return;
                    }
                    removeStream(stream);
                }, function () {
                    L.Logger.error("Error calling unsubscribe.");
                });
            }
        }
    };

    //It searchs the streams that have "name" attribute with "value" value
    that.getStreamsByAttribute = function (name, value) {

        var streams = [], index, stream;

        for (index in that.remoteStreams) {
            if (that.remoteStreams.hasOwnProperty(index)) {
                stream = that.remoteStreams[index];

                if (stream.getAttributes() !== undefined && stream.getAttributes()[name] !== undefined) {
                    if (stream.getAttributes()[name] === value) {
                        streams.push(stream);
                    }
                }
            }
        }

        return streams;
    };

    return that;
};
