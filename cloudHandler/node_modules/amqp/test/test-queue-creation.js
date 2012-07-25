require('./harness');

function countdownLatch(num, callback) {
  var count = num;

  function tick() {
    process.nextTick(function() {
      if (0 == count) {
        callback();
      }
      else {
        tick();
      }
    });
  }

  tick();
  return {
    decr: function() {
      count--;
    }
  }
}

var testsLeft = countdownLatch(3, function() {
  connection.end();
});

// Test multiple creations of the same queue

var callbacks = 0;

connection.on('ready', function() {

  var queueName = 'node-test-queue-creation';
  var queueOpts = {'durable': false};

  var q = connection.queue(queueName, queueOpts, function() {
    callbacks++;
    testsLeft.decr();
    connection.queue(queueName, queueOpts, function() {
      callbacks++;
      testsLeft.decr();
    });

  });

});

// Supplying no name makes the server create a name, which we can see
connection.on('ready', function() {

  var q = connection.queue('', function() {
    assert.ok(q.name != '' && q.name != undefined);
    testsLeft.decr();
  });

});

process.addListener('exit', function() {
  assert.equal(2, callbacks);
});
