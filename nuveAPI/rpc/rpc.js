var sys = require('util');
var amqp = require('amqp');
var rpcPublic = require('./rpcPublic');

var corrID = 0;
var map = {};
var clientQueue;

var connection = amqp.createConnection({host: 'localhost', port: 5672});

connection.on('ready', function () {
	console.log('Conected to rabbitMQ server');

	var exc = connection.exchange('rpcExchange', {type: 'direct'}, function (exchange) {
		console.log('Exchange ' + exchange.name + ' is open');

		var q = connection.queue('nuveQueue', function (queue) {
		  	console.log('Queue ' + queue.name + ' is open');

		  	q.bind('rpcExchange', 'nuve');
	  		q.subscribe(function (message) { 

	    		rpcPublic[message.method](message.args, function(result) {

	    			exc.publish(message.replyTo, {data: result, corrID: message.corrID});
	    		});

	    		
	  		});
		});

		clientQueue = connection.queue('', function (q) {
		  	console.log('ClientQueue ' + q.name + ' is open');

		 	clientQueue.bind('rpcExchange', clientQueue.name);

		  	clientQueue.subscribe(function (message) {
			
				map[message.corrID](message.data);

		  	});

		});
	});

});

exports.callRpc = function(method, args, callback) {

	corrID ++;
	map[corrID] = callback;

	var send = {method: method, args: args, corrID: corrID, replyTo: clientQueue.name};
 	
 	exc.publish('erizoController', send);
	
}