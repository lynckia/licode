var io = require('socket.io');
var express = require ('express');
var net = require('net');
var controller = require('./webRtcController');

var app = express.createServer();
var io = io.listen(app);
var nicknames = {};

io.set('log level', 1);

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

		controller.addPublisher(from, msg, processResponse);
	} else {
		controller.addSubscriber(from, to, msg, processResponse);
	}
	
};



var sendMsgToAll = function (socket, msg) {
    socket.broadcast.emit('broadcast', msg);
};


app.configure(function () {
    app.use(express.errorHandler({ dumpExceptions: false, showStack: false }));
    app.use(express.logger());
    app.use(express.static(__dirname + '/public'));
    //app.set('views', __dirname + '/../views/');
    //disable layout
    //app.set("view options", {layout: false});
});


app.listen (3000);


io.sockets.on('connection', function (socket) {
//	socket.set('name', socket.handshake.session.twitterScreenName);
	console.log("New Socket id ", socket.id);

	socket.on('info', function (name, fn) {          
		console.log('I received a info request by ', socket.id);
		socket.get('name', function (err, name) {
			var message = {id: socket.id, name: name};
			fn(message);
		});
	});
	
	socket.on('nickname', function(name,fn){
		if (nicknames[name]){
			fn('already registered');
		}else{
			console.log('***********Nuevo cliente: ', name);
			nicknames[name] = socket.id;
			socket.nickname = name;
			fn('ready');
			//io.sockets.emit('nicknames', nicknames);
		}
	});
	
	socket.on('unicast', function (to, msg) {          
		//console.log('I received a unicast message to ', to, ' saying ', msg);
		processMess(socket.nickname, to, msg);
	});

	socket.on('broadcast', function (msg) {
		console.log('I received a multicast message from ', socket.nickname , ' id ', nicknames[socket.nickname], ' otroid ', socket.id ,' saying ', msg);
		sendMsgToAll(socket, msg);
	});

	socket.on('nicknames', function(fn) {
		fn(nicknames);
	})

	socket.on('disconnect', function () {
    	if (!socket.nickname) return;
    	delete nicknames[socket.nickname];
    	io.sockets.emit('nicknames', nicknames);
    	controller.deleteCheck(socket.nickname);
  	});

});

