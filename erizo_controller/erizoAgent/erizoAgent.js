/*global require, logger. setInterval, clearInterval, Buffer, exports*/
var Getopt = require('node-getopt');

var spawn = require('child_process').spawn;

var config = require('./../../licode_config');

// Configuration default values
GLOBAL.config = config || {};
GLOBAL.config.erizoAgent = GLOBAL.config.erizoAgent || {};
GLOBAL.config.erizoAgent.maxProcesses = GLOBAL.config.erizoAgent.maxProcesses || 1;
GLOBAL.config.erizoAgent.prerunProcesses = GLOBAL.config.erizoAgent.prerunProcesses === undefined ? 1 : GLOBAL.config.erizoAgent.prerunProcesses;
GLOBAL.config.erizoAgent.publicIP = GLOBAL.config.erizoAgent.publicIP || '';

var BINDED_INTERFACE_NAME = GLOBAL.config.erizoAgent.networkInterface;

// Parse command line arguments
var getopt = new Getopt([
  ['r' , 'rabbit-host=ARG'            , 'RabbitMQ Host'],
  ['g' , 'rabbit-port=ARG'            , 'RabbitMQ Port'],
  ['l' , 'logging-config-file=ARG'    , 'Logging Config File'],
  ['b' , 'rabbit-heartbeat=ARG'       , 'RabbitMQ AMQP Heartbeat Timeout'],
  ['M' , 'maxProcesses=ARG'           , 'Stun Server URL'],
  ['P' , 'prerunProcesses=ARG'        , 'Default video Bandwidth'],
  ['m' , 'metadata=ARG'               , 'JSON metadata'],
  ['h' , 'help'                       , 'display this help']
]);

opt = getopt.parse(process.argv.slice(2));

var metadata;

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
            case "metadata":
                metadata = JSON.parse(value);
            default:
                GLOBAL.config.erizoAgent[prop] = value;
                break;
        }
    }
}

// Load submodules with updated config
var logger = require('./../common/logger').logger;
var amqper = require('./../common/amqper');

// Logger
var log = logger.getLogger("ErizoAgent");

var childs = [];

var SEARCH_INTERVAL = 5000;

var idle_erizos = [];

var erizos = [];

var processes = {};

var guid = (function() {
  function s4() {
    return Math.floor((1 + Math.random()) * 0x10000)
               .toString(16)
               .substring(1);
  }
  return function() {
    return s4() + s4() + '-' + s4() + '-' + s4() + '-' +
           s4() + '-' + s4() + s4() + s4();
  };
})();

var my_erizo_agent_id = guid();

var saveChild = function(id) {
    childs.push(id);
};

var removeChild = function(id) {
    childs.push(id);
};

var launchErizoJS = function() {
    log.info("Launching a new ErizoJS process");
    var id = guid();
    var fs = require('fs');
    var out = fs.openSync('./erizo-' + id + '.log', 'a');
    var err = fs.openSync('./erizo-' + id + '.log', 'a');
    var erizoProcess = spawn('./launch.sh', ['./../erizoJS/erizoJS.js', id, privateIP, publicIP], { detached: true, stdio: [ 'ignore', out, err ] });
    erizoProcess.unref();
    erizoProcess.on('close', function (code) {
        log.info("ErizoJS", id, " has closed");
        var index = idle_erizos.indexOf(id);
        var index2 = erizos.indexOf(id);
        if (index > -1) {
            idle_erizos.splice(index, 1);
        } else if (index2 > -1) {
            erizos.splice(index2, 1);
        }
        if (out!=undefined){
            fs.close(out, function (message){
                if (message){
                    log.error("Error while closing log file", message);
                }
            }
            );
        }
        if(err!=undefined){
            fs.close(err, function (message){
                if (message){
                    log.error("Error while closing log file", message);
                }
            });
        }
        delete processes[id];
        fillErizos();       
    });

    log.info('Launched new ErizoJS ', id);
    processes[id] = erizoProcess;
    idle_erizos.push(id);
};

var dropErizoJS = function(erizo_id, callback) {
   if (processes.hasOwnProperty(erizo_id)) {
      log.warn("Dropping Erizo that was not closed before, possible publisher/subscriber mismatch");
      var process = processes[erizo_id];
      process.kill();
      delete processes[erizo_id];
      callback("callback", "ok");
   }
};

var fillErizos = function () {
    if (erizos.length + idle_erizos.length < GLOBAL.config.erizoAgent.maxProcesses) {
        if (idle_erizos.length < GLOBAL.config.erizoAgent.prerunProcesses) {
            launchErizoJS();
            fillErizos();
        }
    }
};

var cleanErizos = function () {
    log.info("Cleaning up all (killing) erizoJSs on SIGTERM or SIGINT", processes.length);
    for (var p in processes){
        log.info("killing process", processes[p].pid)
        processes[p].kill('SIGKILL');
    }
    process.exit(0);
};

var getErizo = function () {

    var erizo_id = idle_erizos.shift();

    if (!erizo_id) {
        if (erizos.length < GLOBAL.config.erizoAgent.maxProcesses) {
            launchErizoJS();
            return getErizo();
        } else {
            erizo_id = erizos.shift();
        }
    }

    return erizo_id;
}

// TODO: get metadata from a file
var reporter = require('./erizoAgentReporter').Reporter({id: my_erizo_agent_id, metadata: metadata});

var api = {
    createErizoJS: function(callback) {
        try {

            var erizo_id = getErizo();
            log.info('Get erizo JS', erizo_id);
            callback("callback", erizo_id);

            erizos.push(erizo_id);
            fillErizos();

        } catch (error) {
            console.log("Error in ErizoAgent:", error);
        }
    },
    deleteErizoJS: function(id, callback) {
        try {
            dropErizoJS(id, callback);
        } catch(err) {
            log.error("Error stopping ErizoJS");
        }
    },
    getErizoAgents: reporter.getErizoAgent
};

var interfaces = require('os').networkInterfaces(),
    addresses = [],
    k,
    k2,
    address, 
    privateIP, 
    publicIP;


for (k in interfaces) {
    if (interfaces.hasOwnProperty(k)) {
        for (k2 in interfaces[k]) {
            if (interfaces[k].hasOwnProperty(k2)) {
                address = interfaces[k][k2];
                if (address.family === 'IPv4' && !address.internal) {
                    if (k === BINDED_INTERFACE_NAME || !BINDED_INTERFACE_NAME) {
                        addresses.push(address.address);
                    }
                }
            }
        }
    }
}



privateIP = addresses[0];

if (GLOBAL.config.erizoAgent.publicIP === '' || GLOBAL.config.erizoAgent.publicIP === undefined){
    publicIP = addresses[0];
} else {
    publicIP = GLOBAL.config.erizoAgent.publicIP;
}

// Will clean all erizoJS on those signals
process.on('SIGINT', cleanErizos); 
process.on('SIGTERM', cleanErizos);

fillErizos();

amqper.connect(function () {
    "use strict";

    amqper.setPublicRPC(api);
    amqper.bind("ErizoAgent");
    amqper.bind("ErizoAgent_" + my_erizo_agent_id);
    amqper.bind_broadcast("ErizoAgent", function (m) {
        log.warn('No method defined');
    });
});
