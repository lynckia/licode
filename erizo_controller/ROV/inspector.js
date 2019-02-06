/* global require */
/* eslint-disable  no-console */

// eslint-disable-next-line import/no-extraneous-dependencies
const Getopt = require('node-getopt');

// eslint-disable-next-line import/no-unresolved
const config = require('./../../licode_config');
// Configuration default values
global.config = config || {};

// Parse command line arguments
const getopt = new Getopt([
  ['r', 'rabbit-host=ARG', 'RabbitMQ Host'],
  ['g', 'rabbit-port=ARG', 'RabbitMQ Port'],
  ['b', 'rabbit-heartbeat=ARG', 'RabbitMQ AMQP Heartbeat Timeout'],
  ['l', 'logging-config-file=ARG', 'Logging Config File'],
  ['h', 'help', 'display this help'],
]);

const opt = getopt.parse(process.argv.slice(2));


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
    default:
      global.config.erizoAgent[prop] = value;
      break;
  }
});

// Load submodules with updated config
const logger = require('./../common/logger').logger;
const amqper = require('./../common/amqper');


const nuveProxy = (spec) => {
  const that = {};

  const callNuve = (method, additionalErrors, args) => new Promise((resolve, reject) => {
    try {
      spec.amqper.callRpc('nuve', method, args, { callback(response) {
        const errors = ['timeout', 'error'].concat(additionalErrors);
        if (errors.indexOf(response) !== -1) {
          reject(response);
          return;
        }
        resolve(response);
      } });
    } catch (err) {
      console.error('message: Error calling RPC', err);
    }
  });

  that.getErizoControllers = () => callNuve('getErizoControllers', [], undefined);

  return that;
};

const Helpers = {
  removeSingleQuote: (aString) => {
    return aString.replace(/'/g, '');
  },
};


class RovConnection {
  constructor(rpcId, amqp) {
    this.rpcId = rpcId;
    this.isConnected = false;
    this.sessionId = false;
    this.amqper = amqp;
  }
  _rpcCall(id, method, additionalErrors, args) {
    return new Promise((resolve, reject) => {
      try {
        console.log('message: Will call', id, 'method', method, 'args', args);
        this.amqper.callRpc(id, method, args, { callback(response) {
          const errors = ['timeout', 'error'].concat(additionalErrors);
          if (errors.indexOf(response) !== -1) {
            reject(response);
            return;
          }
          resolve(response);
        } });
      } catch (err) {
        console.error('message: Error calling RPC', err);
      }
    });
  }

  connect() {
    return new Promise((resolve, reject) => {
      this._rpcCall(this.rpcId, 'rovMessage', [], [{ action: 'open' }]).then((sessionId) => {
        this.sessionId = sessionId;
        resolve(sessionId);
      }).catch((error) => {
        console.log('message: error connecting: ', error);
        reject(error);
      });
    });
  }

  run(command) {
    // run remote command
    return this._rpcCall(this.rpcId, 'rovMessage', [], [{ action: 'command', id: this.sessionId, message: command }]);
  }
  close() {
    // close remote repl
    return this._rpcCall(this.rpcId, 'rovMessage', [], [{ action: 'close', id: this.sessionId }]);
  }
}

class Inspector {
  constructor() {
    amqper.connect(() => {
    });
    this.sessions = new Map();
    this.nuveProxy = nuveProxy({ amqper });
    this.components = {
      erizoControllers: [],
      erizoAgents: [],
      erizoJS: [],
    };
  }
  // getErizoControllerIdList() {
  //   this.componentProxy.getErizoControllers().then((erizoControllers) => {
  //     erizoControllers.forEach((controller) => {
  //       console.log(`Id: ${controller._id}, IP: ${controller.ip}`);
  //     });
  //   }).catch((error) => {
  //     console.error('Error is', error);
  //   });
  // }
  // getRooms() {
  //   this.nuve.getRooms((roomList) => {
  //     console.log('RoomList is ', roomList);
  //   });
  // }

  // getUsersInRoom(controllerId) {
  //   this.componentProxy.getUsersByRoom(this.erizoControllerId, controllerId).then((users) => {
  //     console.log('users are', users);
  //   });
  // }

  _setRovCallsForComponent(component) {
    const componentWithCalls = component;
    componentWithCalls.connect = this.connectToSession.bind(this, componentWithCalls.rpcId);
    componentWithCalls.run = this.runInSession.bind(this, componentWithCalls.rpcId);
    componentWithCalls.runAndGetPromise =
      this.runInSessionWithPromise.bind(this, componentWithCalls.rpcId);
    componentWithCalls.close = this.closeSession.bind(this, componentWithCalls.rpcId);
    return componentWithCalls;
  }

  getErizoControllers() {
    this.nuveProxy.getErizoControllers().then((controllers) => {
      this.components.erizoControllers = [];
      controllers.forEach((erizoController) => {
        let erizoControllerEntry = {};
        erizoControllerEntry.rpcId = `erizoController_${erizoController._id}`;
        erizoControllerEntry.componentType = 'erizoController';
        erizoControllerEntry = this._setRovCallsForComponent(erizoControllerEntry);
        this.components.erizoControllers.push(erizoControllerEntry);
      });
    });
  }

  getErizoAgents() {
    if (this.components.erizoControllers.length === 0) {
      console.log('No erizoController present to get all ErizoAgents');
      return;
    }
    const controller = this.components.erizoControllers[0];
    this.components.erizoAgents = [];
    controller.connect().then(() => {
      controller.runAndGetPromise('context.ecch.getErizoAgentsList()').then((agentsJson) => {
        const agentsJsonParsed = Helpers.removeSingleQuote(agentsJson);
        const agents = JSON.parse(agentsJsonParsed);
        const agentIds = Object.keys(agents);
        for (let i = 0; i < agentIds.length; i += 1) {
          let agentEntry = Object.assign({}, agents[agentIds[i]].info);
          agentEntry.componentType = 'erizoAgent';
          agentEntry.rpcId = agentEntry.rpc_id;
          agentEntry = this._setRovCallsForComponent(agentEntry);
          this.components.erizoAgents.push(agentEntry);
        }
      });
    });
  }
  getErizoJSProcesses() {
    if (this.components.erizoAgents.length === 0) {
      console.log('No erizoAgent sessions present to get all ErizoJS processes');
      return;
    }
    this.components.erizoAgents.forEach((agent) => {
      agent.connect().then(() => {
        agent.runAndGetPromise('context.toJSON()').then((erizoJsJSON) => {
          console.log('ERizoJSJSON', erizoJsJSON);
          const erizoJsParsed = Helpers.removeSingleQuote(erizoJsJSON);
          const erizoJsProcesses = JSON.parse(erizoJsParsed);
          erizoJsProcesses.forEach((process) => {
            let erizoJsEntry = Object.assign({}, process);
            erizoJsEntry.componentType = 'erizoJS';
            erizoJsEntry.rpcId = `ErizoJS_${process.id}`;
            erizoJsEntry = this._setRovCallsForComponent(erizoJsEntry);
            this.components.erizoJS.push(erizoJsEntry);
          });
        });
      });
    });
  }

  listSessions() {
    return this.sessions;
  }

  connectToSession(componentId) {
    return new Promise((resolve, reject) => {
      if (this.sessions.has(componentId)) {
        resolve();
      }
      const rovConnection = new RovConnection(componentId, amqper);
      rovConnection.connect().then(() => {
        this.sessions.set(componentId, rovConnection);
        console.log(`Connection established and ready to ${componentId}`);
        resolve();
      }).catch((error) => {
        reject(error);
      });
    });
  }

  runInSession(sessionId, command) {
    this.runInSessionWithPromise(sessionId, command).then((result) => {
      console.log(result);
    });
  }
  runInSessionWithPromise(sessionId, command) {
    const rovSession = this.sessions.get(sessionId);
    if (!rovSession) {
      console.error(`Session ${sessionId} does not exist, trying to run ${command}`);
      return false;
    }
    return rovSession.run(command);
  }

  closeSession(sessionId) {
    const rovSession = this.sessions.get(sessionId);
    if (!rovSession) {
      console.error(`Cannot close session, id: ${sessionId} does not exist`);
    }
    rovSession.close().then(() => {
      this.sessions.delete(rovSession);
    });
  }
  closeAllSessions() {
    console.log('Inspector closing all sessions');
    this.sessions.forEach((session) => {
      session.close();
    });
    this.sessions.clear();
  }
}

const repl = require('repl');

const inspector = new Inspector();
const r = repl.start({
  prompt: '> ',
  ignoreUndefined: true,
  useColors: true,
});

r.on('exit', () => {
  inspector.closeAllSessions();
  process.exit(0);
});

Object.defineProperty(r.context, 'inspector', {
  configurable: false,
  enumerable: true,
  value: inspector,
});
