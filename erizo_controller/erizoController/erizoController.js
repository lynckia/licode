/*global require, console, setInterval, clearInterval, Buffer, exports*/
var crypto = require('crypto');
var rpc = require('./rpc/rpc');
var controller = require('./webRtcController');
var ST = require('./Stream');
var io = require('socket.io').listen(8080);
var config = require('./../../lynckia_config');

io.set('log level', 1);

var nuveKey = config.nuve.superserviceKey;

var WARNING_N_ROOMS = config.erizoController.warning_n_rooms;
var LIMIT_N_ROOMS = config.erizoController.limit_n_rooms;

var INTERVAL_TIME_KEEPALIVE = config.erizoController.interval_time_keepAlive;

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
        console.log('Auth fail. Invalid signature.');
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
            console.log('Sending message to', sockets[id], 'in room ', room.id);
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
                        addresses.push(address.address);
                    }
                }
            }
        }
    }

    publicIP = addresses[0];
    privateRegexp = new RegExp(publicIP, 'g');

    rpc.callRpc('nuve', 'addNewErizoController', {cloudProvider: config.cloudProvider.name, ip: publicIP}, function (msg) {

        if (msg === 'timeout') {
            console.log('CloudHandler does not respond');
            return;
        }
        if (msg == 'error') {
            console.log('Error in communication with cloudProvider');
        }

        publicIP = msg.publicIP;
        myId = msg.id;
        myState = 2;

        var intervarId = setInterval(function () {

            rpc.callRpc('nuve', 'keepAlive', myId, function (result) {
                if (result === 'whoareyou') {
                    console.log('I don`t exist in cloudHandler. I`m going to be killed');
                    clearInterval(intervarId);
                    rpc.callRpc('nuve', 'killMe', publicIP, function () {});
                }
            });

        }, INTERVAL_TIME_KEEPALIVE);

        callback();

    });
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

        console.log("Socket connect ", socket.id);

        // Gets 'token' messages on the socket. Checks the signature and ask nuve if it is valid. 
        // Then registers it in the room and callback to the client. 
        socket.on('token', function (token, callback) {

            var tokenDB, user, streamList = [], index;

            if (checkSignature(token, nuveKey)) {

                rpc.callRpc('nuve', 'deleteToken', token.tokenId, function (resp) {

                    if (resp === 'error') {
                        console.log('Token does not exist');
                        callback('error', 'Token does not exist');
                        socket.disconnect();

                    } else if (resp === 'timeout') {
                        console.log('Nuve does not respond');
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
                            room.webRtcController = new controller.WebRtcController();
                            rooms[tokenDB.room] = room;
                            updateMyState();
                        } else {
                            rooms[tokenDB.room].sockets.push(socket.id);
                        }
                        user = {name: tokenDB.userName, role: tokenDB.role};
                        socket.room = rooms[tokenDB.room];
                        socket.user = user;
                        socket.streams = []; //[list of streamIds]
                        socket.state = 'sleeping';

                        console.log('OK, Valid token');

                        for (index in socket.room.streams) {
                            if (socket.room.streams.hasOwnProperty(index)) {
                                streamList.push(socket.room.streams[index].getPublicStream());
                            }
                        }

                        callback('success', {streams: streamList, id: socket.room.id, stunServerUrl: config.erizoController.stunServerUrl});

                    } else {
                        console.log('Invalid host');
                        callback('error', 'Invalid host');
                        socket.disconnect();
                    }
                });

            } else {
                callback('error', 'Authentication error');
                socket.disconnect();
            }
        });

        //Gets 'ssendDataStream' messages on the socket in order to write a message in a dataStream.
        socket.on('sendDataStream', function (msg) {
            var sockets = socket.room.streams[msg.id].getDataSubscribers(), id;
            for (id in sockets) {
                if (sockets.hasOwnProperty(id)) {
                    console.log('Sending dataStream to', sockets[id], 'in stream ', msg.id, 'mensaje', msg.msg);
                    io.sockets.socket(sockets[id]).emit('onDataStream', msg);
                }
            }
        });

        //Gets 'publish' messages on the socket in order to add new stream to the room.
        socket.on('publish', function (options, sdp, callback) {
            var id, st;
            if (options.state !== 'data') {
                if (options.state === 'offer' && socket.state === 'sleeping') {
                    id = Math.random() * 100000000000000000;
                    socket.room.webRtcController.addPublisher(id, sdp, function (answer) {
                        socket.state = 'waitingOk';
                        answer = answer.replace(privateRegexp, publicIP);
                        callback(answer, id);
                    });

                } else if (options.state === 'ok' && socket.state === 'waitingOk') {
                    st = new ST.Stream({id: options.streamId, audio: options.audio, video: options.video, data: options.data, screen: options.screen, attributes: options.attributes});
                    socket.state = 'sleeping';
                    socket.streams.push(options.streamId);
                    socket.room.streams[options.streamId] = st;
                    sendMsgToRoom(socket.room, 'onAddStream', st.getPublicStream());
                }
            } else {
                id = Math.random() * 100000000000000000;
                st = new ST.Stream({id: id, audio: options.audio, video: options.video, data: options.data, screen: options.screen, attributes: options.attributes});
                socket.streams.push(id);
                socket.room.streams[id] = st;
                callback(undefined, id);
                sendMsgToRoom(socket.room, 'onAddStream', st.getPublicStream());
            }

        });

        //Gets 'subscribe' messages on the socket in order to add new subscriber to a determined stream (options.streamId).
        socket.on('subscribe', function (options, sdp, callback) {

            if (socket.room.streams[options.streamId] === undefined) {
                return;
            }

            socket.room.streams[options.streamId].addDataSubscriber(socket.id);

            if (socket.room.streams[options.streamId].hasAudio() || socket.room.streams[options.streamId].hasVideo()) {
                socket.room.webRtcController.addSubscriber(socket.id, options.streamId, sdp, function (answer) {
                    answer = answer.replace(privateRegexp, publicIP);
                    callback(answer);
                });
            } else {
                callback(undefined);
            }

        });

        //Gets 'unpublish' messages on the socket in order to remove a stream from the room.
        socket.on('unpublish', function (streamId) {
            var i, index;

            sendMsgToRoom(socket.room, 'onRemoveStream', {id: streamId});

            if (socket.room.streams[streamId].hasAudio() || socket.room.streams[streamId].hasVideo()) {
                socket.state = 'sleeping';
                socket.room.webRtcController.removePublisher(streamId);
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

            if (socket.room.streams[to] === undefined) {
                return;
            }

            socket.room.streams[to].removeDataSubscriber(socket.id);

            if (socket.room.streams[to].hasAudio() || socket.room.streams[to].hasVideo()) {
                socket.room.webRtcController.removeSubscriber(socket.id, to);
            }

        });

        //When a client leaves the room erizoController removes its streams from the room if exists.  
        socket.on('disconnect', function () {
            var i, index, id;

            console.log('Socket disconnect ', socket.id);

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

                for (i in socket.streams) {
                    if (socket.streams.hasOwnProperty(i)) {
                        id = socket.streams[i];

                        if (socket.room.streams[id].hasAudio() || socket.room.streams[id].hasVideo()) {
                            socket.room.webRtcController.removeClient(socket.id, id);
                        }

                        if (socket.room.streams[id]) {
                            delete socket.room.streams[id];
                        }
                    }
                }
            }

            if (socket.room !== undefined && socket.room.sockets.length === 0) {
                console.log('Empty room ', socket.room.id, '. Deleting it');
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

    var sockets, id;

    if (rooms[room] === undefined) {
        callback('Success');
        return;
    }
    sockets = rooms[room].sockets;

    for (id in sockets) {
        if (sockets.hasOwnProperty(id)) {
            rooms[room].webRtcController.removeClient(sockets[id]);
        }
    }
    console.log('Deleting room ', room, rooms);
    delete rooms[room];
    updateMyState();
    console.log('1 Deleting room ', room, rooms);
    callback('Success');
};

rpc.connect(function () {
    "use strict";

    addToCloudHandler(function () {

        var rpcID = 'erizoController_' + myId;

        rpc.bind(rpcID, listen);

    });
});
