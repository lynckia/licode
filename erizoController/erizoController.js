var crypto = require('crypto');
var rpc = require('./rpc/rpcClient');
var controller = require('./webRtcController');
var io = require('socket.io').listen(8080);

io.set('log level', 1);

var nuveKey = 'grdp1l0';

var rooms = {};

var processResponse = function (from, to, msg){

    toid = nicknames[to];
    var send = {from: from, msg: msg};
    //console.log('Sending unicast to ' + io.sockets.socket(toid).id);
    io.sockets.socket(toid).emit('message', send);
    
    if(from == 'publisher') {   
        io.sockets.emit('nicknames', nicknames);
    }
}


var processMess = function (from, to, msg) {

    if (to == 'publisher'){

        webrtccontroller.addPublisher(from, msg, processResponse);
    } else {
        webrtccontroller.addSubscriber(from, to, msg, processResponse);
    }
    
};



var sendMsgToAll = function (socket, msg) {
    socket.broadcast.emit('broadcast', msg);
};


rpc.connect(function() {

    io.sockets.on('connection', function (socket) {

        console.log("New Socket id ", socket.id);
        
        socket.on('token', function (token, callback) {

            var tokenDB;

            callback('success', 'ok');
/*
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
                            rooms[tokenDB.room] = room;
                        } else {
                            rooms[tokenDB.room].sockets.push(socket.id);
                        }
                        socket.room = rooms[tokenDB.room];
                        console.log('OK, Valid token', tokenDB);
                        callback('success', 'Valid token');
                    
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
*/
        });

        socket.on('publish', function(state, sdp, callback) {

            if (state === 'offer' && socket.state === undefined) {
                
                var webrtccontroller = new controller.WebRTCController();
                webrtccontroller.addPublisher(socket.id, sdp, function (from, to, answer) {
                    console.log('********************envio callback');
                    socket.state = 'offer';
                    callback(answer);
                });

            } else if (state === 'ok' && socket.state === 'offer') {
                socket.state = 'ok';
                console.log('yeah');
                socket.stream = socket.id;
                sendMessageToRoom('');
                io.sockets.emit('onAddStream', socket.stream);
            }

            


        });

        socket.on('disconnect', function () {
            console.log('Socket disconnect');
            if(socket.room !== undefined) {
                var index = socket.room.sockets.indexOf(socket.id);
                if(index !== -1) {
                    socket.room.sockets.splice(index, 1);
                }
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

