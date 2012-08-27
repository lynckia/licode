require('./harness');



connection.addListener('ready', function () {
  puts("connected to " + connection.serverProperties.product);

  var nonAmqExchangeCalledback = false;
  var amqExchangeCalledback = false;
  
  connection.exchange('node-simple-fanout', {type: 'fanout'}, function(exchange) {
    nonAmqExchangeCalledback = true;    
  });
  
  connection.exchange('amq.topic', {type: 'topic'}, function(exchange) {
    amqExchangeCalledback = true;    
  });
  setTimeout( function() {
    assert.ok(nonAmqExchangeCalledback, "non amq.* exchange callback method not called");
    assert.ok(amqExchangeCalledback, "amq.topic exchange callback method not called");
    connection.end();
    connection.destroy();
    }, 1000);
});


