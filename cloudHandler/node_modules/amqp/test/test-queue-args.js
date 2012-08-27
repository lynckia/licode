require('./harness');

connection.on('ready', function() {
  puts("connected to " + connection.serverProperties.product);

  connection.queue('node-queue-args-queue', {
      'arguments': {'x-expires': 3600000}
  }, function(q) {
    puts("queue declared");
    var conn = harness_createConnection();

    conn.on('ready', function() {
      var q = conn.queue('node-queue-args-queue', {
          'arguments': {'x-expires': 3600001}
        }, function(q2) {
          puts("second queue declared");
      });
      q.on('error', function(err) {
        assert.equal(err.code, 406);
        assert.ok(err.message.indexOf('PRECONDITION_FAILED') == 0);
        connection.end();
        conn.end();
      });
    });
  });
});
