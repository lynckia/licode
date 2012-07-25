var crypto = require('crypto');
var rpc = require('./rpc/rpc');
var controller = require('./webRtcController');
var io = require('socket.io').listen(8080);

io.set('log level', 1);

var nuveKey = 'claveNuve';

var rooms = {};

/*
 * Sends a massege of type 'type' to all sockets in a determined room.
 */
var sendMsgToRoom = function(room, type, arg) {

    var sockets = room.sockets; 
    for(var id in sockets) {
        console.log('Sending message to', sockets[id], 'in room ', room.id);
        io.sockets.socket(sockets[id]).emit(type, arg);    
    }
       
};


rpc.connect(function() {

    io.sockets.on('connection', function (socket) {

        console.log("Socket connect ", socket.id);
        
        // Gets 'token' messages on the socket. Checks the signature and ask nuve if it is valid. 
        // Then registers it in the room and callback to the client. 
        socket.on('token', function (token, callback) {

            var tokenDB;

            if(checkSignature(token, nuveKey)) {

                rpc.callRpc('deleteToken', token.tokenId, function(resp) {

                    if (resp == 'error') {
                        console.log('Token does not exist');
                        callback('error', 'Token does not exist');
                        socket.disconnect();

                    } else if (token.host == resp.host) {
                        console.log(resp);
                        tokenDB = resp;
                        if(rooms[tokenDB.room] === undefined) {
                            var room = {};
                            room.id = tokenDB.room;
                            room.sockets = [];
                            room.sockets.push(socket.id);
                            room.streams = [];
                            room.webRtcController = new controller.WebRtcController();
                            rooms[tokenDB.room] = room;
                        } else {
                            rooms[tokenDB.room].sockets.push(socket.id);
                        }
                        var user = {name: tokenDB.userName, role: tokenDB.role};
                        socket.room = rooms[tokenDB.room];
                        socket.user = user;
                        console.log('OK, Valid token');

                        callback('success', {streams: socket.room.streams, id: socket.room.id});
                    
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

        //Gets 'publish' messages on the socket in order to add new stream to the room.
        socket.on('publish', function(state, sdp, callback) {

            if (state === 'offer' && socket.state === undefined) {
                socket.room.webRtcController.addPublisher(socket.id, sdp, function (answer) {
                    socket.state = 'waitingOk';
                    callback(answer, socket.id);
                });

            } else if (state === 'ok' && socket.state === 'waitingOk') {
                socket.state = 'publishing';
                socket.stream = socket.id;
                socket.room.streams.push(socket.stream);
                sendMsgToRoom(socket.room, 'onAddStream', socket.stream);
            }
        });

        //Gets 'subscribe' messages on the socket in order to add new subscriber to a determined stream (to).
        socket.on('subscribe', function(to, sdp, callback) {
            socket.room.webRtcController.addSubscriber(socket.id, to, sdp, function (answer) {
                callback(answer);
            });
        });

        //Gets 'unpublish' messages on the socket in order to remove a stream from the room.
        socket.on('unpublish', function() {

            sendMsgToRoom(socket.room, 'onRemoveStream', socket.stream);
            var index = socket.room.streams.indexOf(socket.id);
            if(index !== -1) {
                socket.room.streams.splice(index, 1);
            }
            socket.state = undefined;
            socket.stream = undefined;
            socket.room.webRtcController.removePublisher(socket.id);
        });

        //Gets 'unsubscribe' messages on the socket in order to remove a subscriber from a determined stream (to).
        socket.on('unsubscribe', function(to) {
            socket.room.webRtcController.removeSubscriber(socket.id, to);
        });

        //When a client leaves the room erizoController removes its stream from the room if exists.  
        socket.on('disconnect', function () {

            console.log('Socket disconnect ', socket.id);
        
            if(socket.stream !== undefined) {
                sendMsgToRoom(socket.room, 'onRemoveStream', socket.stream);
            } 

            if(socket.room !== undefined) {
                var index = socket.room.sockets.indexOf(socket.id);
                if(index !== -1) {
                    socket.room.sockets.splice(index, 1);
                }
                index = socket.room.streams.indexOf(socket.id);
                if(index !== -1) {
                    socket.room.streams.splice(index, 1);
                }
                socket.room.webRtcController.removeClient(socket.id);
            }       
        });
    });
});

/*
 *Gets a list of users in a determined room.
 */
exports.getUsersInRoom = function(room, callback) {

    var users = [];
    if(rooms[room] === undefined) {
        callback(users);
        return;
    }

    var sockets = rooms[room].sockets; 
    
    console.log('para la sala ', room, 'los sockets son: ', sockets);
    for(var id in sockets) {   
        users.push(io.sockets.socket(sockets[id]).user);   
    }
    callback(users);
}

var checkSignature = function(token, key) {

    var calculatedSignature = calculateSignature(token, key);

    if(calculatedSignature != token.signature) {
        console.log('Auth fail. Invalid signature.');
        return false;
    } else {
        return true;
    }
}

var calculateSignature = function(token, key) {

    var toSign = token.tokenId + ',' + token.host;
    var signed = crypto.createHmac('sha1', key).update(toSign).digest('hex');
    return (new Buffer(signed)).toString('base64');
}

