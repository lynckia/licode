require('./harness');

var recvCount = 0;
var body = "hello world";

var properties = {
    "contentType"     : "application/test",
    "contentEncoding" : "binary",
    "priority"        : 9,
    "correlationId"   : "test for correlationId",
    "replyTo"         : "test for replyTo",
    "expiration"      : "test for expiration",
    "messageId"       : "test for messageId",
    "timestamp"       : parseInt(Date.now() / 1000, 10),
    "type"            : "test for type",
    "userId"          : "guest",
    "appId"           : "test for appId",
    "clusterId"       : null
};

connection.addListener('ready', function () {
  puts("connected to " + connection.serverProperties.product);

  var exchange = connection.exchange('node-json-fanout', {type: 'fanout'});

  var q = connection.queue('node-json-queue', function() {

    q.bind(exchange, "*");
  
    q.subscribe(function (json, headers, deliveryInfo) {
      var key = deliveryInfo.routingKey;
      recvCount++;
      assert.equal(properties[key], deliveryInfo[key]);
    })
    .addCallback(function () {
      puts("publishing " + Object.keys(properties).length + " messages");
      Object.keys(properties).forEach(function(p) {
        var props = {};
        props[p] = properties[p];
        exchange.publish(p, body, props);
      });
  
      setTimeout(function () {
        // wait one second to receive the message, then quit
        connection.end();
      }, 1000);
    })
  });
});


process.addListener('exit', function () {
  assert.equal(Object.keys(properties).length, recvCount);
});
