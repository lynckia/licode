/*global L, io*/
'use strict';
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
    var that = Erizo.EventDispatcher(spec),
        connectSocket,
        sendMessageSocket,
        sendSDPSocket,
        sendDataSocket,
        updateAttributes,
        removeStream,
        DISCONNECTED = 0,
        CONNECTING = 1,
        CONNECTED = 2;

    that.remoteStreams = {};
    that.localStreams = {};
    that.roomID = '';
    that.socket = {};
    that.state = DISCONNECTED;
    that.p2p = false;

    that.addEventListener('room-disconnected', function () {
        var index, stream, evt2;
        that.state = DISCONNECTED;

        // Remove all streams
        for (index in that.remoteStreams) {
            if (that.remoteStreams.hasOwnProperty(index)) {
                stream = that.remoteStreams[index];
                removeStream(stream);
                delete that.remoteStreams[index];
                if (stream && !stream.failed){
                    evt2 = Erizo.StreamEvent({type: 'stream-removed', stream: stream});
                    that.dispatchEvent(evt2);
                }
            }
        }
        that.remoteStreams = {};

        // Close Peer Connections
        for (index in that.localStreams) {
            if (that.localStreams.hasOwnProperty(index)) {
                stream = that.localStreams[index];
                if(that.p2p){
                    for(var i in stream.pc){
                        stream.pc[i].close();
                    }
                }else{
                    stream.pc.close();
                }
                delete that.localStreams[index];
            }
        }

        // Close socket
        try {
            that.socket.disconnect();
        } catch (error) {
            L.Logger.debug('Socket already disconnected');
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
            if (stream.pc) stream.pc.close();
            if (stream.local) {
                stream.stream.stop();
            }
            delete stream.stream;
        }
    };

    sendDataSocket = function (stream, msg) {
        if (stream.local) {
            sendMessageSocket('sendDataStream', {id: stream.getID(), msg: msg});
        } else {
            L.Logger.error('You can not send data through a remote stream');
        }
    };

    updateAttributes = function(stream, attrs) {
        if (stream.local) {
            stream.updateLocalAttributes(attrs);
            sendMessageSocket('updateStreamAttributes', {id: stream.getID(), attrs: attrs});
        } else {
            L.Logger.error('You can not update attributes in a remote stream');
        }
    };

    // It connects to the server through socket.io
    connectSocket = function (token, callback, error) {

        var createRemotePc = function (stream, peerSocket) {

            stream.pc = Erizo.Connection({callback: function (msg) {
                  sendSDPSocket('signaling_message', {streamId: stream.getID(),
                                                      peerSocket: peerSocket,
                                                      msg: msg});
              },
              iceServers: that.iceServers,
              maxAudioBW: spec.maxAudioBW,
              maxVideoBW: spec.maxVideoBW,
              limitMaxAudioBW:spec.maxAudioBW,
              limitMaxVideoBW: spec.maxVideoBW});

            stream.pc.onaddstream = function (evt) {
                // Draw on html
                L.Logger.info('Stream subscribed');
                stream.stream = evt.stream;
                var evt2 = Erizo.StreamEvent({type: 'stream-subscribed', stream: stream});
                that.dispatchEvent(evt2);
            };
        };

        // Once we have connected
        that.socket = io.connect(token.host, {reconnect: false,
                                              secure: token.secure,
                                              'force new connection': true,
                                              transports: ['websocket']});

        // We receive an event with a new stream in the room.
        // type can be "media" or "data"
        that.socket.on('onAddStream', function (arg) {
            var stream = Erizo.Stream({streamID: arg.id,
                                       local: false,
                                       audio: arg.audio,
                                       video: arg.video,
                                       data: arg.data,
                                       screen: arg.screen,
                                       attributes: arg.attributes}),
                evt;
            that.remoteStreams[arg.id] = stream;
            evt = Erizo.StreamEvent({type: 'stream-added', stream: stream});
            that.dispatchEvent(evt);
        });

        that.socket.on('signaling_message_erizo', function (arg) {
            var stream;
            if (arg.peerId) {
                stream = that.remoteStreams[arg.peerId];
            } else {
                stream = that.localStreams[arg.streamId];
            }

            if (stream && !stream.failed) {
                stream.pc.processSignalingMessage(arg.mess);
            }
        });

        that.socket.on('signaling_message_peer', function (arg) {

            var stream = that.localStreams[arg.streamId];

            if (stream && !stream.failed) {
                stream.pc[arg.peerSocket].processSignalingMessage(arg.msg);
            } else {
                stream = that.remoteStreams[arg.streamId];

                if (!stream.pc) {
                    createRemotePc(stream, arg.peerSocket);
                }
                stream.pc.processSignalingMessage(arg.msg);
            }
        });

        that.socket.on('publish_me', function (arg) {
            var myStream = that.localStreams[arg.streamId];

            if (myStream.pc === undefined) {
                myStream.pc = {};
            }

            myStream.pc[arg.peerSocket] = Erizo.Connection({callback: function (msg) {
                sendSDPSocket('signaling_message', {streamId: arg.streamId,
                                                    peerSocket: arg.peerSocket, msg: msg});
            }, audio: myStream.hasAudio(), video: myStream.hasVideo(),
            iceServers: that.iceServers});


            myStream.pc[arg.peerSocket].oniceconnectionstatechange = function (state) {
                if (state === 'failed') {
                    myStream.pc[arg.peerSocket].close();
                    delete myStream.pc[arg.peerSocket];
                }
            };

            myStream.pc[arg.peerSocket].addStream(myStream.stream);
            myStream.pc[arg.peerSocket].createOffer();
        });

        that.socket.on('onBandwidthAlert', function (arg){
            L.Logger.info('Bandwidth Alert on', arg.streamID, 'message',
                          arg.message,'BW:', arg.bandwidth);
            if(arg.streamID){
                var stream = that.remoteStreams[arg.streamID];
                if (stream && !stream.failed) {
                    var evt = Erizo.StreamEvent({type:'bandwidth-alert',
                                                 stream:stream,
                                                 msg:arg.message,
                                                 bandwidth: arg.bandwidth});
                    stream.dispatchEvent(evt);
                }

            }
        });

        // We receive an event of new data in one of the streams
        that.socket.on('onDataStream', function (arg) {
            var stream = that.remoteStreams[arg.id],
                evt = Erizo.StreamEvent({type: 'stream-data', msg: arg.msg, stream: stream});
            stream.dispatchEvent(evt);
        });

        // We receive an event of new data in one of the streams
        that.socket.on('onUpdateAttributeStream', function (arg) {
            var stream = that.remoteStreams[arg.id],
                evt = Erizo.StreamEvent({type: 'stream-attributes-update',
                                         attrs: arg.attrs,
                                         stream: stream});
            stream.updateLocalAttributes(arg.attrs);
            stream.dispatchEvent(evt);
        });

        // We receive an event of a stream removed from the room
        that.socket.on('onRemoveStream', function (arg) {
            var stream = that.localStreams[arg.id];
            if (stream && !stream.failed){
                stream.failed = true;
                L.Logger.warning('We received a removeStream from our own stream --' +
                                 ' probably erizoJS timed out');
                var disconnectEvt = Erizo.StreamEvent({type: 'stream-failed',
                        msg:'Publishing local stream failed because of an Erizo Error',
                        stream: stream});
                that.dispatchEvent(disconnectEvt);
                that.unpublish(stream);

                return;
            }
            stream = that.remoteStreams[arg.id];

            if (stream && stream.failed){
                L.Logger.debug('Received onRemoveStream for a stream ' +
                'that we already marked as failed ', arg.id);
                return;
            }else if (!stream){
                L.Logger.debug('Received a removeStream for', arg.id,
                               'and it has not been registered here, ignoring.');
                return;
            }
            delete that.remoteStreams[arg.id];
            removeStream(stream);
            var evt = Erizo.StreamEvent({type: 'stream-removed', stream: stream});
            that.dispatchEvent(evt);
        });

        // The socket has disconnected
        that.socket.on('disconnect', function () {
            L.Logger.info('Socket disconnected, lost connection to ErizoController');
            if (that.state !== DISCONNECTED) {
                L.Logger.error('Unexpected disconnection from ErizoController');
                var disconnectEvt = Erizo.RoomEvent({type: 'room-disconnected',
                                                     message: 'unexpected-disconnection'});
                that.dispatchEvent(disconnectEvt);
            }
        });

        that.socket.on('connection_failed', function(arg){
            var stream,
                disconnectEvt;
            if (arg.type === 'publish'){
                L.Logger.error('ICE Connection Failed on publishing stream',
                                arg.streamId, that.state);
                if (that.state !== DISCONNECTED ) {
                    if(arg.streamId){
                        stream = that.localStreams[arg.streamId];
                        if (stream && !stream.failed) {
                            stream.failed = true;
                            disconnectEvt = Erizo.StreamEvent({type: 'stream-failed',
                                    msg: 'Publishing local stream failed ICE Checks',
                                    stream: stream});
                            that.dispatchEvent(disconnectEvt);
                            that.unpublish(stream);
                        }
                    }
                }
            } else {
                L.Logger.error('ICE Connection Failed on subscribe stream', arg.streamId);
                if (that.state !== DISCONNECTED) {
                    if(arg.streamId){
                        stream = that.remoteStreams[arg.streamId];
                        if (stream && !stream.failed) {
                            stream.failed = true;
                            disconnectEvt = Erizo.StreamEvent({type: 'stream-failed',
                                msg: 'Subscriber failed ICE, cannot reach Licode for media',
                                stream: stream});
                            that.dispatchEvent(disconnectEvt);
                            that.unsubscribe(stream);
                        }
                    }
                }
            }
        });

        that.socket.on('error', function(e){
            L.Logger.error ('Cannot connect to erizo Controller');
            if (error) error('Cannot connect to ErizoController (socket.io error)',e);
        });

        // First message with the token
        sendMessageSocket('token', token, callback, error);
    };

    // Function to send a message to the server using socket.io
    sendMessageSocket = function (type, msg, callback, error) {
        that.socket.emit(type, msg, function (respType, msg) {
            if (respType === 'success') {
                    if (callback) callback(msg);
            } else if (respType === 'error'){
                if (error) error(msg);
            } else {
                if (callback) callback(respType, msg);
            }

        });
    };

    // It sends a SDP message to the server using socket.io
    sendSDPSocket = function (type, options, sdp, callback) {
        if (that.state !== DISCONNECTED){
            that.socket.emit(type, options, sdp, function (response, respCallback) {
                if (callback) callback(response, respCallback);
            });
        }else{
            L.Logger.warning('Trying to send a message over a disconnected Socket');
        }
    };

    // Public functions

    // It stablishes a connection to the room.
    // Once it is done it throws a RoomEvent("room-connected")
    that.connect = function () {
        var token = L.Base64.decodeBase64(spec.token);

        if (that.state !== DISCONNECTED) {
            L.Logger.warning('Room already connected');
        }

        // 1- Connect to Erizo-Controller
        that.state = CONNECTING;
        connectSocket(JSON.parse(token), function (response) {
            var index = 0, stream, streamList = [], streams, roomId, arg, connectEvt;
            streams = response.streams || [];
            that.p2p = response.p2p;
            roomId = response.id;
            that.iceServers = response.iceServers;
            that.state = CONNECTED;
            spec.defaultVideoBW = response.defaultVideoBW;
            spec.maxVideoBW = response.maxVideoBW;

            // 2- Retrieve list of streams
            for (index in streams) {
                if (streams.hasOwnProperty(index)) {
                    arg = streams[index];
                    stream = Erizo.Stream({streamID: arg.id,
                                           local: false,
                                           audio: arg.audio,
                                           video: arg.video,
                                           data: arg.data,
                                           screen: arg.screen,
                                           attributes: arg.attributes});
                    streamList.push(stream);
                    that.remoteStreams[arg.id] = stream;
                }
            }

            // 3 - Update RoomID
            that.roomID = roomId;

            L.Logger.info('Connected to room ' + that.roomID);

            connectEvt = Erizo.RoomEvent({type: 'room-connected', streams: streamList});
            that.dispatchEvent(connectEvt);
        }, function (error) {
            L.Logger.error('Not Connected! Error: ' + error);
            var connectEvt = Erizo.RoomEvent({type: 'room-error', message:error});
            that.dispatchEvent(connectEvt);
        });
    };

    // It disconnects from the room, dispatching a new RoomEvent("room-disconnected")
    that.disconnect = function () {
        L.Logger.debug('Disconnection requested');
        // 1- Disconnect from room
        var disconnectEvt = Erizo.RoomEvent({type: 'room-disconnected',
                                             message: 'expected-disconnection'});
        that.dispatchEvent(disconnectEvt);
    };

    // It publishes the stream provided as argument. Once it is added it throws a
    // StreamEvent("stream-added").
    that.publish = function (stream, options, callback) {
        options = options || {};

        options.maxVideoBW = options.maxVideoBW || spec.defaultVideoBW;
        if (options.maxVideoBW > spec.maxVideoBW) {
            options.maxVideoBW = spec.maxVideoBW;
        }

        if (options.minVideoBW === undefined){
            options.minVideoBW = 0;
        }

        if (options.minVideoBW > spec.defaultVideoBW){
            options.minVideoBW = spec.defaultVideoBW;
        }

        // 1- If the stream is not local we do nothing.
        if (stream && stream.local && that.localStreams[stream.getID()] === undefined) {

            // 2- Publish Media Stream to Erizo-Controller
            if (stream.hasAudio() || stream.hasVideo() || stream.hasScreen()) {
                if (stream.url !== undefined || stream.recording !== undefined) {
                    var type;
                    var arg;
                    if (stream.url) {
                        type = 'url';
                        arg = stream.url;
                    } else {
                        type = 'recording';
                        arg = stream.recording;
                    }
                    L.Logger.info('Checking publish options for', stream.getID());
                    stream.checkOptions(options);
                    sendSDPSocket('publish', {state: type,
                                              data: stream.hasData(),
                                              audio: stream.hasAudio(),
                                              video: stream.hasVideo(),
                                              attributes: stream.getAttributes(),
                                              metadata: options.metadata,
                                              createOffer: options.createOffer},
                                  arg, function (id, error) {

                            if (id !== null) {
                                L.Logger.info('Stream published');
                                stream.getID = function () {
                                    return id;
                                };
                                stream.sendData = function (msg) {
                                    sendDataSocket(stream, msg);
                                };
                                stream.setAttributes = function (attrs) {
                                    updateAttributes(stream, attrs);
                                };
                                that.localStreams[id] = stream;
                                stream.room = that;
                                if (callback)
                                    callback(id);
                            } else {
                                L.Logger.error('Error when publishing stream', error);
                                // Unauth -1052488119
                                // Network -5
                                if (callback)
                                    callback(undefined, error);
                            }
                        });

                } else if (that.p2p) {
                    // We save them now to be used when actually publishing in P2P mode.
                    spec.maxAudioBW = options.maxAudioBW;
                    spec.maxVideoBW = options.maxVideoBW;
                    sendSDPSocket('publish', {state: 'p2p',
                                              data: stream.hasData(),
                                              audio: stream.hasAudio(),
                                              video: stream.hasVideo(),
                                              screen: stream.hasScreen(),
                                              metadata: options.metadata,
                                              attributes: stream.getAttributes()},
                                  undefined, function (id, error) {
                        if (id === null) {
                            L.Logger.error('Error when publishing the stream', error);
                            if (callback)
                                callback(undefined, error);
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
                        stream.setAttributes = function (attrs) {
                            updateAttributes(stream, attrs);
                        };

                        that.localStreams[id] = stream;
                        stream.room = that;
                    });

                } else {
                    L.Logger.info('Publishing to Erizo Normally, is createOffer',
                                  options.createOffer);
                    sendSDPSocket('publish', {state: 'erizo',
                                              data: stream.hasData(),
                                              audio: stream.hasAudio(),
                                              video: stream.hasVideo(),
                                              screen: stream.hasScreen(),
                                              minVideoBW: options.minVideoBW,
                                              attributes: stream.getAttributes(),
                                              createOffer: options.createOffer,
                                              metadata: options.metadata,
                                              scheme: options.scheme},
                                  undefined, function (id, error) {

                            if (id === null) {
                                L.Logger.error('Error when publishing the stream: ', error);
                                if (callback) callback(undefined, error);
                                return;
                            }

                            L.Logger.info('Stream assigned an Id, starting the publish process');
                            stream.getID = function () {
                                return id;
                            };
                            if (stream.hasData()) {
                                stream.sendData = function (msg) {
                                    sendDataSocket(stream, msg);
                                };
                            }
                            stream.setAttributes = function (attrs) {
                                updateAttributes(stream, attrs);
                            };
                            that.localStreams[id] = stream;
                            stream.room = that;

                            stream.pc = Erizo.Connection({callback: function (message) {
                                L.Logger.debug('Sending message', message);
                                sendSDPSocket('signaling_message', {streamId: stream.getID(),
                                                                    msg: message},
                                              undefined, function () {});
                            }, iceServers: that.iceServers,
                               maxAudioBW: options.maxAudioBW,
                               maxVideoBW: options.maxVideoBW,
                               limitMaxAudioBW: spec.maxAudioBW,
                               limitMaxVideoBW: spec.maxVideoBW,
                               audio: stream.hasAudio(),
                               video: stream.hasVideo()});

                            stream.pc.addStream(stream.stream);
                            stream.pc.oniceconnectionstatechange = function (state) {
                                // No one is notifying the other subscribers that this is a failure
                                // they will only receive onRemoveStream
                                if (state === 'failed') {
                                    if (that.state !== DISCONNECTED && stream && !stream.failed) {
                                        stream.failed=true;
                                        L.Logger.warning('Publishing Stream',
                                                         stream.getID(),
                                                         'has failed after successful ICE checks');
                                        var disconnectEvt = Erizo.StreamEvent({
                                            type: 'stream-failed',
                                            msg:'Publishing stream failed after connection',
                                            stream:stream});
                                        that.dispatchEvent(disconnectEvt);
                                        that.unpublish(stream);
                                    }
                                }
                            };
                            if(!options.createOffer)
                                stream.pc.createOffer();
                            if(callback) callback(id);
                        });
                }
            } else if (stream.hasData()) {
                // 3- Publish Data Stream
                sendSDPSocket('publish', {state: 'data',
                                          data: stream.hasData(),
                                          audio: false,
                                          video: false,
                                          screen: false,
                                          metadata: options.metadata,
                                          attributes: stream.getAttributes()},
                              undefined,
                              function (id, error) {
                    if (id === null) {
                        L.Logger.error('Error publishing stream ', error);
                        if (callback)
                            callback(undefined, error);
                        return;
                    }
                    L.Logger.info('Stream published');
                    stream.getID = function () {
                        return id;
                    };
                    stream.sendData = function (msg) {
                        sendDataSocket(stream, msg);
                    };
                    stream.setAttributes = function (attrs) {
                        updateAttributes(stream, attrs);
                    };
                    that.localStreams[id] = stream;
                    stream.room = that;
                    if(callback) callback(id);
                });
            }
        } else {
            L.Logger.error('Trying to publish invalid stream');
            if(callback) callback(undefined, 'Invalid Stream');
        }
    };

    // Returns callback(id, error)
    that.startRecording = function (stream, callback) {
        if (stream){
            L.Logger.debug('Start Recording stream: ' + stream.getID());
            sendMessageSocket('startRecorder', {to: stream.getID()}, function(id, error){
                if (id === null){
                    L.Logger.error('Error on start recording', error);
                    if (callback) callback(undefined, error);
                    return;
                }

                L.Logger.info('Start recording', id);
                if (callback) callback(id);
            });
        }else{
            L.Logger.error('Trying to start recording on an invalid stream', stream);
            if(callback) callback(undefined, 'Invalid Stream');
        }
    };

    // Returns callback(id, error)
    that.stopRecording = function (recordingId, callback) {
        sendMessageSocket('stopRecorder', {id: recordingId}, function(result, error){
            if (result === null){
                L.Logger.error('Error on stop recording', error);
                if (callback) callback(undefined, error);
                return;
            }
            L.Logger.info('Stop recording', recordingId);
            if (callback) callback(true);
        });
    };

    // It unpublishes the local stream in the room, dispatching a StreamEvent("stream-removed")
    that.unpublish = function (stream, callback) {

        // Unpublish stream from Erizo-Controller
        if (stream && stream.local) {
            // Media stream
            sendMessageSocket('unpublish', stream.getID(), function(result, error){
                if (result === null){
                    L.Logger.error('Error unpublishing stream', error);
                    if (callback) callback(undefined, error);
                    return;
                }

                L.Logger.info('Stream unpublished');
                if (callback) callback(true);


            });
            var p2p = stream.room && stream.room.p2p;
            stream.room = undefined;
            if ((stream.hasAudio() ||
                 stream.hasVideo() ||
                 stream.hasScreen()) &&
               stream.url === undefined) {
                if(!p2p){
                    if (stream.pc) stream.pc.close();
                    stream.pc = undefined;
                }else{
                    for (var index in stream.pc){
                        stream.pc[index].close();
                        stream.pc[index] = undefined;
                    }
                }
            }
            delete that.localStreams[stream.getID()];

            stream.getID = function () {};
            stream.sendData = function () {};
            stream.setAttributes = function () {};

        } else {
            var error = 'Cannot unpublish, stream does not exist or is not local';
            L.Logger.error();
            if (callback) callback(undefined, error);
            return;
        }
    };

    // It subscribe to a remote stream and draws it inside the HTML tag given by the ID='elementID'
    that.subscribe = function (stream, options, callback) {

        options = options || {};

        if (stream && !stream.local) {

            if (stream.hasVideo() || stream.hasAudio() || stream.hasScreen()) {
                // 1- Subscribe to Stream

                if (that.p2p) {
                    sendSDPSocket('subscribe', {streamId: stream.getID(),
                                                metadata: options.metadata});
                    if(callback) callback(true);
                } else {
                    L.Logger.info('Checking subscribe options for', stream.getID());
                    stream.checkOptions(options);
                    sendSDPSocket('subscribe', {streamId: stream.getID(),
                                                audio: options.audio,
                                                video: options.video,
                                                data: options.data,
                                                browser: Erizo.getBrowser(),
                                                createOffer: options.createOffer,
                                                metadata: options.metadata,
                                                slideShowMode: options.slideShowMode},
                                  undefined, function (result, error) {
                            if (result === null) {
                                L.Logger.error('Error subscribing to stream ', error);
                                if (callback)
                                    callback(undefined, error);
                                return;
                            }

                            L.Logger.info('Subscriber added');

                            stream.pc = Erizo.Connection({callback: function (message) {
                                L.Logger.info('Sending message', message);
                                sendSDPSocket('signaling_message', {streamId: stream.getID(),
                                                                    msg: message,
                                                                    browser: stream.pc.browser},
                                              undefined, function () {});
                              },
                              nop2p: true,
                              audio: options.audio,
                              video: options.video,
                              iceServers: that.iceServers});

                            stream.pc.onaddstream = function (evt) {
                                // Draw on html
                                L.Logger.info('Stream subscribed');
                                stream.stream = evt.stream;
                                var evt2 = Erizo.StreamEvent({type: 'stream-subscribed',
                                                              stream: stream});
                                that.dispatchEvent(evt2);
                            };

                            stream.pc.oniceconnectionstatechange = function (state) {
                                if (state === 'failed') {
                                    if (that.state !== DISCONNECTED && stream &&!stream.failed) {
                                        stream.failed = true;
                                        L.Logger.warning('Subscribing stream',
                                                        stream.getID(),
                                                        'has failed after successful ICE checks');
                                        var disconnectEvt = Erizo.StreamEvent(
                                              {type: 'stream-failed',
                                               msg: 'Subscribing stream failed after connection',
                                               stream:stream });
                                        that.dispatchEvent(disconnectEvt);
                                        that.unsubscribe(stream);
                                    }
                                }
                            };

                            stream.pc.createOffer(true);
                            if(callback) callback(true);
                        });

                }
            } else if (stream.hasData() && options.data !== false) {
                sendSDPSocket('subscribe',
                              {streamId: stream.getID(),
                               data: options.data,
                               metadata: options.metadata},
                              undefined, function (result, error) {
                    if (result === null) {
                        L.Logger.error('Error subscribing to stream ', error);
                        if (callback)
                            callback(undefined, error);
                        return;
                    }
                    L.Logger.info('Stream subscribed');
                    var evt = Erizo.StreamEvent({type: 'stream-subscribed', stream: stream});
                    that.dispatchEvent(evt);
                    if(callback) callback(true);
                });
            } else {
                L.Logger.warning ('There\'s nothing to subscribe to');
                if (callback) callback(undefined, 'Nothing to subscribe to');
                return;
            }
            // Subscribe to stream stream
            L.Logger.info('Subscribing to: ' + stream.getID());
        }else{
            var error = 'Error on subscribe';
            if (!stream){
                L.Logger.warning('Cannot subscribe to invalid stream', stream);
                error = 'Invalid or undefined stream';
            }
            else if (stream.local){
                L.Logger.warning('Cannot subscribe to local stream, you should ' +
                                 'subscribe to the remote version of your local stream');
                error = 'Local copy of stream';
            }
            if (callback)
                callback(undefined, error);
            return;
        }
    };

    // It unsubscribes from the stream, removing the HTML element.
    that.unsubscribe = function (stream, callback) {
        // Unsubscribe from stream stream
        if (that.socket !== undefined) {
            if (stream && !stream.local) {
                sendMessageSocket('unsubscribe', stream.getID(), function (result, error) {
                    if (result === null) {
                        if (callback)
                            callback(undefined, error);
                        return;
                    }
                    removeStream(stream);
                    if (callback) callback(true);
                }, function () {
                    L.Logger.error('Error calling unsubscribe.');
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

                if (stream.getAttributes() !== undefined &&
                    stream.getAttributes()[name] !== undefined) {

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
