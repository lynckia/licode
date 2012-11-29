var controller = require('./../erizoController/webRtcController');
var express = require ('express');
var net = require('net');
var http = require('http');
var app = express();
var server = http.createServer(app);
var io = require('socket.io').listen(server);

io.set('log level', 1);

app.configure(function () {
    app.use(express.errorHandler({ dumpExceptions: true, showStack: true }));
    app.use(express.logger());
    app.use(express.static(__dirname + '/public'));

});

server.listen (3004);


var roomSt = {};
roomSt.sockets = [];
roomSt.streams = [];
roomSt.webRtcController = new controller.WebRtcController();

var roomVc = {};
roomVc.sockets = [];
roomVc.streams = [];
roomVc.webRtcController = new controller.WebRtcController();


var sendMsgToRoom = function(room, type, arg) {

    var sockets = room.sockets; 
    for(var id in sockets) {
        console.log('Sending message to', sockets[id]);
        io.sockets.socket(sockets[id]).emit(type, arg);    
    }
       
};


io.sockets.on('connection', function (socket) {

    console.log("Socket connect ", socket.id);

    socket.on('sdp', function (sdp) {
        console.log('lllllllllllllll', sdp);
    });
    
    socket.on('initConnection', function (room, callback) {

        if (room == 'streaming') {
            socket.room = roomSt;
        } else {
            socket.room = roomVc;    
        }
        socket.room.sockets.push(socket.id);
        callback('success', {streams: socket.room.streams}, socket.id);
             
    });

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

    socket.on('subscribe', function(to, sdp, callback) {
        socket.room.webRtcController.addSubscriber(socket.id, to, sdp, function (answer) {
            callback(answer);
        });
     });

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

    socket.on('unsubscribe', function(to) {
        socket.room.webRtcController.removeSubscriber(socket.id, to);
    });

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
