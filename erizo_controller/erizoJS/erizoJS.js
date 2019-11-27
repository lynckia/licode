/* global require */

// eslint-disable-next-line import/no-extraneous-dependencies
const Getopt = require('node-getopt');

let addon;
// eslint-disable-next-line import/no-unresolved
const config = require('./../../licode_config');
// eslint-disable-next-line import/no-unresolved
const mediaConfig = require('./../../rtp_media_config');

global.config = config || {};
global.config.erizo = global.config.erizo || {};
global.config.erizo.numWorkers = global.config.erizo.numWorkers || 24;
global.config.erizo.numIOWorkers = global.config.erizo.numIOWorkers || 1;
global.config.erizo.useConnectionQualityCheck =
  global.config.erizo.useConnectionQualityCheck || false;
global.config.erizo.stunserver = global.config.erizo.stunserver || '';
global.config.erizo.stunport = global.config.erizo.stunport || 0;
global.config.erizo.minport = global.config.erizo.minport || 0;
global.config.erizo.maxport = global.config.erizo.maxport || 0;
global.config.erizo.turnserver = global.config.erizo.turnserver || '';
global.config.erizo.turnport = global.config.erizo.turnport || 0;
global.config.erizo.turnusername = global.config.erizo.turnusername || '';
global.config.erizo.turnpass = global.config.erizo.turnpass || '';
global.config.erizo.networkinterface = global.config.erizo.networkinterface || '';
global.config.erizo.activeUptimeLimit = global.config.erizo.activeUptimeLimit || 7;
global.config.erizo.maxTimeSinceLastOperation = global.config.erizo.maxTimeSinceLastOperation || 3;
global.config.erizo.checkUptimeInterval = global.config.erizo.checkUptimeInterval || 1800;
global.mediaConfig = mediaConfig || {};
// Parse command line arguments
const getopt = new Getopt([
  ['r', 'rabbit-host=ARG', 'RabbitMQ Host'],
  ['g', 'rabbit-port=ARG', 'RabbitMQ Port'],
  ['b', 'rabbit-heartbeat=ARG', 'RabbitMQ AMQP Heartbeat Timeout'],
  ['l', 'logging-config-file=ARG', 'Logging Config File'],
  ['s', 'stunserver=ARG', 'Stun Server IP'],
  ['p', 'stunport=ARG', 'Stun Server port'],
  ['m', 'minport=ARG', 'Minimum port'],
  ['M', 'maxport=ARG', 'Maximum port'],
  ['t', 'turnserver=ARG', 'TURN server IP'],
  ['T', 'turnport=ARG', 'TURN server PORT'],
  ['c', 'turnusername=ARG', 'TURN username'],
  ['C', 'turnpass=ARG', 'TURN password'],
  ['d', 'debug', 'Run Debug erizoAPI addon'],
  ['h', 'help', 'display this help'],
]);

const opt = getopt.parse(process.argv.slice(2));
let isDebugMode = false;

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
    case 'logging-config-file':
      global.config.logger = global.config.logger || {};
      global.config.logger.configFile = value;
      break;
    case 'debug':
      // eslint-disable-next-line import/no-unresolved, global-require
      addon = require('./../../erizoAPI/build/Release/addonDebug');
      isDebugMode = true;
      break;
    default:
      global.config.erizo[prop] = value;
      break;
  }
});

if (!addon) {
  // eslint-disable-next-line
  addon = require('./../../erizoAPI/build/Release/addon');
}

// Load submodules with updated config
const logger = require('./../common/logger').logger;
const amqper = require('./../common/amqper');
const controller = require('./erizoJSController');

// Logger
const log = logger.getLogger('ErizoJS');

const rpcID = process.argv[2];

process.on('unhandledRejection', (error) => {
  log.error('unhandledRejection', error);
});


const threadPool = new addon.ThreadPool(global.config.erizo.numWorkers);
threadPool.start();

const ioThreadPool = new addon.IOThreadPool(global.config.erizo.numIOWorkers);

log.info('Starting ioThreadPool');
ioThreadPool.start();

const ejsController = controller.ErizoJSController(rpcID, threadPool, ioThreadPool);

ejsController.keepAlive = (callback) => {
  callback('callback', true);
};

ejsController.privateRegexp = new RegExp(process.argv[3], 'g');
ejsController.publicIP = process.argv[4];

amqper.connect(() => {
  try {
    amqper.setPublicRPC(ejsController);

    log.info(`message: Started, erizoId: ${rpcID}, isDebugMode: ${isDebugMode}`);

    amqper.bindBroadcast('ErizoJS');
    amqper.bind(`ErizoJS_${rpcID}`, () => {
      log.debug(`message: bound to amqp queue, queueId: ErizoJS_${rpcID}`);
    });
  } catch (err) {
    log.error('message: AMQP connection error, ', logger.objectToLog(err));
  }
});
