/*global require*/
'use strict';
var Getopt = require('node-getopt');
var addon;
var config = require('./../../licode_config');
var mediaConfig = require('./../../rtp_media_config');

global.config = config || {};
global.config.erizo = global.config.erizo || {};
global.config.erizo.numWorkers = global.config.erizo.numWorkers || 24;
global.config.erizo.numIOWorkers = global.config.erizo.numIOWorkers || 1;
global.config.erizo.useNicer = global.config.erizo.useNicer || false;
global.config.erizo.stunserver = global.config.erizo.stunserver || '';
global.config.erizo.stunport = global.config.erizo.stunport || 0;
global.config.erizo.minport = global.config.erizo.minport || 0;
global.config.erizo.maxport = global.config.erizo.maxport || 0;
global.config.erizo.turnserver = global.config.erizo.turnserver || '';
global.config.erizo.turnport = global.config.erizo.turnport || 0;
global.config.erizo.turnusername = global.config.erizo.turnusername || '';
global.config.erizo.turnpass = global.config.erizo.turnpass || '';
global.config.erizo.networkinterface = global.config.erizo.networkinterface || '';
global.mediaConfig = mediaConfig || {};
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
  ['d', 'debug'                   , 'Run Debug erizoAPI addon'],
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
                console.log('Loading debug version');
                addon = require('./../../erizoAPI/build/Release/addonDebug');
                break;
            default:
                global.config.erizo[prop] = value;
                break;
        }
    }
}

if (!addon) {
  addon = require('./../../erizoAPI/build/Release/addon');
}

// Load submodules with updated config
var logger = require('./../common/logger').logger;
var amqper = require('./../common/amqper');
var controller = require('./erizoJSController');

// Logger
var log = logger.getLogger('ErizoJS');

var threadPool = new addon.ThreadPool(global.config.erizo.numWorkers);
threadPool.start();

var ioThreadPool = new addon.IOThreadPool(global.config.erizo.numIOWorkers);

if (global.config.erizo.useNicer) {
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
