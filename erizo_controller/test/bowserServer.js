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

app.listen (3004);

var callerId;
var answererId;


io.sockets.on('connection', function (socket) {

    console.log("Socket connect ", socket.id);
    
    socket.on('initConnection', function (type) {



        if (type == 'caller') {
            callerId = socket.id;
        } else {
            answererId = socket.id;    
        }

        console.log('*************************', type);
             
    });

    socket.on('offer', function (mess) {
        console.log('offer', mess);
        io.sockets.socket(answererId).emit('signaling', mess);
    });

    socket.on('answer', function (mess) {
        console.log('answer', mess);
        io.sockets.socket(callerId).emit('signaling', mess);
    });


    
});
