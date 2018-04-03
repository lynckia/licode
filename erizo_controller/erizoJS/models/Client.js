'use strict';
var Connection = require('./Connection').Connection;
var logger = require('./../../common/logger').logger;
var log = logger.getLogger('Client');

class Client {

  constructor(id, threadPool, ioThreadPool, singlePc = false) {
    log.debug(`Constructor Client ${id}`);
    this.id = id;
    this.connections = new Map();
    this.threadPool = threadPool;
    this.ioThreadPool = ioThreadPool;
    this.singlePc = singlePc;
    this.connectionClientId = 0;
  }

  _getNewConnectionClientId() {
    this.connectionClientId += 1;
    let id = `${this.id}_${this.connectionClientId}`;
    while (this.connections.get(id)) {
      id = `${this.id}_${this.connectionClientId}`;
      this.connectionClientId += 1;
    }
    return id;
  }

  getOrCreateConnection(options) {
    let connection = this.connections.values().next().value;
    log.info(`message: getOrCreateConnection, clientId: ${this.id}, singlePC: ${this.singlePc}`);
    if (!this.singlePc || !connection) {
      let id = this._getNewConnectionClientId();
      connection = new Connection(id, this.threadPool, this.ioThreadPool, options);
      this.addConnection(connection);
    }
    return connection;
  }

  getConnection(id) {
    return this.connections.get(id);
  }

  addConnection(connection) {
    log.info(`message: Adding connection to Client, clientId: ${this.id}, ` +
      `connectionId: ${connection.id}`);
    this.connections.set(connection.id, connection);
    log.debug(`Client connections list size after add : ${this.connections.size}`);
  }

  maybeCloseConnection(id) {
    let connection = this.connections.get(id);
    log.debug(`message: maybeCloseConnection, connectionId: ${id}`);
    if (connection !== undefined) {
      // ExternalInputs don't have mediaStreams but have to be closed
      if (connection.getNumMediaStreams() === 0) {
        log.info(`message: closing empty connection, clientId: ${this.id}` +
        ` connectionId: ${connection.id}`);
        connection.close();
        this.connections.delete(id);
      }
    } else {
      log.error(`message: trying to close unregistered connection, id: ${id}` +
      `, clientId: ${this.id}, remaining connections:${this.connections.size}`);
    }
    return this.connections.size;
  }
}

exports.Client = Client;
