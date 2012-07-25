require('./harness');

var fire, fired = false,
    replyCode = null,
    replyText = null;

connection.addListener('ready', function () {
  connection.exchange('node-simple-fanout', {type: 'fanout'},
                      function(exchange) {
    exchange.on('basic-return', function(args) {
      fired = true;
      replyCode = args['replyCode'];
      replyText = args['replyText'];
      clearTimeout(fire);
      followup();
    });
    exchange.publish("", "hello", { mandatory: true, immediate: true });
  });
});

function followup() {
  assert.ok(fired);
  assert.ok(replyCode === 312);
  assert.ok(replyText === "NO_ROUTE");
  connection.end();
}
fire = setTimeout(function() {
  followup();
}, 5000);
