var crypto = require('crypto');
var rpc = require('./rpc/rpcClient');
var controller = require('./webRtcController');
var io = require('socket.io').listen(8080);

io.set('log level', 1);


var nuveKey = 'grdp1l0';
var tokenS = 'eyJ0b2tlbklkIjoiNGZmZDQ2ODhlNDgwZDRjMzAyMDAwMDAxIiwiaG9zdCI6Imh0dHA6Ly9yb3NlbmRvLmRpdC51cG0uZXMiLCJzaWduYXR1cmUiOiJObU0xTXpVNVkySmtZVEk0WldVMk5EWTFOV1ppWXpJME5qVmhOMlJqWWpWaE5HUmtaRGt5TVE9PSJ9';


var nicknames = {};

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


rpc.connect(function() {

	io.sockets.on('connection', function (socket) {

		console.log("New Socket id ", socket.id);
		
		socket.on('token', function (token, callback, error) {          
			
			var tokenJ = JSON.parse(new Buffer(token.token, 'base64').toString('ascii'));
			var token;

			if(checkSignature(tokenJ, nuveKey)) {

				rpc.callRpc('deleteToken', tokenJ.tokenId, function(resp) {

					if (resp == 'error') {
						console.log('Token does not exist');
						error('Token does not exist');
					} else if (tokenJ.host == resp.host) {
						console.log('OK');
						token = resp;
						console.log(token);
						callback('Success');
					}
				});

			} else {
				error('Authentication error');
			}

		});


		socket.on('disconnect', function () {
	    
	  	});

	});

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

