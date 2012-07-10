var sys = require('util');
var amqp = require('amqp');
var rpcPublic = require('./rpcPublic');

var connection = amqp.createConnection({host: 'localhost', port: 5672});

connection.on('ready', function () {
	console.log('Conected to rabbitMQ server');

	var exc = connection.exchange('rpcExchange', {type: 'direct'}, function (exchange) {
		console.log('Exchange ' + exchange.name + ' is open');

		var q = connection.queue('serverQueue', function (queue) {
		  	console.log('Queue ' + queue.name + ' is open');

		  	q.bind('rpcExchange', 'server');
	  		q.subscribe(function (message) { 

	    		rpcPublic[message.method](message.args, function(result) {

	    			exc.publish(message.replyTo, {data: result, corrID: message.corrID});
	    		});

	    		
	  		});
		});
	});

});