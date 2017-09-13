const assert = require('assert');
const fs = require('fs');
const os = require('os');
const spawn = require('child_process').spawn;

const _ = require('lodash');
const config = require('config');

const logger = require('./common/logger').logger;

const db = require('./database').db;

const log = logger.getLogger('ErizoAgent');

/** Queue of idle erizos waiting for incoming publishers. */
const idleErizos = [];

/** Round robin queue of busy erizos already handling some publishers. */
const busyErizos = [];

/** ErizoJS ID -> Process Handle map */
const processes = {};

let privateIP;
let publicIP;

const guid = (() => {
    const s4 = () => Math.floor((1 + Math.random()) * 0x10000).toString(16).substring(1);
    return () => `${s4()}${s4()}-${s4()}-${s4()}-${s4()}-${s4()}${s4()}${s4()}`;
})();

const myErizoAgentId = guid();

/**
 * Gets next candidate ErizoJS for incoming publisher.
 *
 * Following algorithm is used when picking erizo:
 * 1. If there is idle erizo in queue, than it is popped from it and returned
 * 2. If there are no idle erizos, but there are less than <code>maxProcesses</code>
 *    running busy erizos, a new erizo is launched and returned
 * 3. Otherwise, first busy erizo from round robin queue is popped and returned
 *
 * @returns {*} ErizoJS ID
 */
function getErizo() {
    const idleErizo = idleErizos.shift();
    if (idleErizo) return idleErizo;

    if (busyErizos.length < global.config.erizoAgent.maxProcesses) {
        launchErizoJS();
        return getErizo();
    }

    return busyErizos.shift();
}

/**
 * Ensure there are some idle erizos ready to be used by incoming publishers.
 *
 * Won't launch new erizos if total erizo count reached <code>maxProcesses</code> config.
 */
function fillErizos() {
    while (busyErizos.length + idleErizos.length < global.config.erizoAgent.maxProcesses
           && idleErizos.length < global.config.erizoAgent.prerunProcesses) {
        launchErizoJS();
    }
}

/**
 * Starts new instance of ErizoJS and adds it to idleErizos.
 *
 * The process will automatically handle its death, remove itself from all collections
 * and run fillErizos.
 */
function launchErizoJS() {
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
        _.pull(busyErizos, id);
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
}

module.exports = {
    erizoAgentId: myErizoAgentId,

    /**
     * Starts ErizoJS manager.
     */
    start() {
        const address = _.entries(os.networkInterfaces())
            .filter(([k, v]) => !global.config.erizoAgent.networkinterface
                                || k === global.config.erizoAgent.networkinterface)
            .map(([k, v]) => v)
            .reduce((x, y) => x.concat(y), [])
            .filter(addr => addr.family === 'IPv4' && !addr.internal)
            .map(addr => addr.address)[0];

        assert(address);

        privateIP = address;

        if (global.config.erizoAgent.publicIP === '' || global.config.erizoAgent.publicIP === undefined) {
            publicIP = address;

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
    },

    createErizoJS(callback) {
        const erizoId = getErizo();
        log.debug(`message: createErizoJS returning, erizoId: ${erizoId}`);
        callback('callback', { erizoId, agentId: myErizoAgentId });

        busyErizos.push(erizoId);
        fillErizos();
    },

    dropErizoJS(erizoId, callback) {
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
    },

    /**
     * Brutally kills all running erizos.
     */
    cleanErizos() {
        log.debug(`message: killing erizoJSs on close, numProcesses: ${processes.length}`);
        _.values(processes).forEach((p) => {
            log.debug(`message: killing process, processId: ${p.pid}`);
            p.kill('SIGKILL');
        });
    }
};
