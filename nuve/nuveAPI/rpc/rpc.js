/*global exports, require, console, Buffer, setTimeout, clearTimeout*/
var sys = require('util');
var amqp = require('amqp');
var rpcPublic = require('./rpcPublic');
var config = require('./../../../licode_config');

var TIMEOUT = 3000;

var corrID = 0;
var map = {};   //{corrID: {fn: callback, to: timeout}}
var clientQueue;
var connection;
var exc;

// Create the amqp connection to rabbitMQ server
var addr = {};

if (config.rabbit.url !== undefined) {
    addr.url = config.rabbit.url;
} else {
    addr.host = config.rabbit.host;
    addr.port = config.rabbit.port;
}

exports.connect = function () {

    connection = amqp.createConnection(addr);

    connection.on('ready', function () {
        "use strict";

        console.log('Conected to rabbitMQ server');

        //Create a direct exchange 
        exc = connection.exchange('rpcExchange', {type: 'direct'}, function (exchange) {
            console.log('Exchange ' + exchange.name + ' is open');

            //Create the queue for receive messages
            var q = connection.queue('nuveQueue', function (queue) {
                console.log('Queue ' + queue.name + ' is open');

                q.bind('rpcExchange', 'nuve');
                q.subscribe(function (message) {

                    rpcPublic[message.method](message.args, function (type, result) {
                        exc.publish(message.replyTo, {data: result, corrID: message.corrID, type: type});
                    });

                });
            });

            //Create the queue for send messages
            clientQueue = connection.queue('', function (q) {
                console.log('ClientQueue ' + q.name + ' is open');

                clientQueue.bind('rpcExchange', clientQueue.name);

                clientQueue.subscribe(function (message) {

                    if (map[message.corrID] !== undefined) {

                        map[message.corrID].fn[message.type](message.data);
                        clearTimeout(map[message.corrID].to);
                        delete map[message.corrID];
                    }
                });

            });
        });

    });
}

var callbackError = function (corrID) {
    "use strict";

    map[corrID].fn('timeout');
    delete map[corrID];
};

/*
 * Calls remotely the 'method' function defined in rpcPublic of 'to'.
 */
exports.callRpc = function (to, method, args, callbacks) {
    "use strict";

    corrID += 1;
    map[corrID] = {};
    map[corrID].fn = callbacks;
    map[corrID].to = setTimeout(callbackError, TIMEOUT, corrID);

    var send = {method: method, args: args, corrID: corrID, replyTo: clientQueue.name};

    exc.publish(to, send);

};

