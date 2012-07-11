var L = L || {};

L.Comm = function(L) {

    var socket;

    var connect = function(token, callback, error) {
        L.Comm.socket = io.connect(token.host, {reconnect: false});
        L.Comm.sendMessage('token', token, callback, error);
    
        L.Comm.socket.on('disconnect', function (argument) {
            L.Logger.info("Socket disconnected");
         });
    };

    var sendMessage = function(type, msg, callback, error) {

        L.Comm.socket.emit(type, msg, function(respType, msg) {
            if(respType === "success") {
                callback(msg);
            } else {
                error(msg);
            }
        });

    };

    var sendSDP = function(type, state, sdp, callback) {
        L.Comm.socket.emit(type, state, sdp, function(response, respCallback) {
            callback(response, respCallback);
        });
    };

    return {
        connect: connect,
        sendMessage: sendMessage,
        sendSDP: sendSDP
    };

}(L);