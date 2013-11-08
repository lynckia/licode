var sys = require('util');
var amqp = require('amqp');
var rpcPublic = require('./rpcPublic');
var config = require('./../../../licode_config');
var logger = require('./../logger').logger;

var TIMEOUT = 2000;

var corrID = 0;
var map = {};	//{corrID: {fn: callback, to: timeout}}
var connection;
var exc;
var clientQueue;

var addr = {};

if (config.rabbit.url !== undefined) {
	addr.url = config.rabbit.url;
} else {
	addr.host = config.rabbit.host;
	addr.port = config.rabbit.port;
}

exports.connect = function(callback) {

	// Create the amqp connection to rabbitMQ server
	connection = amqp.createConnection(addr);
	connection.on('ready', function () {

		//Create a direct exchange
		exc = connection.exchange('rpcExchange', {type: 'direct'}, function (exchange) {
			logger.info('Exchange ' + exchange.name + ' is open');

			//Create the queue for send messages
	  		clientQueue = connection.queue('', function (q) {
			  	logger.info('ClientQueue ' + q.name + ' is open');

			 	clientQueue.bind('rpcExchange', clientQueue.name);

			  	clientQueue.subscribe(function (message) {

			  		if(map[message.corrID] !== undefined) {
			  			clearTimeout(map[message.corrID].to);
						map[message.corrID].fn(message.data);
						delete map[message.corrID];
					}
			  	});

			  	callback();
			});
		});
	});
}

exports.bind = function(id, callback) {

	//Create the queue for receive messages
	var q = connection.queue(id, function (queueCreated) {
	  	logger.info('Queue ' + queueCreated.name + ' is open');

	  	q.bind('rpcExchange', id);
  		q.subscribe(function (message) {

    		rpcPublic[message.method](message.args, function(result) {

    			exc.publish(message.replyTo, {data: result, corrID: message.corrID});
    		});

  		});

  		callback();

	});
}

/*
 * Calls remotely the 'method' function defined in rpcPublic of 'to'.
 */
exports.callRpc = function(to, method, args, callback) {

	corrID ++;
	map[corrID] = {};
	map[corrID].fn = callback;
	map[corrID].to = setTimeout(callbackError, TIMEOUT, corrID);

	var send = {method: method, args: args, corrID: corrID, replyTo: clientQueue.name };

 	exc.publish(to, send);

}

var callbackError = function(corrID) {
	map[corrID].fn('timeout');
	delete map[corrID];
}