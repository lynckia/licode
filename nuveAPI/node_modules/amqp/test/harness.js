global.util =  require('util');
puts = console.log;
global.assert =  require('assert');
global.amqp = require('../amqp');

var options = global.options || {};
if (process.argv[2]) {
  var server = process.argv[2].split(':');
  if (server[0]) options.host = server[0];
  if (server[1]) options.port = parseInt(server[1]);
}
var implOpts = {
  defaultExchangeName: 'amq.topic'
};

function harness_createConnection() {
  return amqp.createConnection(options, implOpts);
}

global.harness_createConnection = harness_createConnection;

global.connection = harness_createConnection();

global.errorCallback = function(e) {
  throw e;
};

global.connection.addListener('error', global.errorCallback);

global.connection.addListener('close', function (e) {
  console.log('connection closed.');
});

