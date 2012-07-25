require('./harness');

connects = 0;

connection.addListener('ready', function () {
  connects++;
  puts("connected to " + connection.serverProperties.product);

  var e = connection.exchange();

  var q = connection.queue('node-test-autodelete', {exclusive: true});
  q.on('queueDeclareOk', function (args) {
    puts('queue opened.');
    assert.equal(0, args.messageCount);
    assert.equal(0, args.consumerCount);
    
    q.bind(e, "#");
    
    q.on('queueBindOk', function () {
      puts('bound');
      // publish message, but don't consume it.
      e.publish('routingKey', {hello: 'world'});
      puts('message published');
      puts('closing connection...');
      connection.end();
    });
  });

});

connection.addListener('close', function () {
  if (connects < 3) connection.reconnect();
});


process.addListener('exit', function () {
  assert.equal(3, connects);
});
