var controller = require('./webRtcController');
var express = require ('express');
var net = require('net');
var app = express.createServer();
var io = require('socket.io').listen(app);

io.set('log level', 1);

app.configure(function () {
    app.use(express.errorHandler({ dumpExceptions: true, showStack: true }));
    app.use(express.logger());
    app.use(express.static(__dirname + '/public'));

});

app.listen (3004);


var room = {};
room.sockets = [];
room.streams = [];
room.webRtcController = new controller.WebRtcController();


var sendMsgToRoom = function(type, arg) {

    var sockets = room.sockets; 
    for(var id in sockets) {
        console.log('Sending message to', sockets[id]);
        io.sockets.socket(sockets[id]).emit(type, arg);    
    }
       
};


io.sockets.on('connection', function (socket) {

    console.log("Socket connect ", socket.id);
    
    socket.on('initConnection', function (callback) {

        room.sockets.push(socket.id);

        callback('success', {streams: room.streams});     
    });

    socket.on('publish', function(state, sdp, callback) {

        if (state === 'offer' && socket.state === undefined) {
            room.webRtcController.addPublisher(socket.id, sdp, function (answer) {
                socket.state = 'waitingOk';
                callback(answer, socket.id);
            });

        } else if (state === 'ok' && socket.state === 'waitingOk') {
            socket.state = 'publishing';
            socket.stream = socket.id;
            room.streams.push(socket.stream);
            sendMsgToRoom('onAddStream', socket.stream);
        }
    });

    socket.on('subscribe', function(to, sdp, callback) {
        room.webRtcController.addSubscriber(socket.id, to, sdp, function (answer) {
            callback(answer);
        });
     });

    socket.on('unpublish', function() {

        sendMsgToRoom('onRemoveStream', socket.stream);
        var index = room.streams.indexOf(socket.id);
        if(index !== -1) {
            room.streams.splice(index, 1);
        }
        socket.state = undefined;
        socket.stream = undefined;
        room.webRtcController.removePublisher(socket.id);
    });

    socket.on('unsubscribe', function(to) {
        room.webRtcController.removeSubscriber(socket.id, to);
    });

    socket.on('disconnect', function () {

        console.log('Socket disconnect ', socket.id);
    
        if(socket.stream !== undefined) {
            sendMsgToRoom('onRemoveStream', socket.stream);
        } 

        if(room !== undefined) {
            var index = room.sockets.indexOf(socket.id);
            if(index !== -1) {
                room.sockets.splice(index, 1);
            }
            index = room.streams.indexOf(socket.id);
            if(index !== -1) {
                room.streams.splice(index, 1);
            }
            room.webRtcController.removeClient(socket.id);
        }       
    });
});


exports.getUsersInRoom = function(callback) {

    var users = [];
    if(room === undefined) {
        callback(users);
        return;
    }

    var sockets = room.sockets; 
    
    console.log('los sockets son: ', sockets);
    for(var id in sockets) {   
        users.push(io.sockets.socket(sockets[id]).user);   
    }
    callback(users);
}
