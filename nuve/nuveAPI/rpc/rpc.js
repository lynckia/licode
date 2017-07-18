/*global exports, require, setTimeout, clearTimeout*/
'use strict';
var amqp = require('amqp');
var rpcPublic = require('./rpcPublic');
var config = require('config');
var logger = require('./../logger').logger;

// Logger
var log = logger.getLogger('RPC');

var rabbitConfig = config.get('rabbit');
var TIMEOUT = 3000;

var corrID = 0;
var map = {};   //{corrID: {fn: callback, to: timeout}}
var clientQueue;
var connection;
var exc;

// Create the amqp connection to rabbitMQ server
var addr = {};

if (rabbitConfig.url !== undefined) {
    addr.url = rabbitConfig.url;
} else {
    addr.host = rabbitConfig.host;
    addr.port = rabbitConfig.port;
    addr.login = rabbitConfig.login;
    addr.password = rabbitConfig.password;
}

if(rabbitConfig.heartbeat !==undefined){
    addr.heartbeat = rabbitConfig.heartbeat;
}

exports.connect = function (callback) {

    connection = amqp.createConnection(addr);

    connection.on('ready', function () {
        log.info('message: AMQP connected');

        //Create a direct exchange
        exc = connection.exchange('rpcExchange', {type: 'direct'}, function (exchange) {
            log.info('message: rpcExchange open, exchangeName: ' + exchange.name);

            var next = function() {
              //Create the queue for receive messages
              var q = connection.queue('nuveQueue', function (queue) {
                  log.info('message: queue open, queueName: ' + queue.name);

                  q.bind('rpcExchange', 'nuve');
                  q.subscribe(function (message) {

                      rpcPublic[message.method](message.args, function (type, result) {
                          exc.publish(message.replyTo,
                                      {data: result, corrID: message.corrID, type: type});
                      });

                  });
                  if (callback) {
                      callback();
                  }
              });
            };

            //Create the queue for send messages
            clientQueue = connection.queue('', function (q) {
                log.info('message: clientQueue open, queueName: ' + q.name);

                clientQueue.bind('rpcExchange', clientQueue.name);

                clientQueue.subscribe(function (message) {

                    if (map[message.corrID] !== undefined) {

                        map[message.corrID].fn[message.type](message.data);
                        clearTimeout(map[message.corrID].to);
                        delete map[message.corrID];
                    }
                });
                next();
            });
        });

    });

    connection.on('error', function(e) {
       log.error('message: AMQP connection error killing process, errorMsg: ' +
           logger.objectToLog(e));
       process.exit(1);
    });
};

var callbackError = function (corrID) {
    for (var i in map[corrID].fn) {
        map[corrID].fn[i]('timeout');
    }
    delete map[corrID];
};

/*
 * Calls remotely the 'method' function defined in rpcPublic of 'to'.
 */
exports.callRpc = function (to, method, args, callbacks) {
    corrID += 1;
    map[corrID] = {};
    map[corrID].fn = callbacks;
    map[corrID].to = setTimeout(callbackError, TIMEOUT, corrID);

    var send = {method: method, args: args, corrID: corrID, replyTo: clientQueue.name};

    exc.publish(to, send);

};
