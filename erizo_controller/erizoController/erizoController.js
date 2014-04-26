/*global require, logger. setInterval, clearInterval, Buffer, exports*/
var crypto = require('crypto');
var rpc = require('./rpc/rpc');
var controller = require('./webRtcController');
var ST = require('./Stream');
var http = require('http');
var server = http.createServer();
var io = require('socket.io').listen(server, {log:false});
var config = require('./../../licode_config');
var logger = require('./logger').logger;
var Permission = require('./permission');

server.listen(8080);

io.set('log level', 0);

var nuveKey = config.nuve.superserviceKey;

var WARNING_N_ROOMS = config.erizoController.warning_n_rooms;
var LIMIT_N_ROOMS = config.erizoController.limit_n_rooms;

var INTERVAL_TIME_KEEPALIVE = config.erizoController.interval_time_keepAlive;

var BINDED_INTERFACE_NAME = config.erizoController.networkInterface;

var myId;
var rooms = {};
var myState;

var calculateSignature = function (token, key) {
    "use strict";

    var toSign = token.tokenId + ',' + token.host,
        signed = crypto.createHmac('sha1', key).update(toSign).digest('hex');
    return (new Buffer(signed)).toString('base64');
};

var checkSignature = function (token, key) {
    "use strict";

    var calculatedSignature = calculateSignature(token, key);

    if (calculatedSignature !== token.signature) {
        logger.info('Auth fail. Invalid signature.');
        return false;
    } else {
        return true;
    }
};

/*
 * Sends a massege of type 'type' to all sockets in a determined room.
 */
var sendMsgToRoom = function (room, type, arg) {
    "use strict";

    var sockets = room.sockets,
        id;
    for (id in sockets) {
        if (sockets.hasOwnProperty(id)) {
            logger.info('Sending message to', sockets[id], 'in room ', room.id);
            io.sockets.socket(sockets[id]).emit(type, arg);
        }
    }
};

var privateRegexp;
var publicIP;

var addToCloudHandler = function (callback) {
    "use strict";

    var interfaces = require('os').networkInterfaces(),
        addresses = [],
        k,
        k2,
        address;


    for (k in interfaces) {
        if (interfaces.hasOwnProperty(k)) {
            for (k2 in interfaces[k]) {
                if (interfaces[k].hasOwnProperty(k2)) {
                    address = interfaces[k][k2];
                    if (address.family === 'IPv4' && !address.internal) {
                        if (k === BINDED_INTERFACE_NAME || !BINDED_INTERFACE_NAME) {
                            addresses.push(address.address);
                        }
                    }
                }
            }
        }
    }

    privateRegexp = new RegExp(addresses[0], 'g');
    
    if (config.erizoController.publicIP === '' || config.erizoController.publicIP === undefined){        
        publicIP = addresses[0];
    } else {
        publicIP = config.erizoController.publicIP;
    }

    var addECToCloudHandler = function(attempt) {
        if (attempt <= 0) {
            return;
        }

        rpc.callRpc('nuve', 'addNewErizoController', {cloudProvider: config.cloudProvider.name, ip: publicIP}, function (msg) {

            if (msg === 'timeout') {
                logger.info('CloudHandler does not respond');

                // We'll try it more!
                setTimeout(function() {
                    attempt = attempt - 1;
                    addECToCloudHandler(attempt);
                }, 3000);
                return;
            }
            if (msg == 'error') {
                logger.info('Error in communication with cloudProvider');
            }

            publicIP = msg.publicIP;
            myId = msg.id;
            myState = 2;

            var intervarId = setInterval(function () {

                rpc.callRpc('nuve', 'keepAlive', myId, function (result) {
                    if (result === 'whoareyou') {

                        // TODO: It should try to register again in Cloud Handler. But taking into account current rooms, users, ...
                        logger.info('I don`t exist in cloudHandler. I`m going to be killed');
                        clearInterval(intervarId);
                        rpc.callRpc('nuve', 'killMe', publicIP, function () {});
                    }
                });

            }, INTERVAL_TIME_KEEPALIVE);

            callback();

        });
    };
    addECToCloudHandler(5);
};

//*******************************************************************
//       When adding or removing rooms we use an algorithm to check the state
//       If there is a state change we send a message to cloudHandler
//
//       States:
//            0: Not available
//            1: Warning
//            2: Available
//*******************************************************************
var updateMyState = function () {
    "use strict";

    var nRooms = 0, newState, i, info;

    for (i in rooms) {
        if (rooms.hasOwnProperty(i)) {
            nRooms += 1;
        }
    }

    if (nRooms < WARNING_N_ROOMS) {
        newState = 2;
    } else if (nRooms > LIMIT_N_ROOMS) {
        newState = 0;
    } else {
        newState = 1;
    }

    if (newState === myState) {
        return;
    }

    myState = newState;

    info = {id: myId, state: myState};
    rpc.callRpc('nuve', 'setInfo', info, function () {});
};

var listen = function () {
    "use strict";

    io.sockets.on('connection', function (socket) {

        logger.info("Socket connect ", socket.id);

        // Gets 'token' messages on the socket. Checks the signature and ask nuve if it is valid.
        // Then registers it in the room and callback to the client.
        socket.on('token', function (token, callback) {

            var tokenDB, user, streamList = [], index;

            if (checkSignature(token, nuveKey)) {

                rpc.callRpc('nuve', 'deleteToken', token.tokenId, function (resp) {

                    if (resp === 'error') {
                        logger.info('Token does not exist');
                        callback('error', 'Token does not exist');
                        socket.disconnect();

                    } else if (resp === 'timeout') {
                        logger.info('Nuve does not respond');
                        callback('error', 'Nuve does not respond');
                        socket.disconnect();

                    } else if (token.host === resp.host) {
                        tokenDB = resp;
                        if (rooms[tokenDB.room] === undefined) {
                            var room = {};
                            room.id = tokenDB.room;
                            room.sockets = [];
                            room.sockets.push(socket.id);
                            room.streams = {}; //streamId: Stream
                            if (tokenDB.p2p) {
                                logger.info('Token of p2p room');
                                room.p2p = true;
                            } else {
                                room.webRtcController = new controller.WebRtcController();
                            }
                            rooms[tokenDB.room] = room;
                            updateMyState();
                        } else {
                            rooms[tokenDB.room].sockets.push(socket.id);
                        }
                        user = {name: tokenDB.userName, role: tokenDB.role};
                        socket.user = user;
                        var permissions = config.roles[tokenDB.role] || [];
                        socket.user.permissions = {};
                        for (var i in permissions) {
                            var permission = permissions[i];
                            socket.user.permissions[permission] = true;
                        }
                        socket.room = rooms[tokenDB.room];
                        socket.streams = []; //[list of streamIds]
                        socket.state = 'sleeping';

                        logger.info('OK, Valid token');

                        for (index in socket.room.streams) {
                            if (socket.room.streams.hasOwnProperty(index)) {
                                streamList.push(socket.room.streams[index].getPublicStream());
                            }
                        }

                        callback('success', {streams: streamList, 
                                            id: socket.room.id, 
                                            p2p: socket.room.p2p,
                                            defaultVideoBW: config.erizoController.defaultVideoBW,
                                            maxVideoBW: config.erizoController.maxVideoBW,
                                            stunServerUrl: config.erizoController.stunServerUrl,
                                            turnServer: config.erizoController.turnServer
                                            });

                    } else {
                        logger.info('Invalid host');
                        callback('error', 'Invalid host');
                        socket.disconnect();
                    }
                });

            } else {
                callback('error', 'Authentication error');
                socket.disconnect();
            }
        });

        //Gets 'sendDataStream' messages on the socket in order to write a message in a dataStream.
        socket.on('sendDataStream', function (msg) {
            var sockets = socket.room.streams[msg.id].getDataSubscribers(), id;
            for (id in sockets) {
                if (sockets.hasOwnProperty(id)) {
                    logger.info('Sending dataStream to', sockets[id], 'in stream ', msg.id, 'mensaje', msg.msg);
                    io.sockets.socket(sockets[id]).emit('onDataStream', msg);
                }
            }
        });

        //Gets 'publish' messages on the socket in order to add new stream to the room.
        socket.on('publish', function (options, sdp, callback) {
            var id, st;
            if (!socket.user.permissions[Permission.PUBLISH]) {
		callback('error', 'unauthorized');
                return;
            }
            if (options.state === 'url') {
                id = Math.random() * 1000000000000000000;
                socket.room.webRtcController.addExternalInput(id, sdp, function (result) {
                    if (result === 'success') {
                        st = new ST.Stream({id: id, audio: options.audio, video: options.video, data: options.data, attributes: options.attributes});
                        socket.streams.push(id);
                        socket.room.streams[id] = st;
                        callback(result, id);
                        sendMsgToRoom(socket.room, 'onAddStream', st.getPublicStream());
                    } else {
                        callback(result);
                    }
                });
            } else if (options.state !== 'data' && !socket.room.p2p) {
                if (options.state === 'offer' && socket.state === 'sleeping') {
                    id = Math.random() * 1000000000000000000;
                    socket.room.webRtcController.addPublisher(id, sdp, function (answer) {
                        socket.state = 'waitingOk';
                        answer = answer.replace(privateRegexp, publicIP);
                        callback(answer, id);
                    }, function() {
                        if (socket.room.streams[id] !== undefined) {
                            sendMsgToRoom(socket.room, 'onAddStream', socket.room.streams[id].getPublicStream());
                        }
                    });

                } else if (options.state === 'ok' && socket.state === 'waitingOk') {
                    st = new ST.Stream({id: options.streamId, audio: options.audio, video: options.video, data: options.data, screen: options.screen, attributes: options.attributes});
                    socket.state = 'sleeping';
                    socket.streams.push(options.streamId);
                    socket.room.streams[options.streamId] = st;
                }
            } else if (options.state === 'p2pSignaling') {
                io.sockets.socket(options.subsSocket).emit('onPublishP2P', {sdp: sdp, streamId: options.streamId}, function(answer) {
                    callback(answer);
                });
            } else {
                id = Math.random() * 1000000000000000000;
                st = new ST.Stream({id: id, socket: socket.id, audio: options.audio, video: options.video, data: options.data, screen: options.screen, attributes: options.attributes});
                socket.streams.push(id);
                socket.room.streams[id] = st;
                callback(undefined, id);
                sendMsgToRoom(socket.room, 'onAddStream', st.getPublicStream());

            }
        });

        //Gets 'subscribe' messages on the socket in order to add new subscriber to a determined stream (options.streamId).
        socket.on('subscribe', function (options, sdp, callback) {
            if (!socket.user.permissions[Permission.SUBSCRIBE]) {
                callback('error', 'unauthorized');
                return;
            }
            var stream = socket.room.streams[options.streamId];

            if (stream === undefined) {
                return;
            }

            if (stream.hasData() && options.data !== false) {
                stream.addDataSubscriber(socket.id);
            }

            if (stream.hasAudio() || stream.hasVideo() || stream.hasScreen()) {

                if (socket.room.p2p) {
                    var s = stream.getSocket();
                    io.sockets.socket(s).emit('onSubscribeP2P', {streamId: options.streamId, subsSocket: socket.id}, function(offer) {
                        callback(offer);
                    });

                } else {
                    socket.room.webRtcController.addSubscriber(socket.id, options.streamId, options.audio, options.video, sdp, function (answer) {
                        answer = answer.replace(privateRegexp, publicIP);
                        callback(answer);
                    });
                }
            } else {
                callback(undefined);
            }

        });

        //Gets 'startRecorder' messages
        socket.on('startRecorder', function (options) {
          if (!socket.user.permissions[Permission.RECORD]) {
              callback('error', 'unauthorized');
              return;
          }
          var streamId = options.to;
          var url = options.url;
          logger.info("erizoController.js: Starting recorder streamID " + streamId + " url " + url);
            if (socket.room.streams[streamId].hasAudio() || socket.room.streams[streamId].hasVideo() || socket.room.streams[streamId].hasScreen()) {
                socket.room.webRtcController.addExternalOutput(streamId, url);
                logger.info("erizoController.js: Recorder Started");
            }
        });

        socket.on('stopRecorder', function (options) {
          if (!socket.user.permissions[Permission.RECORD]) {
              callback('error', 'unauthorized');
              return;
          }
          logger.info("erizoController.js: Stoping recorder to streamId " + options.to + " url " + options.url);
          socket.room.webRtcController.removeExternalOutput(options.to, options.url);
        });

        //Gets 'unpublish' messages on the socket in order to remove a stream from the room.
        socket.on('unpublish', function (streamId) {
            if (!socket.user.permissions[Permission.PUBLISH]) {
                callback('error', 'unauthorized');
                return;
            }
            var i, index;

            sendMsgToRoom(socket.room, 'onRemoveStream', {id: streamId});

            if (socket.room.streams[streamId].hasAudio() || socket.room.streams[streamId].hasVideo() || socket.room.streams[streamId].hasScreen()) {
                socket.state = 'sleeping';
                if (!socket.room.p2p) {
                    socket.room.webRtcController.removePublisher(streamId);
                }
            }

            index = socket.streams.indexOf(streamId);
            if (index !== -1) {
                socket.streams.splice(index, 1);
            }
            if (socket.room.streams[streamId]) {
                delete socket.room.streams[streamId];
            }

        });

        //Gets 'unsubscribe' messages on the socket in order to remove a subscriber from a determined stream (to).
        socket.on('unsubscribe', function (to) {
            if (!socket.user.permissions[Permission.SUBSCRIBE]) {
                callback('error', 'unauthorized');
                return;
            }
            if (socket.room.streams[to] === undefined) {
                return;
            }

            socket.room.streams[to].removeDataSubscriber(socket.id);

            if (socket.room.streams[to].hasAudio() || socket.room.streams[to].hasVideo() || socket.room.streams[to].hasScreen()) {
                if (!socket.room.p2p) {
                    socket.room.webRtcController.removeSubscriber(socket.id, to);
                };
            }

        });

        //When a client leaves the room erizoController removes its streams from the room if exists.
        socket.on('disconnect', function () {
            var i, index, id;

            logger.info('Socket disconnect ', socket.id);

            for (i in socket.streams) {
                if (socket.streams.hasOwnProperty(i)) {
                    sendMsgToRoom(socket.room, 'onRemoveStream', {id: socket.streams[i]});
                }
            }

            if (socket.room !== undefined) {

                for (i in socket.room.streams) {
                    if (socket.room.streams.hasOwnProperty(i)) {
                        socket.room.streams[i].removeDataSubscriber(socket.id);
                    }
                }

                index = socket.room.sockets.indexOf(socket.id);
                if (index !== -1) {
                    socket.room.sockets.splice(index, 1);
                }

                if (socket.room.webRtcController) {
                    socket.room.webRtcController.removeSubscriptions(socket.id);
                }

                for (i in socket.streams) {
                    if (socket.streams.hasOwnProperty(i)) {
                        id = socket.streams[i];

                        if (socket.room.streams[id].hasAudio() || socket.room.streams[id].hasVideo() || socket.room.streams[id].hasScreen()) {
                            if (!socket.room.p2p) {
                                socket.room.webRtcController.removePublisher(id);
                            }

                        }

                        if (socket.room.streams[id]) {
                            delete socket.room.streams[id];
                        }
                    }
                }
            }

            if (socket.room !== undefined && socket.room.sockets.length === 0) {
                logger.info('Empty room ', socket.room.id, '. Deleting it');
                delete rooms[socket.room.id];
                updateMyState();
            }
        });
    });
};


/*
 *Gets a list of users in a determined room.
 */
exports.getUsersInRoom = function (room, callback) {
    "use strict";

    var users = [], sockets, id;
    if (rooms[room] === undefined) {
        callback(users);
        return;
    }

    sockets = rooms[room].sockets;

    for (id in sockets) {
        if (sockets.hasOwnProperty(id)) {
            users.push(io.sockets.socket(sockets[id]).user);
        }
    }

    callback(users);
};

/*
 * Delete a determined room.
 */
exports.deleteRoom = function (room, callback) {
    "use strict";

    var sockets, streams, id, j;

    logger.info('Deleting room ', room);

    if (rooms[room] === undefined) {
        callback('Success');
        return;
    }
    sockets = rooms[room].sockets;

    for (id in sockets) {
        if (sockets.hasOwnProperty(id)) {
            rooms[room].webRtcController.removeSubscriptions(sockets[id]);
        }
    }

    streams = rooms[room].streams;

    for (j in streams) {
        if (streams[j].hasAudio() || streams[j].hasVideo() || streams[j].hasScreen()) {
            if (!room.p2p) {
                rooms[room].webRtcController.removePublisher(j);
            }

        }
    }

    delete rooms[room];
    updateMyState();
    logger.info('Deleted room ', room, rooms);
    callback('Success');
};

rpc.connect(function () {
    "use strict";

    addToCloudHandler(function () {

        var rpcID = 'erizoController_' + myId;

        rpc.bind(rpcID, listen);

    });
});
