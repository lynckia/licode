require('./harness');

var recvCount = 0;
var body = new Buffer([1,99,253,255,0,1,5,6])

connection.addListener('ready', function () {
  puts("connected to " + connection.serverProperties.product);

  var exchange = connection.exchange('node-binary-fanout', {type: 'fanout'});

  var q = connection.queue('node-binary-queue', function() {

    q.bind(exchange, "*");
  
    q.subscribeRaw(function (m) {
      var data;
      m.addListener('data', function (d) { data = d; });
      m.addListener('end', function () {
        recvCount++;
        m.acknowledge();
        switch (m.routingKey) {
          case 'message.bin1':
            assert.equal(util.inspect(body), util.inspect(data));
            break;

          default:
            throw new Error('unexpected routing key: ' + m.routingKey);
        }
      });
    })
    .addCallback(function () {
      puts("publishing 1 raw message");
      exchange.publish('message.bin1', body);
  
      setTimeout(function () {
        // wait one second to receive the message, then quit
        connection.end();
      }, 1000);
    })
  });
});


process.addListener('exit', function () {
  assert.equal(1, recvCount);
});
