const fs = require('fs');
const os = require('os');
const spawn = require('child_process').spawn;

const _ = require('lodash');

const config = require('config');
const opts = require('./options');

// Configuration default values
global.config = {};
global.config.erizoAgent = config.get('erizoAgent');

const BINDED_INTERFACE_NAME = global.config.erizoAgent.networkInterface;

const interfaces = os.networkInterfaces();
const addresses = [];
let address;
let privateIP;
let publicIP;

opts.processArgv();

// Load submodules with updated config
const logger = require('./common/logger').logger;
const amqper = require('./common/amqper');

const db = require('./database').db;
const metrics = require('./metrics');

const log = logger.getLogger('ErizoAgent');

const idleErizos = [];

const erizos = [];

const processes = {};

const guid = (() => {
    const s4 = () => Math.floor((1 + Math.random()) * 0x10000).toString(16).substring(1);
    return () => `${s4()}${s4()}-${s4()}-${s4()}-${s4()}-${s4()}${s4()}${s4()}`;
})();

const myErizoAgentId = guid();

/**
 * Starts new instance of ErizoJS and adds it to idleErizos.
 *
 * The process will automatically handle its death, remove itself from all collections
 * and run fillErizos.
 */
let launchErizoJS;

/**
 * Ensure there are some idle erizos ready to be used by incoming publishers.
 *
 * Won't launch new erizos if total erizo count reached <code>maxProcesses</code> config.
 */
const fillErizos = () => {
    while (erizos.length + idleErizos.length < global.config.erizoAgent.maxProcesses
           && idleErizos.length < global.config.erizoAgent.prerunProcesses) {
        launchErizoJS();
    }
};

launchErizoJS = () => {
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

        _.pull(idleErizos, id);
        _.pull(erizos, id);
        delete processes[id];

        if (out !== undefined) {
            fs.close(out, (message) => {
                if (message) {
                    log.error(`'message: error closing log file, erizoId: ${id}, error:`, message);
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

        fillErizos();
    });

    log.info(`message: launched new ErizoJS, erizoId: ${id}`);

    processes[id] = erizoProcess;
    idleErizos.push(id);
};

const dropErizoJS = (erizoId, callback) => {
    if ({}.hasOwnProperty.call(processes, erizoId)) {
        log.warn(`message: Dropping Erizo that was not closed before possible publisher/subscriber mismatch, erizoId: ${erizoId}`);
        processes[erizoId].kill();

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

const cleanErizos = () => {
    log.debug(`message: killing erizoJSs on close, numProcesses: ${processes.length}`);
    _.values(processes).forEach((p) => {
        log.debug(`message: killing process, processId: ${p.pid}`);
        p.kill('SIGKILL');
    });
    process.exit(0);
};

const getErizo = () => {
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
const reporter = require('./erizoAgentReporter').Reporter({
    id: myErizoAgentId,
    metadata: opts.metadata
});

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

for (const k in interfaces) {
    if (interfaces.hasOwnProperty(k)) {
        if (!global.config.erizoAgent.networkinterface ||
            global.config.erizoAgent.networkinterface === k) {
            for (const k2 in interfaces[k]) {
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

metrics.startReporter(myErizoAgentId);

amqper.connect(() => {
    amqper.setPublicRPC(api);
    amqper.bind('ErizoAgent');
    amqper.bind(`ErizoAgent_${myErizoAgentId}`);
    amqper.bindBroadcast('ErizoAgent', () => {
        log.warn('message: amqp no method defined');
    });
});
