
const Connection = require('./Connection').Connection;
const logger = require('./../../common/logger').logger;
const EventEmitter = require('events').EventEmitter;

const log = logger.getLogger('Client');

class Client extends EventEmitter {
  constructor(erizoControllerId, erizoJSId, id, threadPool,
    ioThreadPool, singlePc = false, options = {}) {
    super();
    log.info(`Constructor Client ${id},`,
      logger.objectToLog(options), logger.objectToLog(options.metadata));
    this.id = id;
    this.erizoJSId = erizoJSId;
    this.erizoControllerId = erizoControllerId;
    this.connections = new Map();
    this.threadPool = threadPool;
    this.ioThreadPool = ioThreadPool;
    this.singlePc = singlePc;
    this.connectionClientId = 0;
    this.options = options;
  }

  _getNewConnectionClientId() {
    this.connectionClientId += 1;
    let id = `${this.id}_${this.erizoJSId}_${this.connectionClientId}`;
    while (this.connections.get(id)) {
      id = `${this.id}_${this.erizoJSId}_${this.connectionClientId}`;
      this.connectionClientId += 1;
    }
    return id;
  }

  getOrCreateConnection(options) {
    let connection = this.connections.values().next().value;
    log.info(`message: getOrCreateConnection, clientId: ${this.id}, singlePC: ${this.singlePc},`,
      logger.objectToLog(this.options), logger.objectToLog(this.options.metadata));
    if (!this.singlePc || !connection) {
      const id = this._getNewConnectionClientId();
      connection = new Connection(this.erizoControllerId, id, this.threadPool,
        this.ioThreadPool, this.id, options);
      connection.on('status_event', this.emit.bind(this, 'status_event'));
      this.addConnection(connection);
    }
    return connection;
  }

  getConnection(id) {
    return this.connections.get(id);
  }

  addConnection(connection) {
    log.info(`message: Adding connection to Client, clientId: ${this.id}, ` +
      `connectionId: ${connection.id},`,
    logger.objectToLog(this.options), logger.objectToLog(this.options.metadata));
    this.connections.set(connection.id, connection);
    log.debug(`Client connections list size after add : ${this.connections.size}`,
      logger.objectToLog(this.options), logger.objectToLog(this.options.metadata));
  }

  getConnections() {
    return Array.from(this.connections.values());
  }

  forceCloseConnection(id) {
    const connection = this.connections.get(id);
    if (connection !== undefined) {
      this.connections.delete(connection.id);
      connection.close();
    }
  }

  closeAllConnections() {
    log.debug(`message: client closing all connections, clientId: ${this.id},`,
      logger.objectToLog(this.options), logger.objectToLog(this.options.metadata));
    this.connections.forEach((connection) => {
      connection.close();
    });
    this.connections.clear();
  }

  maybeCloseConnection(id) {
    const connection = this.connections.get(id);
    log.debug(`message: maybeCloseConnection, connectionId: ${id},`,
      logger.objectToLog(this.options), logger.objectToLog(this.options.metadata));
    if (connection !== undefined) {
      // ExternalInputs don't have mediaStreams but have to be closed
      if (connection.getNumMediaStreams() === 0) {
        if (this.singlePc) {
          log.info(`message: not closing connection because it is singlePC, clientId: ${id},`,
            logger.objectToLog(this.options), logger.objectToLog(this.options.metadata));
          return this.connections.size;
        }
        log.info(`message: closing empty connection, clientId: ${this.id},` +
        ` connectionId: ${connection.id},`,
        logger.objectToLog(this.options), logger.objectToLog(this.options.metadata));
        connection.close();
        this.connections.delete(id);
      }
    } else {
      log.warn(`message: trying to close unregistered connection, id: ${id}` +
      `, clientId: ${this.id}, remaining connections:${this.connections.size},`,
      logger.objectToLog(this.options), logger.objectToLog(this.options.metadata));
    }
    return this.connections.size;
  }
}

exports.Client = Client;
