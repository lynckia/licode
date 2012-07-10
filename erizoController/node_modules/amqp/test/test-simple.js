require('./harness');

var recvCount = 0;
var body = "hello world";

connection.addListener('ready', function () {
  puts("connected to " + connection.serverProperties.product);

  connection.exchange('node-simple-fanout', {type: 'fanout'}, function(exchange) {
      connection.queue('node-simple-queue', function(q) {
        q.bind(exchange, "*")
        q.on('queueBindOk', function() {
          q.on('basicConsumeOk', function () {
            puts("publishing message");
            exchange.publish("message.text", body, {contentType: 'text/plain'});

            setTimeout(function () {
              // wait one second to receive the message, then quit
              connection.end();
            }, 1000);
          });

          q.subscribeRaw(function (m) {
            puts("--- Message (" + m.deliveryTag + ", '" + m.routingKey + "') ---");
            puts("--- contentType: " + m.contentType);

            recvCount++;

            assert.equal('text/plain', m.contentType);

            var size = 0;
            m.addListener('data', function (d) { size += d.length; });

            m.addListener('end', function () {
              assert.equal(body.length, size);
              m.acknowledge();
            });
          })
        });
      });
  });
});


process.addListener('exit', function () {
  assert.equal(1, recvCount);
});
