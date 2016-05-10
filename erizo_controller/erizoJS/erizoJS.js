/*global require, logger. setInterval, clearInterval, Buffer, exports*/
var Getopt = require('node-getopt');
var config = require('./../../licode_config');

GLOBAL.config = config || {};
GLOBAL.config.erizo = GLOBAL.config.erizo || {};
GLOBAL.config.erizo.stunserver = GLOBAL.config.erizo.stunserver || '';
GLOBAL.config.erizo.stunport = GLOBAL.config.erizo.stunport || 0;
GLOBAL.config.erizo.minport = GLOBAL.config.erizo.minport || 0;
GLOBAL.config.erizo.maxport = GLOBAL.config.erizo.maxport || 0;
GLOBAL.config.erizo.turnserver = GLOBAL.config.erizo.turnserver || '';
GLOBAL.config.erizo.turnport = GLOBAL.config.erizo.turnport || 0;
GLOBAL.config.erizo.turnusername = GLOBAL.config.erizo.turnusername || '';
GLOBAL.config.erizo.turnpass = GLOBAL.config.erizo.turnpass || '';

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

opt = getopt.parse(process.argv.slice(2));

for (var prop in opt.options) {
    if (opt.options.hasOwnProperty(prop)) {
        var value = opt.options[prop];
        switch (prop) {
            case "help":
                getopt.showHelp();
                process.exit(0);
                break;
            case "rabbit-host":
                GLOBAL.config.rabbit = GLOBAL.config.rabbit || {};
                GLOBAL.config.rabbit.host = value;
                break;
            case "rabbit-port":
                GLOBAL.config.rabbit = GLOBAL.config.rabbit || {};
                GLOBAL.config.rabbit.port = value;
                break;
            case "rabbit-heartbeat":
                GLOBAL.config.rabbit = GLOBAL.config.rabbit || {};
                GLOBAL.config.rabbit.heartbeat = value;
                break;
            case "logging-config-file":
                GLOBAL.config.logger = GLOBAL.config.logger || {};
                GLOBAL.config.logger.config_file = value;
                break;
            default:
                GLOBAL.config.erizo[prop] = value;
                break;
        }
    }
}

// Load submodules with updated config
var logger = require('./../common/logger').logger;
var amqper = require('./../common/amqper');
var controller = require('./erizoJSController');

// Logger
var log = logger.getLogger("ErizoJS");

var ejsController = controller.ErizoJSController();

ejsController.keepAlive = function(callback) {
    callback('callback', true);
};

ejsController.privateRegexp = new RegExp(process.argv[3], 'g');
ejsController.publicIP = process.argv[4];

amqper.connect(function () {
    "use strict";
    try {
        amqper.setPublicRPC(ejsController);

        var rpcID = process.argv[2];

        log.info("ID: ErizoJS_" + rpcID);

        amqper.bind("ErizoJS_" + rpcID, function() {
            log.info("ErizoJS bound to amqp queue");
            
        });
    } catch (err) {
        log.error("AMQP connection error", err);
    }

});
