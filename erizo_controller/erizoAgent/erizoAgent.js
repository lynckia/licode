const fs = require('fs');
const os = require('os');
const spawn = require('child_process').spawn;
const util = require('util');

const Getopt = require('node-getopt');
const _ = require('lodash');

const config = require('config');
const db = require('./database').db;

const graphite = require('./common/graphite');

// Configuration default values
global.config = {};
global.config.erizoAgent = config.get('erizoAgent');

const BINDED_INTERFACE_NAME = global.config.erizoAgent.networkInterface;

const interfaces = os.networkInterfaces();
const addresses = [];
let k;
let k2;
let address;
let privateIP;
let publicIP;
let metadata;

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

const opt = getopt.parse(process.argv.slice(2));
_.forOwn(opt.options).forEach((value, prop) => {
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
        metadata = JSON.parse(value);
        break;
    default:
        global.config.erizoAgent[prop] = value;
        break;
    }
});

// Load submodules with updated config
const logger = require('./common/logger').logger;
const amqper = require('./common/amqper');

// Logger
const log = logger.getLogger('ErizoAgent');

const idleErizos = [];

const erizos = [];

const processes = {};

const guid = (() => {
    const s4 = () => Math.floor((1 + Math.random()) * 0x10000).toString(16).substring(1);
    return () => `${s4()}${s4()}-${s4()}-${s4()}-${s4()}-${s4()}${s4()}${s4()}`;
})();

const myErizoAgentId = guid();

let launchErizoJS;

const fillErizos = function () {
    if (erizos.length + idleErizos.length < global.config.erizoAgent.maxProcesses) {
        if (idleErizos.length < global.config.erizoAgent.prerunProcesses) {
            launchErizoJS();
            fillErizos();
        }
    }
};

launchErizoJS = function () {
    const id = guid();
    let erizoProcess;
    let out;
    let err;

    log.debug(`message: launching ErizoJS, erizoId: ${id}`);
    if (global.config.erizoAgent.useIndividualLogFiles) {
        out = fs.openSync(`${global.config.erizoAgent.instanceLogDir}/erizo-${id}.log`, 'a');
        err = fs.openSync(`${global.config.erizoAgent.instanceLogDir}/erizo-${id}.log`, 'a');
        erizoProcess = spawn('./launch.sh',
            ['erizoJS.js', myErizoAgentId, id, privateIP, publicIP],
            { detached: true, stdio: ['ignore', out, err] });
    } else {
        erizoProcess = spawn('./launch.sh',
            ['erizoJS.js', myErizoAgentId, id, privateIP, publicIP],
            { detached: true, stdio: ['ignore', 'pipe', 'pipe'] });
        erizoProcess.stdout.setEncoding('utf8');
        erizoProcess.stdout.on('data', (message) => {
            console.log(`[erizo-${id}]`, message.replace(/\n$/, ''));
        });
        erizoProcess.stderr.setEncoding('utf8');
        erizoProcess.stderr.on('data', (message) => {
            console.log(`[erizo-${id}]`, message.replace(/\n$/, ''));
        });
    }
    erizoProcess.unref();
    erizoProcess.on('close', () => {
        log.info(`message: closed, erizoId: ${id}`);
        const index = idleErizos.indexOf(id);
        const index2 = erizos.indexOf(id);
        if (index > -1) {
            idleErizos.splice(index, 1);
        } else if (index2 > -1) {
            erizos.splice(index2, 1);
        }

        if (out !== undefined) {
            fs.close(out, (message) => {
                if (message) {
                    log.error(`${'message: error closing log file, ' +
                    'erizoId: '}${id}, error:`, message);
                }
            });
        }

        if (err !== undefined) {
            fs.close(err, (message) => {
                if (message) {
                    log.error(`message: error closing log file, erizoId: ${id}, error:`, message);
                }
            });
        }
        delete processes[id];
        fillErizos();
    });

    log.info(`message: launched new ErizoJS, erizoId: ${id}`);
    processes[id] = erizoProcess;
    idleErizos.push(id);
};

const dropErizoJS = function (erizoId, callback) {
    if ({}.hasOwnProperty.call(processes, erizoId)) {
        log.warn(`message: Dropping Erizo that was not closed before possible publisher/subscriber mismatch, erizoId:${erizoId}`);
        const process = processes[erizoId];
        process.kill();
        delete processes[erizoId];

        db.erizoJS.remove({
            erizoAgentID: myErizoAgentId,
            erizoJSID: erizoId,
        }, (error) => {
            if (error) {
                log.warn(`message: remove erizoJS error, ${logger.objectToLog(error)}`);
            }
            callback('callback', 'ok');
        });
    }
};

const cleanErizos = function () {
    log.debug(`message: killing erizoJSs on close, numProcesses: ${processes.length}`);
    _.values(processes).forEach((p) => {
        log.debug(`message: killing process, processId: ${p.pid}`);
        p.kill('SIGKILL');
    });
    process.exit(0);
};

const getErizo = function () {
    let erizoId = idleErizos.shift();

    if (!erizoId) {
        if (erizos.length < global.config.erizoAgent.maxProcesses) {
            launchErizoJS();
            return getErizo();
        }
        erizoId = erizos.shift();
    }

    return erizoId;
};

// TODO: get metadata from a file
const reporter = require('./erizoAgentReporter').Reporter({ id: myErizoAgentId, metadata });

const api = {
    createErizoJS(callback) {
        try {
            const erizoId = getErizo();
            log.debug(`message: createErizoJS returning, erizoId: ${erizoId}`);
            callback('callback', { erizoId, agentId: myErizoAgentId });

            erizos.push(erizoId);
            fillErizos();
        } catch (error) {
            log.error('message: error creating ErizoJS, error:', error);
        }
    },
    deleteErizoJS(id, callback) {
        try {
            dropErizoJS(id, callback);
        } catch (err) {
            log.error('message: error stopping ErizoJS');
        }
    },
    getErizoAgents: reporter.getErizoAgent
};

for (k in interfaces) {
    if (interfaces.hasOwnProperty(k)) {
        if (!global.config.erizoAgent.networkinterface ||
            global.config.erizoAgent.networkinterface === k) {
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
}

privateIP = addresses[0];

if (global.config.erizoAgent.publicIP === '' || global.config.erizoAgent.publicIP === undefined) {
    publicIP = addresses[0];

    if (config.get('cloudProvider.name') === 'amazon') {
        const AWS = require('aws-sdk'); // eslint-disable-line
        new AWS.MetadataService({
            httpOptions: {
                timeout: 5000
            }
        }).request('/latest/meta-data/public-ipv4', (err, data) => {
            if (err) {
                log.info('Error: ', err);
            } else {
                log.info('Got public ip: ', data);
                publicIP = data;
                fillErizos();
            }
        });
    } else {
        fillErizos();
    }
} else {
    publicIP = global.config.erizoAgent.publicIP;
    fillErizos();
}

// Will clean all erizoJS on those signals
process.on('SIGINT', cleanErizos);
process.on('SIGTERM', cleanErizos);

const reportMetrics = () => {
    const now = Date.now();
    const interval = global.config.erizoAgent.statsUpdateInterval * 2;

    db.erizoJS.aggregate(
        [
            {
                $match: {
                    erizoAgentID: myErizoAgentId,
                    lastUpdated: { $lte: new Date(now), $gte: new Date(now - interval) }
                }
            },
            { $sort: { lastUpdated: -1 } },
            {
                $group: {
                    _id: { erizoAgentID: '$erizoAgentID', erizoJSID: '$erizoJSID' },
                    publishersCount: { $first: '$publishersCount' },
                    subscribersCount: { $first: '$subscribersCount' },
                    stats: { $first: '$stats' }
                }
            },
            {
                $group: {
                    _id: '$_id.erizoAgentID',

                    publishersCount: { $sum: '$publishersCount' },
                    subscribersCount: { $sum: '$subscribersCount' },

                    video_fractionLost_min: { $min: '$stats.video.fractionLost.min' },
                    video_fractionLost_max: { $max: '$stats.video.fractionLost.max' },
                    video_fractionLost_total: { $sum: '$stats.video.fractionLost.total' },
                    video_fractionLost_count: { $sum: '$stats.video.fractionLost.count' },

                    video_jitter_min: { $min: '$stats.video.jitter.min' },
                    video_jitter_max: { $max: '$stats.video.jitter.max' },
                    video_jitter_total: { $sum: '$stats.video.jitter.total' },
                    video_jitter_count: { $sum: '$stats.video.jitter.count' }
                }
            }
        ],
        (err, docs) => {
            if (err) {
                log.warn('failed to collect erizoAgent metrics', err);
                return;
            }

            if (docs.length === 0) {
                return;
            }

            if (docs.length !== 1) {
                log.error(`expected single document result, got ${util.inspect(docs)}`);
                return;
            }

            const d = docs[0];

            graphite.put('publishers.count', d.publishersCount);
            graphite.put('subscribers.count', d.subscribersCount);

            if (d.video_fractionLost_count > 0) {
                graphite.put('video.fractionLost.min', d.video_fractionLost_min);
                graphite.put('video.fractionLost.max', d.video_fractionLost_max);
                graphite.put('video.fractionLost.avg', d.video_fractionLost_total / d.video_fractionLost_count);
            }

            if (d.video_jitter_count > 0) {
                graphite.put('video.jitter.min', d.video_jitter_min);
                graphite.put('video.jitter.max', d.video_jitter_max);
                graphite.put('video.jitter.avg', d.video_jitter_total / d.video_jitter_count);
            }

            log.debug('submitted erizoAgent metrics');
        }
    );
};

setInterval(reportMetrics, config.erizoAgent.statsUpdateInterval);

amqper.connect(() => {
    amqper.setPublicRPC(api);
    amqper.bind('ErizoAgent');
    amqper.bind(`ErizoAgent_${myErizoAgentId}`);
    amqper.bindBroadcast('ErizoAgent', () => {
        log.warn('message: amqp no method defined');
    });
});
