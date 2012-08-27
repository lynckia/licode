require('./harness');

var queueName = 'node-unsubscribe-test';
var counter = 0;

connection.on('ready', function() {
  var ex = connection.exchange('');
  var q = connection.queue(queueName, {autoDelete: false}, function() {
    ex.publish(queueName, {"msg": 'Message1'});
    var defr = q.subscribe(function() { counter += 1; });
    defr.addCallback(function(ok) {
      // NB there is a race here, since the publish above and this
      // unsubscribe are sent over different channels.
      var defr2 = q.unsubscribe(ok.consumerTag);
      defr2.addCallback(function() {
        connection.publish(queueName, {"msg": 'Message2'});
        // alas I cannot think of a way to synchronise on the queue
        setTimeout(function() { q.destroy().addCallback(function() {
          connection.end(); })}, 500);
      });
    });
  });
});

process.addListener('exit', function() {
  assert.equal(1, counter);
});
