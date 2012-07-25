require('./harness');

var recvCount = 0;
var body = "hello world";

connection.addListener('ready', function () {
  puts("connected to " + connection.serverProperties.product);

  var e = connection.exchange('node-ack-fanout', {type: 'fanout'});
  var q = connection.queue('node-123ack-queue', function() {
    q.bind(e, 'ackmessage.*');
    q.on('queueBindOk', function() {
      q.on('basicConsumeOk', function () {
        puts("publishing 2 json messages");

        e.publish('ackmessage.json1', { name: 'A' });
        e.publish('ackmessage.json2', { name: 'B' });
      });
      
      q.subscribe({ ack: true }, function (json) {
        recvCount++;
        puts('Got message ' + JSON.stringify(json));

        if (recvCount == 1) {
          puts('Got message 1.. waiting');
          assert.equal('A', json.name);
          setTimeout(function () {
            puts('shift!');
            q.shift();
          }, 1000);
        } else if (recvCount == 2) {
          puts('got message 2');
          assert.equal('B', json.name);

          puts('closing connection');

          connection.end();
        } else {
          throw new Error('Too many message!');
        }
      })
    })
  });
});


process.addListener('exit', function () {
  assert.equal(2, recvCount);
});
