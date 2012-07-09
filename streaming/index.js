var io = require('socket.io');
var express = require ('express');
var net = require('net');
var controller = require('./webRtcController');

var app = express.createServer();
var io = io.listen(app);
var nicknames = {};


var processResponse = function (from, to, data){
	sendMsgToPeer(from,to,data);
}


var sendMsgToPeer = function (from, to, msg) {
	toid = nicknames[to];
	if (to == 'publisher'|| toid =='publisher'){
		controller.processMsg(from, 'publisher', msg, processResponse);
		return;
	}
	if (to == 'subscriber'|| toid =='subscriber'){
		controller.processMsg(from, 'subscriber', msg, processResponse);
		return;
	}
    console.log("Sending unicast to " + io.sockets.socket(toid).id);
    io.sockets.socket(toid).emit('message', msg);
};



var sendMsgToAll = function (socket, msg) {
    socket.broadcast.emit('message', msg);
};


app.configure(function () {
    app.use(express.errorHandler({ dumpExceptions: true, showStack: true }));
    app.use(express.logger());
    app.use(express.static(__dirname + '/public'));
    //app.set('views', __dirname + '/../views/');
    //disable layout
    //app.set("view options", {layout: false});
});
/*
app.get('/', function(req, res){
	res.send('hello World');
});
*/

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
			nicknames[name] = socket.id;
			socket.nickname = name;
			fn('ready');
			io.sockets.emit('nicknames', nicknames);
		}
	});
	
	socket.on('unicast', function (to, msg) {          
		console.log('I received a unicast message to ', to, ' saying ', msg);
		sendMsgToPeer(socket.nickname, to, msg);
	});

	socket.on('broadcast', function (msg) {
		console.log('I received a multicast message from ', socket.nickname , ' id ', nicknames[socket.nickname], ' otroid ', socket.id ,' saying ', msg);
		sendMsgToAll(socket, msg);
	});
	socket.on('disconnect', function () {
    	if (!socket.nickname) return;
    	delete nicknames[socket.nickname];
    	socket.broadcast.emit('nicknames', nicknames);
    	controller.deleteCheck(socket.nickname);
  	});

});

