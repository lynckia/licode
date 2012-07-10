amqp = require('../amqp');

var name = process.argv[2];
console.log("exchange: " + name);

var creds =
  { host:     process.env['AMQP_HOST']      || 'localhost'
  , login:    process.env['AMQP_LOGIN']     || 'guest'
  , password: process.env['AMQP_PASSWORD']  || 'guest'
  , vhost:    process.env['AMQP_VHOST']     || '/'
  };

connection = amqp.createConnection(creds);

connection.addListener('error', function (e) {
  throw e;
});

connection.addListener('ready', function () {
  console.log("Connected");
  var e = connection.exchange(name);
  e.destroy().addCallback(function () {
    console.log('exchange destroyed.');
    connection.close();
  });
});

