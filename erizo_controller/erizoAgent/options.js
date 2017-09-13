const _ = require('lodash');
const Getopt = require('node-getopt');

// Parse command line arguments
const getopt = new Getopt([
    ['r', 'rabbit-host=ARG', 'RabbitMQ Host'],
    ['g', 'rabbit-port=ARG', 'RabbitMQ Port'],
    ['b', 'rabbit-heartbeat=ARG', 'RabbitMQ AMQP Heartbeat Timeout'],
    ['l', 'logging-config-file=ARG', 'Logging Config File'],
    ['M', 'max-processes=ARG', 'Stun Server URL'],
    ['P', 'prerun-processes=ARG', 'Default video Bandwidth'],
    ['I', 'individual-logs', 'Use individual log files for ErizoJS processes'],
    ['m', 'metadata=ARG', 'JSON metadata'],
    ['h', 'help', 'display this help']
]);

exports.metadata = undefined;

exports.processArgv = () => {
    const opt = getopt.parse(process.argv.slice(2));
    _.forOwn(opt.options, (value, prop) => {
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
        case 'max-processes':
            global.config.erizoAgent = global.config.erizoAgent || {};
            global.config.erizoAgent.maxProcesses = value;
            break;
        case 'prerun-processes':
            global.config.erizoAgent = global.config.erizoAgent || {};
            global.config.erizoAgent.prerunProcesses = value;
            break;
        case 'individual-logs':
            global.config.erizoAgent = global.config.erizoAgent || {};
            global.config.erizoAgent.useIndividualLogFiles = true;
            break;
        case 'metadata':
            exports.metadata = JSON.parse(value);
            break;
        default:
            global.config.erizoAgent[prop] = value;
            break;
        }
    });
};
