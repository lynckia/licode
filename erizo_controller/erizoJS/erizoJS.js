/*global require*/
'use strict';
var Getopt = require('node-getopt');
var config = require('config');

var ERIZO_API_ADDON_PATH = config.get('erizo.erizoAPIAddonPath');

var addon = require(ERIZO_API_ADDON_PATH);

GLOBAL.config =  {};
GLOBAL.config.erizo = config.get('erizo');
GLOBAL.config.erizoController = config.get('erizoController');
GLOBAL.mediaConfig = config.get('mediaConfig');

// Parse command line arguments
var getopt = new Getopt([
  ['r' , 'rabbit-host=ARG'            , 'RabbitMQ Host'],
  ['g' , 'rabbit-port=ARG'            , 'RabbitMQ Port'],
  ['b' , 'rabbit-heartbeat=ARG'       , 'RabbitMQ AMQP Heartbeat Timeout'],
  ['l' , 'logging-config-file=ARG'    , 'Logging Config File'],
  ['s' , 'stunserver=ARG'             , 'Stun Server IP'],
  ['p' , 'stunport=ARG'               , 'Stun Server port'],
  ['m' , 'minport=ARG'                , 'Minimum port'],
  ['M' , 'maxport=ARG'                , 'Maximum port'],
  ['t' , 'turnserver=ARG'             , 'TURN server IP'],
  ['T' , 'turnport=ARG'               , 'TURN server PORT'],
  ['c' , 'turnusername=ARG'           , 'TURN username'],
  ['C' , 'turnpass=ARG'               , 'TURN password'],
  ['h' , 'help'                       , 'display this help']
]);

var opt = getopt.parse(process.argv.slice(2));

for (var prop in opt.options) {
    if (opt.options.hasOwnProperty(prop)) {
        var value = opt.options[prop];
        switch (prop) {
            case 'help':
                getopt.showHelp();
                process.exit(0);
                break;
            case 'rabbit-host':
                GLOBAL.config.rabbit = GLOBAL.config.rabbit || {};
                GLOBAL.config.rabbit.host = value;
                break;
            case 'rabbit-port':
                GLOBAL.config.rabbit = GLOBAL.config.rabbit || {};
                GLOBAL.config.rabbit.port = value;
                break;
            case 'rabbit-heartbeat':
                GLOBAL.config.rabbit = GLOBAL.config.rabbit || {};
                GLOBAL.config.rabbit.heartbeat = value;
                break;
            case 'logging-config-file':
                GLOBAL.config.logger = GLOBAL.config.logger || {};
                GLOBAL.config.logger.configFile = value;
                break;
            default:
                GLOBAL.config.erizo[prop] = value;
                break;
        }
    }
}

// Load submodules with updated config
var logger = require('./common/logger').logger;
var amqper = require('./common/amqper');
var controller = require('./erizoJSController');

// Logger
var log = logger.getLogger('ErizoJS');

var threadPool = new addon.ThreadPool(GLOBAL.config.erizo.numWorkers);
threadPool.start();

var ioThreadPool = new addon.IOThreadPool(GLOBAL.config.erizo.numIOWorkers);

if (GLOBAL.config.erizo.useNicer) {
  log.info('Starting ioThreadPool');
  ioThreadPool.start();
}

var ejsController = controller.ErizoJSController(threadPool, ioThreadPool);

ejsController.keepAlive = function(callback) {
    callback('callback', true);
};

ejsController.privateRegexp = new RegExp(process.argv[3], 'g');
ejsController.publicIP = process.argv[4];

amqper.connect(function () {
    try {
        amqper.setPublicRPC(ejsController);

        var rpcID = process.argv[2];

        log.info('message: Started, erizoId: ' + rpcID);

        amqper.bindBroadcast('ErizoJS');
        amqper.bind('ErizoJS_' + rpcID, function() {
            log.debug('message: bound to amqp queue, queueId: ErizoJS_' + rpcID );

        });
    } catch (err) {
        log.error('message: AMQP connection error, ' + logger.objectToLog(err));
    }

});
