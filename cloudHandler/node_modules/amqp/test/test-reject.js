require('./harness');

var received_count = 0;

connection.addListener('ready', function () {
  puts("connected to " + connection.serverProperties.product);

  var exchange = connection.exchange('node-json-fanout', {type: 'fanout'});

  var q = connection.queue('node-json-queue', {autoDelete: false}, function() {
    var origMessage1 = {two:2, one:1},
				origMessage2 = {three:3};
				rejected_count = 0;

    q.bind(exchange, "*");
  
    q.subscribe({ack: true}, function (json, headers, deliveryInfo, m) {
			received_count++;
			if (deliveryInfo.routingKey == 'accept'){
				m.acknowledge();
			} else {
				if (++rejected_count < 3){	
					m.reject(true);
				} else {
					m.reject();
				}					
      }
		})
    .addCallback(function () {
     	exchange.publish('reject', origMessage1);
 			exchange.publish('accept', origMessage2);

      setTimeout(function () {
        // wait one second to receive the message, then quit
        connection.end();
      }, 1000);
    })
  });
});


process.addListener('exit', function () {
	assert.equal(received_count, 4);
});
