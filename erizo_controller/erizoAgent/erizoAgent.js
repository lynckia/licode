/* global require */

// eslint-disable-next-line import/no-extraneous-dependencies
const Getopt = require('node-getopt');

const spawn = require('child_process').spawn;

// eslint-disable-next-line import/no-unresolved
const config = require('./../../licode_config');
const ErizoList = require('./erizoList').ErizoList;
const fs = require('fs');
const RovReplManager = require('./../common/ROV/rovReplManager').RovReplManager;

// Configuration default values
global.config = config || {};
global.config.erizoAgent = global.config.erizoAgent || {};
global.config.erizoAgent.maxProcesses = global.config.erizoAgent.maxProcesses || 1;
global.config.erizoAgent.prerunProcesses = global.config.erizoAgent.prerunProcesses === undefined
  ? 1 : global.config.erizoAgent.prerunProcesses;
global.config.erizoAgent.publicIP = global.config.erizoAgent.publicIP || '';
global.config.erizoAgent.instanceLogDir = global.config.erizoAgent.instanceLogDir || '.';
global.config.erizoAgent.useIndividualLogFiles =
  global.config.erizoAgent.useIndividualLogFiles || false;

global.config.erizoAgent.launchDebugErizoJS = global.config.erizoAgent.launchDebugErizoJS || false;

const BINDED_INTERFACE_NAME = global.config.erizoAgent.networkInterface;
const LAUNCH_SCRIPT = './launch.sh';

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
  ['d', 'debug', 'Run erizoJS with debug library'],
  ['h', 'help', 'display this help'],
]);

const interfaces = require('os').networkInterfaces();


const addresses = [];

let privateIP;
let publicIP;
let address;

const opt = getopt.parse(process.argv.slice(2));

let metadata;

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
    case 'debug':
      global.config.erizoAgent.launchDebugErizoJS = true;
      break;
    default:
      global.config.erizoAgent[prop] = value;
      break;
  }
});

// Load submodules with updated config
const logger = require('./../common/logger').logger;
const amqper = require('./../common/amqper');

// Logger
const log = logger.getLogger('ErizoAgent');

const erizos = new ErizoList(global.config.erizoAgent.prerunProcesses,
  global.config.erizoAgent.maxProcesses);

const guid = (function guid() {
  function s4() {
    return Math.floor((1 + Math.random()) * 0x10000)
      .toString(16)
      .substring(1);
  }
  return function id() {
    return `${s4() + s4()}-${s4()}-${s4()}-${
      s4()}-${s4()}${s4()}${s4()}`;
  };
}());

const replManager = new RovReplManager(erizos);

const printErizoLogMessage = (id, message) => {
  // eslint-disable-next-line no-console
  console.log(id, message);
};

const myErizoAgentId = guid();

const launchErizoJS = (erizo) => {
  const id = erizo.id;
  log.debug(`message: launching ErizoJS, erizoId: ${id}`);
  let erizoProcess; let out; let
    err;
  const erizoLaunchOptions = ['./../erizoJS/erizoJS.js', id, privateIP, publicIP];
  if (global.config.erizoAgent.launchDebugErizoJS) {
    erizoLaunchOptions.push('-d');
  }

  if (global.config.erizoAgent.useIndividualLogFiles) {
    out = fs.openSync(`${global.config.erizoAgent.instanceLogDir}/erizo-${id}.log`, 'a');
    err = fs.openSync(`${global.config.erizoAgent.instanceLogDir}/erizo-${id}.log`, 'a');
    erizoProcess = spawn(LAUNCH_SCRIPT, erizoLaunchOptions,
      { detached: true, stdio: ['ignore', out, err] });
  } else {
    erizoProcess = spawn(LAUNCH_SCRIPT, erizoLaunchOptions,
      { detached: true, stdio: ['ignore', 'pipe', 'pipe'] });
    erizoProcess.stdout.setEncoding('utf8');
    erizoProcess.stdout.on('data', (message) => {
      printErizoLogMessage(`[erizo-${id}]`, message.replace(/\n$/, ''));
    });
    erizoProcess.stderr.setEncoding('utf8');
    erizoProcess.stderr.on('data', (message) => {
      printErizoLogMessage(`[erizo-${id}]`, message.replace(/\n$/, ''));
    });
  }
  erizoProcess.unref();
  erizoProcess.on('close', () => {
    log.info(`message: closed, erizoId: ${id}`);
    erizos.delete(id);

    if (out !== undefined) {
      fs.close(out, (message) => {
        if (message) {
          log.error('message: error closing log file, ',
            `erizoId: ${id}`, 'error:', message);
        }
      });
    }

    if (err !== undefined) {
      fs.close(err, (message) => {
        if (message) {
          log.error('message: error closing log file, ',
            `erizoId: ${id}`, 'error:', message);
        }
      });
    }
    erizos.fill();
  });

  log.info(`message: launched new ErizoJS, erizoId: ${id}`);
  // eslint-disable-next-line no-param-reassign
  erizo.process = erizoProcess;
};

erizos.on('launch-erizo', launchErizoJS);

const dropErizoJS = (erizoId, callback) => {
  const process = erizos.delete(erizoId);
  if (process) {
    log.warn('message: Dropping Erizo that was not closed before - ' +
               `possible publisher/subscriber mismatch, erizoId: ${erizoId}`);
    process.kill();
    callback('callback', 'ok');
  }
};

const cleanErizos = () => {
  log.debug(`message: killing erizoJSs on close, numProcesses: ${erizos.running.length}`);
  erizos.forEach((erizo) => {
    const process = erizo.process;
    if (process) {
      log.debug(`message: killing process, processId: ${process.pid}`);
      process.kill('SIGKILL');
    }
  });
  erizos.clear();
  process.exit(0);
};

// TODO: get metadata from a file
const reporter = require('./erizoAgentReporter').Reporter({ id: myErizoAgentId, metadata });

const api = {
  createErizoJS(internalId, callback) {
    try {
      const erizo = erizos.getErizo(internalId);
      log.debug(`message: createErizoJS returning, erizoId: ${erizo.id} ` +
        `agentId: ${myErizoAgentId}, internalId: ${erizo.position}`);
      callback('callback',
        { erizoId: erizo.id, agentId: myErizoAgentId, internalId: erizo.position });
      erizos.fill();
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
  rovMessage: (args, callback) => {
    replManager.processRpcMessage(args, callback);
  },
  getErizoAgents: reporter.getErizoAgent,
};

if (interfaces) {
  Object.keys(interfaces).forEach((k) => {
    if (!global.config.erizoAgent.networkinterface ||
      global.config.erizoAgent.networkinterface === k) {
      Object.keys(interfaces[k]).forEach((k2) => {
        address = interfaces[k][k2];
        if (address.family === 'IPv4' && !address.internal) {
          if (k === BINDED_INTERFACE_NAME || !BINDED_INTERFACE_NAME) {
            addresses.push(address.address);
          }
        }
      });
    }
  });
}

privateIP = addresses[0];

if (global.config.erizoAgent.publicIP === '' || global.config.erizoAgent.publicIP === undefined) {
  publicIP = addresses[0];

  if (global.config.cloudProvider.name === 'amazon') {
    // eslint-disable-next-line
    const AWS = require('aws-sdk');
    new AWS.MetadataService({
      httpOptions: {
        timeout: 5000,
      },
    }).request('/latest/meta-data/public-ipv4', (err, data) => {
      if (err) {
        log.info('Error: ', err);
      } else {
        log.info('Got public ip: ', data);
        publicIP = data;
        erizos.fill();
      }
    });
  } else {
    erizos.fill();
  }
} else {
  publicIP = global.config.erizoAgent.publicIP;
  erizos.fill();
}

// Will clean all erizoJS on those signals
process.on('SIGINT', cleanErizos);
process.on('SIGTERM', cleanErizos);

amqper.connect(() => {
  amqper.setPublicRPC(api);
  amqper.bind('ErizoAgent');
  amqper.bind(`ErizoAgent_${myErizoAgentId}`);
  amqper.bindBroadcast('ErizoAgent', () => {
    log.warn('message: amqp no method defined');
  });
});
