var crypto = require('crypto');
var rpc = require('./rpc/rpcClient');
var controller = require('./webRtcController');
var io = require('socket.io').listen(8080);

io.set('log level', 1);

var nuveKey = 'grdp1l0';

var rooms = {};

var sendMsgToRoom = function(room, type, arg) {

    var sockets = room.sockets; 
    for(var id in sockets) {
        console.log('Sending message to ', sockets[id]);
        io.sockets.socket(sockets[id]).emit(type, arg);    
    }
       
};


rpc.connect(function() {

    io.sockets.on('connection', function (socket) {

        console.log("New Socket id ", socket.id);
        
        socket.on('token', function (token, callback) {

            var tokenDB;

            if(checkSignature(token, nuveKey)) {

                rpc.callRpc('deleteToken', token.tokenId, function(resp) {

                    if (resp == 'error') {
                        console.log('Token does not exist');
                        callback('error', 'Token does not exist');
                        socket.disconnect();

                    } else if (token.host == resp.host) {
                        tokenDB = resp;
                        if(rooms[tokenDB.room] === undefined) {
                            var room = {};
                            room.id = tokenDB.room;
                            room.sockets = [];
                            room.sockets.push(socket.id);
                            room.streams = [];
                            console.log('************* Creo webRTCController');
                            room.webRtcController = new controller.WebRtcController();
                            rooms[tokenDB.room] = room;
                        } else {
                            rooms[tokenDB.room].sockets.push(socket.id);
                        }
                        socket.room = rooms[tokenDB.room];
                        //console.log('OK, Valid token', tokenDB);

                        //enviar los streams de la sala!
                        callback('success', rooms[tokenDB.room].streams);
                    
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

        socket.on('publish', function(state, sdp, callback) {
            console.log('*****SDP: ', sdp);

            if (state === 'offer' && socket.state === undefined) {
                console.log('*****OFFER: ', sdp);
                socket.room.webRtcController.addPublisher(socket.id, sdp, function (answer) {
                    socket.state = 'waitingOk';
                    callback(answer, socket.id);
                });

            } else if (state === 'ok' && socket.state === 'waitingOk') {
                console.log('*****OK: ', sdp);
                socket.state = 'publishing';
                socket.stream = socket.id;
                socket.room.streams.push(socket.stream);
                sendMsgToRoom(socket.room, 'onAddStream', socket.stream);
            }
        });

        socket.on('subscribe', function(to, sdp, callback) {
            
            socket.room.webRtcController.addSubscriber(socket.id, to, sdp, function (answer) {
                callback(answer);
            });

           
         });

        socket.on('disconnect', function () {
            console.log('Socket disconnect');
            if(socket.room !== undefined) {
                var index = socket.room.sockets.indexOf(socket.id);
                if(index !== -1) {
                    socket.room.sockets.splice(index, 1);
                }
                index = socket.room.streams.indexOf(socket.id);
                if(index !== -1) {
                    socket.room.streams.splice(index, 1);
                }
                socket.room.webRtcController.deleteCheck(socket.id);
            }       
        });

    });

});



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

