global.options = { heartbeat: 1 };

require('./harness');

connects = 0;
var closed = 0;

var hb = setInterval(function() {
  puts(" -> heartbeat");
  connection.heartbeat();
}, 1000);

setTimeout(function() {
  assert.ok(!closed);
  clearInterval(hb);
  setTimeout(function() { assert.ok(closed); }, 3000);
}, 5000);

connection.on('heartbeat', function() {
  puts(" <- heartbeat");
});
connection.on('close', function() {
  puts("closed");
  closed = 1;
});
connection.addListener('ready', function () {
  connects++;
  puts("connected to " + connection.serverProperties.product);

  var e = connection.exchange();

  var q = connection.queue('node-test-hearbeat', {autoDelete: true});
  q.on('queueDeclareOk', function (args) {
    puts('queue opened.');
    assert.equal(0, args.messageCount);
    assert.equal(0, args.consumerCount);

    q.bind(e, "#");
    q.subscribe(function(json) {
      assert.ok(false);
    });
  });
});
