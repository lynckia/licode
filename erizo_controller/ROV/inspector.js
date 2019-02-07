/* global require */
/* eslint-disable  no-console */

// eslint-disable-next-line import/no-extraneous-dependencies
const Getopt = require('node-getopt');
const RovClient = require('./RovClient').RovClient;


// eslint-disable-next-line import/no-unresolved
const config = require('../../licode_config');
// Configuration default values
global.config = config || {};

// Parse command line arguments
const getopt = new Getopt([
  ['r', 'rabbit-host=ARG', 'RabbitMQ Host'],
  ['g', 'rabbit-port=ARG', 'RabbitMQ Port'],
  ['b', 'rabbit-heartbeat=ARG', 'RabbitMQ AMQP Heartbeat Timeout'],
  ['l', 'logging-config-file=ARG', 'Logging Config File'],
  ['h', 'help', 'display this help'],
]);

const opt = getopt.parse(process.argv.slice(2));


Object.keys(opt.options).forEach((prop) => {
  const value = opt.options[prop];
  switch (prop) {
    case 'help':
      getopt.showHelp();
      process.exit(0);
      break;
    case 'rabbit-host':
      global.config.rabbit = global.config.rabbit || {};
      global.config.rabbit.host = value;
      break;
    case 'rabbit-port':
      global.config.rabbit = global.config.rabbit || {};
      global.config.rabbit.port = value;
      break;
    case 'rabbit-heartbeat':
      global.config.rabbit = global.config.rabbit || {};
      global.config.rabbit.heartbeat = value;
      break;
    default:
      global.config.erizoAgent[prop] = value;
      break;
  }
});

// Load submodules with updated config
const amqper = require('./../common/amqper');
const repl = require('repl');

const rovClient = new RovClient(amqper);
const r = repl.start({
  prompt: '> ',
  ignoreUndefined: true,
  useColors: true,
});

r.on('exit', () => {
  rovClient.closeAllSessions();
  process.exit(0);
});

Object.defineProperty(r.context, 'rovClient', {
  configurable: false,
  enumerable: true,
  value: rovClient,
});
