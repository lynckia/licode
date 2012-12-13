var sys = require('util');
var amqp = require('amqp');
var rpcPublic = require('./rpcPublic');
var config = require('./../../../lynckia_config');

var TIMEOUT = 2000;

var corrID = 0;
var map = {};	//{corrID: {fn: callback, to: timeout}}
var clientQueue;
var exc;

// Create the amqp connection to rabbitMQ server
var addr = {};

if (config.rabbit.url !== undefined) {
	addr.url = config.rabbit.url;
} else {
	addr.host = config.rabbit.host;
	addr.port = config.rabbit.port;
}

var connection = amqp.createConnection(addr);

connection.on('ready', function () {
	console.log('Conected to rabbitMQ server');

	//Create a direct exchange 
	exc = connection.exchange('rpcExchange', {type: 'direct'}, function (exchange) {
		console.log('Exchange ' + exchange.name + ' is open');

		//Create the queue for receive messages
		var q = connection.queue('cloudHandlerQueue', function (queue) {
		  	console.log('Queue ' + queue.name + ' is open');

		  	q.bind('rpcExchange', 'cloudHandler');
	  		q.subscribe(function (message) { 

	    		rpcPublic[message.method](message.args, function(result) {

	    			exc.publish(message.replyTo, {data: result, corrID: message.corrID});
	    		});

	    		
	  		});
		});

		//Create the queue for send messages
		clientQueue = connection.queue('', function (q) {
		  	console.log('ClientQueue ' + q.name + ' is open');

		 	clientQueue.bind('rpcExchange', clientQueue.name);

		  	clientQueue.subscribe(function (message) {

		  		if(map[message.corrID] !== undefined) {
			
					map[message.corrID].fn(message.data);
					clearTimeout(map[message.corrID].to);
					delete map[message.corrID];
				}

		  	});

		});
	});

});

/*
 * Calls remotely the 'method' function defined in rpcPublic of 'to'.
 */
exports.callRpc = function(to, method, args, callback) {

	corrID ++;
	map[corrID] = {};
	map[corrID].fn = callback;
	map[corrID].to = setTimeout(callbackError, TIMEOUT, corrID);

	var send = {method: method, args: args, corrID: corrID, replyTo: clientQueue.name};
 	
 	exc.publish(to, send);
	
}

var callbackError = function(corrID) {
	map[corrID].fn('timeout');
	delete map[corrID];
}



