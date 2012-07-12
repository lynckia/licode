var L = L || {};

L.Comm = (function (L) {
    "use strict";
    var socket, connect, sendMessage, sendSDP;

    connect = function (token, callback, error) {
        L.Comm.socket = io.connect(token.host, {reconnect: false});

        L.Comm.socket.on('onAddStream', function (stream) {
            L.Logger.info("New Stream " + stream);
        });

        L.Comm.socket.on('disconnect', function (argument) {
            L.Logger.info("Socket disconnected");
        });



        L.Comm.sendMessage('token', token, callback, error);


    };

    sendMessage = function (type, msg, callback, error) {

        L.Comm.socket.emit(type, msg, function (respType, msg) {
            if (respType === "success") {
                callback(msg);
            } else {
                error(msg);
            }
        });

    };

    sendSDP = function (type, state, sdp, callback) {
        L.Comm.socket.emit(type, state, sdp, function (response, respCallback) {
            callback(response, respCallback);
        });
    };

    return {
        connect: connect,
        sendMessage: sendMessage,
        sendSDP: sendSDP
    };

}(L));