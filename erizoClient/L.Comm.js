var L = L || {};
L.Comm = function(L) {
	var privateVar;
	var connect = function(token, callback, error) {
		L.Comm.privateVar = 0;
	};
	var sendMessage = function(type, msg, callback, error) {

	};
	return {
		connect: connect,
		sendMessage: sendMessage
	};
}(L);