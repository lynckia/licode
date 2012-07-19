var sys = require('util');
var amqp = require('amqp');
var rpcPublic = require('./rpcPublic');

var corrID = 0;
var map = {};
var exc;
var queue;

exports.connect = function(callback) {

	var connection = amqp.createConnection({host: 'chotis2.dit.upm.es', port: 5672});
	connection.on('ready', function () {

		exc = connection.exchange('rpcExchange', {type: 'direct'}, function (exchange) {
			console.log('Exchange ' + exchange.name + ' is open');

			var q = connection.queue('erizoControllerQueue', function (queueCreated) {
			  	console.log('Queue ' + queueCreated.name + ' is open');

			  	q.bind('rpcExchange', 'erizoController');
		  		q.subscribe(function (message) { 

		    		rpcPublic[message.method](message.args, function(result) {

		    			exc.publish(message.replyTo, {data: result, corrID: message.corrID});
		    		});

		  		});

		  		queue = connection.queue('', function (q) {
				  	console.log('Queue ' + q.name + ' is open');

				 	queue.bind('rpcExchange', queue.name);

				  	queue.subscribe(function (message) {
					
						map[message.corrID](message.data);

				  	});

				  	callback();
				});

			});

		});
	});
}

exports.callRpc = function(method, args, callback) {

	corrID ++;
	map[corrID] = callback;

	var send = {method: method, args: args, corrID: corrID, replyTo: queue.name };
 	
 	exc.publish('nuve', send);
	
}
