require('./harness');
// test-type-and-headers.js
var recvCount = 0;
var body = "the devil is in the type, and also in the headers";

connection.addListener('ready', function () {
  puts("connected to " + connection.serverProperties.product);

  connection.exchange('node-th-fanout', {type: 'fanout'}, function(exchange) {
      connection.queue('node-th-queue', function(q) {
        q.bind(exchange, "*");
        q.on('queueBindOk', function() {
          q.on('basicConsumeOk', function () {
            puts("publishing message");
            exchange.publish("message.text", body,
            {
              type: 'typeProperty',
              headers:
              {
                stringHeader: "Hello, World",
                bigIntP : 0xffffffff + 1,
                bigIntN : -0xffffffff - 1,
                intP : 1234,
                intN : (-1234),
                floatP: 1.1234,
                floatN: -1.1234,
                boolT: true,
                boolF: false,
                date: new Date(2001, 00, 01),
                obj: { hello: "world" },
                buf: new Buffer([0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15])
              }
            });
          });
          q.subscribeRaw(function (m) {
            puts("--- Message (" + m.deliveryTag + ", '" + m.routingKey + "') ---");
            puts("--- type: " + m.type);
            puts("--- headers: " + JSON.stringify(m.headers));
            puts("");
            recvCount++;
            assert.equal('typeProperty', m.type);
            assert.equal('Hello, World', m.headers.stringHeader);
            assert.equal(0xffffffff + 1, m.headers.bigIntP);
            assert.equal(-0xffffffff - 1, m.headers.bigIntN);
            assert.equal(1234, m.headers.intP);
            assert.equal(-1234, m.headers.intN);
            assert.equal(1.1234, m.headers.floatP);
            assert.equal(-1.1234, m.headers.floatN);
            assert.equal(true, m.headers.boolT);
            assert.equal(false, m.headers.boolF);
            assert.equal(new Date(2001,00,01).valueOf(), m.headers.date.valueOf());
            assert.equal(JSON.stringify({hello:"world"}), JSON.stringify(m.headers.obj));
            assert.equal(new Buffer([0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15]).toString(), m.headers.buf.toString());

          });
          setTimeout(function () {
            // wait one second to receive the message, then quit
            connection.end();
          }, 1000);
        });
      });

  });
});


process.addListener('exit', function () {
  assert.equal(1, recvCount);
});
