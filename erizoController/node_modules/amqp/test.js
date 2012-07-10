var amqp = require('./amqp');


var connection = amqp.createConnection({host: 'localhost'});


connection.addListener('close', function (e) {
  if (e) {
    throw e;
  } else {
    console.log('connection closed.');
  }
});


connection.addListener('ready', function () {
  console.log("connected to " + connection.serverProperties.product);

  var exchange = connection.exchange('clock', {type: 'fanout'});

  var q = connection.queue('my-events-receiver');

  q.bind(exchange, "*").addCallback(function () {
    console.log("publishing message");
    exchange.publish("message.json", {hello: 'world', foo: 'bar'});
    exchange.publish("message.text", 'hello world', {contentType: 'text/plain'});
  });

  q.subscribe(function (m) {
    console.log("--- Message (" + m.deliveryTag + ", '" + m.routingKey + "') ---");
    console.log("--- contentType: " + m.contentType);

    m.addListener('data', function (d) {
      console.log(d);
    });

    m.addListener('end', function () {
      m.acknowledge();
      console.log("--- END (" + m.deliveryTag + ", '" + m.routingKey + "') ---");
    });
  });
});
