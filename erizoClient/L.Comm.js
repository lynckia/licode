var L = L || {};

L.Comm = function(L) {

	var socket;
	var server = 'rosendo.dit.upm.es:8080';

	var connect = function(token, callback, error) {
		
		L.Comm.socket = io.connect(server);
    	L.Comm.sendMessage('token', token, callback, error);
    	
	};

	var sendMessage = function(type, msg, callback, error) {

        L.Comm.socket.emit(type, msg, callback, error);

	};

	return {
		connect: connect,
		sendMessage: sendMessage
	};

}(L);