require('./harness');

var recvCount = 0;
var body = "hello world";

connection.addListener('ready', function () {
  puts("connected to " + connection.serverProperties.product);

  var exchange = connection.exchange('node-json-fanout', {type: 'fanout'});

  var q = connection.queue('node-json-queue', function() {
    var origMessage1 = {two:2, one:1},
        origMessage2 = {foo:'bar', hello: 'world'},
        origMessage3 = {coffee:'caf\u00E9', tea: 'th\u00E9'};

    q.bind(exchange, "*");
  
    q.subscribe(function (json, headers, deliveryInfo) {
      recvCount++;
  
      assert.equal("node-json-fanout", deliveryInfo.exchange);
      assert.equal("node-json-queue", deliveryInfo.queue);
      assert.equal(false, deliveryInfo.redelivered);

      switch (deliveryInfo.routingKey) {
        case 'message.json1':
          assert.deepEqual(origMessage1, json);
          break;
  
        case 'message.json2':
          assert.deepEqual(origMessage2, json);
          break;
  
        case 'message.json3':
          assert.deepEqual(origMessage3, json);
          break;
  
        default:
          throw new Error('unexpected routing key: ' + deliveryInfo.routingKey);
      }
    })
    .addCallback(function () {
      puts("publishing 3 json messages");
      exchange.publish('message.json1', origMessage1);
      exchange.publish('message.json2', origMessage2, {contentType: 'application/json'});
      exchange.publish('message.json3', origMessage3, {contentType: 'application/json'});
  
      setTimeout(function () {
        // wait one second to receive the message, then quit
        connection.end();
      }, 1000);
    })
  });
});


process.addListener('exit', function () {
  assert.equal(3, recvCount);
});
