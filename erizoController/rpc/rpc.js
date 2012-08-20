var sys = require('util');
var amqp = require('amqp');
var rpcPublic = require('./rpcPublic');

var corrID = 0;
var map = {};
var connection;
var exc;
var clientQueue;

exports.connect = function(callback) {

	// Create the amqp connection to rabbitMQ server
	connection = amqp.createConnection({host: 'chotis2.dit.upm.es', port: 5672});
	connection.on('ready', function () {

		//Create a direct exchange 
		exc = connection.exchange('rpcExchange', {type: 'direct'}, function (exchange) {
			console.log('Exchange ' + exchange.name + ' is open');

			//Create the queue for send messages
	  		clientQueue = connection.queue('', function (q) {
			  	console.log('ClientQueue ' + q.name + ' is open');

			 	clientQueue.bind('rpcExchange', clientQueue.name);

			  	clientQueue.subscribe(function (message) {
				
					map[message.corrID](message.data);
					delete map[message.corrID];

			  	});

			  	callback();
			});
		});
	});
}

exports.bind = function(id, callback) {

	//Create the queue for receive messages
	var q = connection.queue(id, function (queueCreated) {
	  	console.log('Queue ' + queueCreated.name + ' is open');

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
	map[corrID] = callback;

	var send = {method: method, args: args, corrID: corrID, replyTo: clientQueue.name };
 	
 	exc.publish(to, send);
	
}
