/* global require */
/* eslint-disable  no-console */

// eslint-disable-next-line import/no-extraneous-dependencies
const Getopt = require('node-getopt');
const vm = require('vm');
const repl = require('repl');
const RovClient = require('./rovClient').RovClient;


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

let inspectorRepl;

const maybeRemoteEval = (cmd, context, filename, callback) => {
  if (context.rovClient.hasDefaultSession()) {
    inspectorRepl.setPrompt(`${context.rovClient.defaultSession}>`);
    if (cmd.startsWith('exit')) {
      console.log(`Requested exit from remote session ${context.rovClient.defaultSession}`);
      context.rovClient.closeSession(context.rovClient.defaultSession);
      context.rovClient.setDefaultSession(undefined);
      inspectorRepl.setPrompt('>');
      callback(null, undefined);
      return;
    }
    context.rovClient.runInDefaultSession(cmd).then((rpcResult) => {
      console.log(rpcResult);
      callback(null, undefined);
    });
    return;
  }
  const result = vm.runInContext(cmd, context);
  callback(null, result);
};


const rovClient = new RovClient(amqper);
amqper.connect(() => {
  rovClient.updateComponentsList().then(() => {
    inspectorRepl = repl.start({
      prompt: '> ',
      ignoreUndefined: true,
      useColors: true,
      eval: maybeRemoteEval,
    });

    inspectorRepl.on('exit', () => {
      rovClient.closeAllSessions().then(() => {
        process.exit(0);
      })
        .catch((error) => {
          console.log('Could not close all sessions when exiting', error);
          process.exit(1);
        });
    });

    Object.defineProperty(inspectorRepl.context, 'rovClient', {
      configurable: false,
      enumerable: true,
      value: rovClient,
    });
  });
});
