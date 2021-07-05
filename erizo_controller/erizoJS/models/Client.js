
const RtcPeerConnection = require('./RTCPeerConnection');
const logger = require('./../../common/logger').logger;
const PerformanceStats = require('../../common/PerformanceStats');
const EventEmitter = require('events').EventEmitter;

const log = logger.getLogger('Client');

const CONN_SDP = 202;

class Client extends EventEmitter {
  constructor(erizoControllerId, erizoJSId, id, threadPool,
    ioThreadPool, singlePc = false, streamPriorityStrategy = false,
    options = {}) {
    super();
    log.info(`Constructor Client ${id}, singlePC: ${singlePc}, strategy: ${streamPriorityStrategy}`,
      logger.objectToLog(options), logger.objectToLog(options.metadata));
    this.id = id;
    this.erizoJSId = erizoJSId;
    this.erizoControllerId = erizoControllerId;
    this.connections = new Map();
    this.threadPool = threadPool;
    this.ioThreadPool = ioThreadPool;
    this.singlePc = singlePc;
    this.streamPriorityStrategy = Client._getStreamPriorityStrategy(streamPriorityStrategy);
    this.connectionClientId = 0;
    this.options = options;
  }

  static _getStreamPriorityStrategy(streamPriorityStrategy) {
    if (streamPriorityStrategy &&
      global.bwDistributorConfig.strategyDefinitions &&
      global.bwDistributorConfig.strategyDefinitions[streamPriorityStrategy]) {
      return streamPriorityStrategy;
    }
    log.info(`Trying to set undefined strategy Id ${streamPriorityStrategy}, reverting to default algorithm`);
    return false;
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

  _createConnection(options) {
    const id = this._getNewConnectionClientId();
    const configuration = {};
    configuration.options = options;
    configuration.erizoControllerId = this.erizoControllerId;
    configuration.id = id;
    configuration.clientId = this.id;
    configuration.threadPool = this.threadPool;
    configuration.ioThreadPool = this.ioThreadPool;
    configuration.trickleIce = options.trickleIce;
    configuration.mediaConfiguration = options.mediaConfiguration;
    configuration.isRemote = options.isRemote;
    configuration.encryptTransport = options.encryptTransport;
    configuration.streamPriorityStrategy = this.streamPriorityStrategy;
    const connection = new RtcPeerConnection(configuration);
    connection.on('status_event', (...args) => {
      this.emit('status_event', ...args);
    });
    connection.on('negotiationneeded', this.onNegotiationNeeded.bind(this, connection));
    connection.makingOffer = false;
    connection.ignoreOffer = false;
    connection.srdAnswerPending = false;
    connection.isRemote = options.isRemote;
    connection.offers = 0;
    connection.answers = 0;
    connection.init();
    return connection;
  }

  getOrCreateConnection(options) {
    let connection;
    // In Single PC we use two connections, one for publishers and another for subscribers.
    // eslint-disable-next-line no-restricted-syntax
    for (const conn of this.connections.values()) {
      if (conn.isRemote === options.isRemote) {
        connection = conn;
      }
    }
    log.debug(`message: getOrCreateConnection, clientId: ${this.id}, singlePC: ${this.singlePc}`,
      logger.objectToLog(this.options), logger.objectToLog(this.options.metadata));
    if (!this.singlePc || !connection) {
      connection = this._createConnection(options);
      this.addConnection(connection);
    }
    return connection;
  }

  // These two methods implement the Perfect Negotiation algorithm in https://w3c.github.io/webrtc-pc/#perfect-negotiation-example
  // This is the non-polite peer
  async onNegotiationNeeded(connectionInput) {
    const connection = connectionInput;
    try {
      log.info(`message: Connection Negotiation Needed, clientId: ${this.id}, connection: ${connection.id}, makingOffer: ${connection.makingOffer}, offers: ${connection.offers}, state: ${connection.signalingState},`,
        logger.objectToLog(this.options), logger.objectToLog(this.options.metadata));
      connection.makingOffer = true;
      connection.offers += 1;
      PerformanceStats.mark(`${connection.id}_offer_${connection.offers}`, PerformanceStats.Marks.CONNECTION_NEGOTIATION_OFFER_CREATING);
      await connection.onInitialized;
      await connection.setLocalDescription();
      PerformanceStats.mark(`${connection.id}_offer_${connection.offers}`, PerformanceStats.Marks.CONNECTION_NEGOTIATION_OFFER_CREATED);
      this.emit('status_event', this.erizoControllerId, this.id, connection.id, { type: 'offer', sdp: connection.localDescription }, CONN_SDP);
      PerformanceStats.mark(`${connection.id}_offer_${connection.offers}`, PerformanceStats.Marks.CONNECTION_NEGOTIATION_OFFER_SENT);
    } catch (e) {
      if (connection.signalingState !== 'closed') {
        const stack = e && e.stack && e.stack.replace(/\n/g, '\\n').replace(/\s/g, '').replace(/:/g, '.');
        const message = e && e.message;
        log.error(`message: Error creating offer, clientId: ${this.id}, connection: ${connection.id}, error: ${message}, stack: ${stack}, error: ${e.toString()}, state: ${connection.signalingState},`,
          logger.objectToLog(this.options), logger.objectToLog(this.options.metadata));
      }
    } finally {
      connection.makingOffer = false;
      log.info(`message: Offer created, clientId: ${this.id}, connection: ${connection.id}, makingOffer: ${connection.makingOffer}, offers: ${connection.offers}, state: ${connection.signalingState},`,
        logger.objectToLog(this.options), logger.objectToLog(this.options.metadata));
    }
  }

  async onSignalingMessage(connectionInput, description) {
    const connection = connectionInput;

    if (description.candidate) {
      try {
        await connection.addIceCandidate(description);
      } catch (e) {
        log.error(`message: Error adding ICE candidate, clientId: ${this.id}, message> ${e.message}`,
          logger.objectToLog(this.options), logger.objectToLog(this.options.metadata));
      }
    } else {
      // If we have a setRemoteDescription() answer operation pending, then
      // we will be "stable" by the time the next setRemoteDescription() is
      // executed, so we count this being stable when deciding whether to
      // ignore the offer.
      const isStable =
          connection.signalingState === 'stable' ||
          (connection.signalingState === 'have-local-offer' && connection.srdAnswerPending);
      connection.ignoreOffer =
          description.type === 'offer' && (connection.makingOffer || !isStable);
      if (connection.ignoreOffer) {
        log.debug(`message: Glare - ignoring offer, clientId ${this.id}`, logger.objectToLog(this.options), logger.objectToLog(this.options.metadata));
        return;
      }

      connection.srdAnswerPending = description.type === 'answer';
      if (connection.srdAnswerPending) {
        PerformanceStats.mark(`${connection.id}_offer_${connection.offers}`, PerformanceStats.Marks.CONNECTION_NEGOTIATION_ANSWER_RECEIVED);
      } else {
        connection.answers += 1;
        PerformanceStats.mark(`${connection.id}_answer_${connection.answers}`, PerformanceStats.Marks.CONNECTION_NEGOTIATION_OFFER_RECEIVED);
      }
      await connection.setRemoteDescription(description);
      if (connection.srdAnswerPending) {
        PerformanceStats.mark(`${connection.id}_offer_${connection.offers}`, PerformanceStats.Marks.CONNECTION_NEGOTIATION_ANSWER_SET);
      } else {
        PerformanceStats.mark(`${connection.id}_answer_${connection.answers}`, PerformanceStats.Marks.CONNECTION_NEGOTIATION_OFFER_SET);
      }
      connection.srdAnswerPending = false;
      if (description.type === 'offer') {
        PerformanceStats.mark(`${connection.id}_answer_${connection.answers}`, PerformanceStats.Marks.CONNECTION_NEGOTIATION_ANSWER_CREATING);
        await connection.setLocalDescription();
        PerformanceStats.mark(`${connection.id}_answer_${connection.answers}`, PerformanceStats.Marks.CONNECTION_NEGOTIATION_ANSWER_CREATED);
        this.emit('status_event', this.erizoControllerId, this.id, connection.id, { type: 'answer', sdp: connection.localDescription }, CONN_SDP);
        PerformanceStats.mark(`${connection.id}_answer_${connection.answers}`, PerformanceStats.Marks.CONNECTION_NEGOTIATION_ANSWER_SENT);
      }
    }
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

  setStreamPriorityStrategy(streamPriorityStrategy) {
    this.streamPriorityStrategy = Client._getStreamPriorityStrategy(streamPriorityStrategy);
    this.connections.forEach((connection) => {
      connection.setStreamPriorityStrategy(this.streamPriorityStrategy);
    });
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
      if (connection.getNumStreams() === 0) {
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
