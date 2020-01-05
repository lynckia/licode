/* global require */
/* eslint-disable  class-methods-use-this */

const logger = require('./../common/logger').logger;

const log = logger.getLogger('RovClient');

class NuveProxy {
  constructor(amqper) {
    this.amqper = amqper;
  }

  callNuve(method, additionalErrors, args) {
    return new Promise((resolve, reject) => {
      try {
        this.amqper.callRpc('nuve', method, args, { callback(response) {
          const errors = ['timeout', 'error'].concat(additionalErrors);
          if (errors.indexOf(response) !== -1) {
            reject(response);
            return;
          }
          resolve(response);
        } });
      } catch (err) {
        log.error('message: Error calling RPC', err);
      }
    });
  }

  getErizoControllers() {
    return this.callNuve('getErizoControllers', [], undefined);
  }
}

class RovConnection {
  constructor(rpcId, amqp) {
    this.rpcId = rpcId;
    this.isConnected = false;
    this.sessionId = false;
    this.amqper = amqp;
  }
  _getErizoControllersFromNuve() {
    return this._rpcCall('nuve', 'getErizoControllers', [], undefined);
  }

  _rpcCall(id, method, additionalErrors, args) {
    return new Promise((resolve, reject) => {
      try {
        this.amqper.callRpc(id, method, args, { callback(response) {
          const errors = ['timeout', 'error'].concat(additionalErrors);
          if (errors.indexOf(response) !== -1) {
            log.error('Failed rpc call', id, method, args);
            reject(response);
            return;
          }
          resolve(response);
        } });
      } catch (err) {
        log.error('message: Error calling RPC', err);
      }
    });
  }

  connect() {
    return new Promise((resolve, reject) => {
      this._rpcCall(this.rpcId, 'rovMessage', [], [{ action: 'open' }]).then((sessionId) => {
        this.sessionId = sessionId;
        this.isConnected = true;
        resolve(sessionId);
      }).catch((error) => {
        log.error('message: error connecting: ', error);
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
    return new Promise((resolve, reject) => {
      this._rpcCall(this.rpcId, 'rovMessage', [], [{ action: 'close', id: this.sessionId }])
        .then(() => {
          log.debug(`Connection ${this.rpcId} is closed`);
          this.isConnected = false;
          resolve();
        })
        .catch(() => {
          this.isConnected = false;
          reject();
        });
    });
  }
}

class RovClient {
  constructor(amqper) {
    this.amqper = amqper;
    this.sessions = new Map();
    this.nuveProxy = new NuveProxy(this.amqper);
    this.components = {
      erizoControllers: new Map(), // erizoControllers: { id: info }
      erizoAgents: new Map(),
      erizoJS: new Map(),
    };
    this.defaultSession = undefined;
  }

  _setRovCallsForComponent(component) {
    const componentWithCalls = component;
    componentWithCalls.connect = this.connectToSession.bind(this, componentWithCalls.rpcId);
    componentWithCalls.run = this.runInSession.bind(this, componentWithCalls.rpcId);
    componentWithCalls.runAndGetPromise =
      this.runInSessionWithPromise.bind(this, componentWithCalls.rpcId);
    componentWithCalls.close = this.closeSession.bind(this, componentWithCalls.rpcId);
    componentWithCalls.openRemoteSession =
      this.setDefaultSession.bind(this, componentWithCalls.rpcId);
    return componentWithCalls;
  }

  _establishComponentConnections(componentList) {
    return new Promise((resolve, reject) => {
      const connectPromises = [];
      componentList.forEach((component) => {
        connectPromises.push(component.connect());
      });
      Promise.all(connectPromises).then(() => {
        resolve();
      }).catch((error) => {
        reject(error);
      });
    });
  }
  _clearComponentConnections(componentList) {
    const closePromises = [];
    componentList.forEach((component) => {
      closePromises.push(component.close());
    });
    return Promise.all(closePromises);
  }

  connectToFirstErizoController() {
    if (this.components.erizoControllers.size === 0) {
      return;
    }
    const first = this.components.erizoControllers.values().next().value;
    first.connect().then(first.openRemoteSession.bind(this));
  }

  connectToFirstErizoJS() {
    if (this.components.erizoJS.size === 0) {
      return;
    }
    const first = this.components.erizoJS.values().next().value;
    first.connect().then(first.openRemoteSession.bind(this));
  }

  getErizoControllers() {
    return new Promise((resolve, reject) => {
      this.nuveProxy.getErizoControllers().then((controllers) => {
        this.components.erizoControllers.clear();
        controllers.forEach((erizoController) => {
          let erizoControllerEntry = {};
          erizoControllerEntry.rpcId = `erizoController_${erizoController._id}`;
          erizoControllerEntry.componentType = 'erizoController';
          erizoControllerEntry = this._setRovCallsForComponent(erizoControllerEntry);
          this.components.erizoControllers.set(erizoController._id, erizoControllerEntry);
        });
        resolve();
      }).catch((error) => {
        reject(error);
      });
    });
  }
  getErizoAgents() {
    return new Promise((resolve, reject) => {
      if (this.components.erizoControllers.size === 0) {
        log.error('No erizoController present to get all ErizoAgents');
        reject('No erizoController found');
        return;
      }
      // For now we only use the first controller since they all share the full erizoAgent pool
      const controller = this.components.erizoControllers.values().next().value;
      controller.connect().then(() => {
        controller.runAndGetPromise('console.log(context.ecch.getErizoAgentsList())').then((agentsJson) => {
          this.components.erizoAgents.clear();
          const agents = JSON.parse(agentsJson);
          const agentIds = Object.keys(agents);
          for (let i = 0; i < agentIds.length; i += 1) {
            let agentEntry = Object.assign({}, agents[agentIds[i]].info);
            agentEntry.componentType = 'erizoAgent';
            agentEntry.rpcId = agentEntry.rpc_id;
            agentEntry = this._setRovCallsForComponent(agentEntry);
            this.components.erizoAgents.set(agentIds[i], agentEntry);
          }
          resolve();
        }).catch((error) => {
          reject(error);
        });
      }).catch((error) => {
        reject(error);
      });
    });
  }

  getErizoJSProcesses() {
    return new Promise((resolve, reject) => {
      if (this.components.erizoAgents.length === 0) {
        log.error('No erizoAgent sessions present to get all ErizoJS processes');
        reject('No erizoAgent session');
        return;
      }
      this.components.erizoJS.clear();
      this._establishComponentConnections(this.components.erizoAgents).then(() => {
        const erizoListPromises = [];
        this.components.erizoAgents.forEach((agent) => {
          erizoListPromises.push(agent.runAndGetPromise('console.log(context.toJSON())'));
        });
        return Promise.all(erizoListPromises);
      }).then((erizoResults) => {
        erizoResults.forEach((erizoJsJSON) => {
          const erizoJsProcesses = JSON.parse(erizoJsJSON);
          erizoJsProcesses.forEach((process) => {
            let erizoJsEntry = Object.assign({}, process);
            erizoJsEntry.componentType = 'erizoJS';
            erizoJsEntry.rpcId = `ErizoJS_${process.id}`;
            erizoJsEntry = this._setRovCallsForComponent(erizoJsEntry);
            this.components.erizoJS.set(process.id, erizoJsEntry);
          });
        });
        resolve();
      }).catch((error) => {
        log.error('Error in getErizoJSProcesses', error);
        reject();
      });
    });
  }

  updateComponentsList() {
    return new Promise((resolve, reject) => {
      log.debug('Updating component list');
      this.getErizoControllers()
        .then(this.getErizoAgents.bind(this))
        .then(this.getErizoJSProcesses.bind(this))
        .then(() => {
          log.debug('Component list updated');
          resolve();
        })
        .catch((error) => {
          log.error('Error in updateComponentList', error);
          reject(error);
        });
    });
  }

  runInComponentList(command, componentList) {
    return this.connectToComponentList(componentList).then(() => {
      const runPromises = [];
      componentList.forEach((component) => {
        runPromises.push(component.runAndGetPromise(command));
      });
      return Promise.all(runPromises);
    });
  }

  connectToComponentList(componentList) {
    const connectPromises = [];
    componentList.forEach((component) => {
      connectPromises.push(component.connect());
    });
    return Promise.all(connectPromises);
  }

  listSessions() {
    return this.sessions;
  }

  connectToSession(componentId) {
    log.info(`connectToSession ${componentId}`);
    if (this.sessions.has(componentId)) {
      log.debug(`Already connected to ${componentId}`);
      return Promise.resolve();
    }
    const rovConnection = new RovConnection(componentId, this.amqper);
    return rovConnection.connect().then(() => {
      this.sessions.set(componentId, rovConnection);
      log.info(`Connection established and ready to ${componentId}`);
      return Promise.resolve();
    });
  }

  runInSession(sessionId, command) {
    this.runInSessionWithPromise(sessionId, command).then((result) => {
      // eslint-disable-next-line
      console.log(result);
    });
  }

  runInSessionWithPromise(sessionId, command) {
    const rovSession = this.sessions.get(sessionId);
    if (!rovSession) {
      log.error(`Session ${sessionId} does not exist, trying to run ${command}`);
      return false;
    }
    return rovSession.run(command);
  }

  setDefaultSession(sessionId) {
    this.defaultSession = sessionId;
  }

  hasDefaultSession() {
    return this.defaultSession !== undefined;
  }

  runInDefaultSession(command) {
    return this.runInSessionWithPromise(this.defaultSession, command);
  }

  closeSession(sessionId) {
    log.debug(`Closing session ${sessionId}`);
    const rovSession = this.sessions.get(sessionId);
    if (!rovSession) {
      log.error(`Cannot close session, id: ${sessionId} does not exist`);
      return Promise.resolve();
    }
    return rovSession.close().then(() => {
      log.debug(`Deleting session ${sessionId}`);
      this.sessions.delete(rovSession);
      return Promise.resolve();
    });
  }

  closeAllSessions() {
    log.debug('RovClient closing all sessions');
    const closePromises = [];
    this.sessions.forEach((session) => {
      closePromises.push(session.close());
    });
    return Promise.all(closePromises).then(() => {
      this.sessions.clear();
      return Promise.resolve();
    });
  }
}
exports.RovClient = RovClient;
