/* global require, exports, setInterval, clearInterval, Promise */
const perfHooks = require('perf_hooks');

const logger = require('./../common/logger').logger;
const amqper = require('./../common/amqper');
const RovReplManager = require('./../common/ROV/rovReplManager').RovReplManager;
const Client = require('./models/Client').Client;
const Publisher = require('./models/Publisher').Publisher;
const ExternalInput = require('./models/Publisher').ExternalInput;
const PublisherManager = require('./models/PublisherManager').PublisherManager;
const PerformanceStats = require('../common/PerformanceStats');

// Logger
const log = logger.getLogger('ErizoJSController');

exports.ErizoJSController = (erizoJSId, threadPool, ioThreadPool) => {
  const that = {};
  // {streamId1: Publisher, streamId2: Publisher}
  const publisherManager = new PublisherManager();
  // {clientId: Client}
  const clients = new Map();
  const replManager = new RovReplManager(that);
  const io = ioThreadPool;
  // {streamId: {timeout: timeout, interval: interval}}
  const statsSubscriptions = {};
  const WARN_NOT_FOUND = 404;
  const WARN_CONFLICT = 409;
  PerformanceStats.init();

  const MAX_INACTIVE_UPTIME = global.config.erizo.activeUptimeLimit * 24 * 60 * 60 * 1000;
  const MAX_TIME_SINCE_LAST_OP =
    global.config.erizo.maxTimeSinceLastOperation * 60 * 60 * 1000;

  const uptimeStats = {
    startTime: 0,
    lastOperation: 0,
  };

  // For perf metrics
  const histogram = perfHooks.monitorEventLoopDelay({ resolution: 10 });
  histogram.enable();

  const checkUptimeStats = () => {
    const now = new Date();
    const timeSinceStart = now.getTime() - uptimeStats.startTime.getTime();
    const timeSinceLastOperation = now.getTime() - uptimeStats.lastOperation.getTime();
    log.debug(`Checking uptime - timeSinceStart: ${timeSinceStart}, timeSinceLastOperation ${timeSinceLastOperation}, max since start ${MAX_INACTIVE_UPTIME}, max since opf ${MAX_TIME_SINCE_LAST_OP}`);
    if (timeSinceStart > MAX_INACTIVE_UPTIME &&
      timeSinceLastOperation > MAX_TIME_SINCE_LAST_OP) {
      log.error(`Shutting down ErizoJS - uptime: ${timeSinceStart}, timeSinceLastOperation ${timeSinceLastOperation}`);
      process.exit(0);
    }
  };

  const updateUptimeInfo = () => {
    uptimeStats.lastOperation = new Date();
    if (uptimeStats.startTime === 0) {
      log.info('Start checking uptime, interval', global.config.erizo.checkUptimeInterval);
      uptimeStats.startTime = new Date();
      setInterval(checkUptimeStats, global.config.erizo.checkUptimeInterval * 1000);
    }
  };

  const initMetrics = () => {
    that.metrics = {
      connectionsFailed: 0,
    };
  };


  that.publisherManager = publisherManager;
  that.ioThreadPool = io;
  initMetrics();

  const onAdaptSchemeNotify = (callbackRpc, type, message) => {
    callbackRpc(type, message);
  };

  const onPeriodicStats = (clientId, streamId, newStats) => {
    const timeStamp = new Date();
    amqper.broadcast('stats', { pub: streamId,
      subs: clientId,
      stats: JSON.parse(newStats),
      timestamp: timeStamp.getTime() });
  };

  const onConnectionStatusEvent = (erizoControllerId, clientId,
    connectionId, connectionEvent, newStatus) => {
    const rpcID = `erizoController_${erizoControllerId}`;
    amqper.callRpc(rpcID, 'connectionStatusEvent', [clientId, connectionId, newStatus, connectionEvent]);

    if (connectionEvent.type === 'failed') {
      that.metrics.connectionsFailed += 1;
    }
  };

  const getOrCreateClient = (erizoControllerId, clientId, options = {}) => {
    let client = clients.get(clientId);
    const singlePC = options.singlePC || false;
    const streamPriorityStrategy = options.streamPriorityStrategy;
    if (client === undefined) {
      client = new Client(erizoControllerId, erizoJSId, clientId,
        threadPool, ioThreadPool, !!singlePC, streamPriorityStrategy, options);
      client.on('status_event', onConnectionStatusEvent.bind(this));
      clients.set(clientId, client);
    }
    return client;
  };

  const closeNode = (node, sendOffer, requestId) => {
    const clientId = node.clientId;
    const connection = node.connection;
    log.debug(`message: closeNode, clientId: ${node.clientId}, streamId: ${node.streamId}`);

    const closePromise = node.close(sendOffer, requestId);

    return closePromise.then(() => {
      log.debug(`message: Node Closed, clientId: ${node.clientId}, streamId: ${node.streamId}`);
      const client = clients.get(clientId);
      if (client === undefined) {
        log.debug('message: trying to close node with no associated client,' +
          `clientId: ${clientId}, streamId: ${node.streamId}`);
        return;
      }
      const remainingConnections = client.maybeCloseConnection(connection.id);
      if (remainingConnections === 0) {
        log.debug(`message: Client is empty, clientId: ${client.id}`);
      }
    });
  };

  // RemoveClient does not imply deleting the data structures of publishers and subscribers
  // we rely on erizoController to remove the publishers and subscribers and then call removeClient
  that.removeClient = (clientId, callback) => {
    log.info(`message: requested removeClient, clientId: ${clientId}`);
    const client = clients.get(clientId);
    if (client === undefined) {
      log.info(`message: removeClient - client is not present in this ErizoJS, clientId: ${clientId}`);
      callback('callback', false);
      return;
    }

    log.info(`message: removeClient - removingPublished streams, clientId: ${clientId}`);
    const publishers = publisherManager.getPublishersByClientId(clientId);

    const removePromises = [];

    publishers.forEach((pub) => {
      log.info(`message: removeClient - removingPublished stream, clientId: ${clientId}, streamId: ${pub.streamId}`);
      removePromises.push(that.removePublisher(clientId, pub.streamId));
    });

    removePromises.push(that.removeSubscriptions(clientId));

    Promise.all(removePromises).then(() => {
      log.info(`message: removeClient - closing all connections on, clientId: ${clientId}`);
      client.closeAllConnections();
      clients.delete(client.id);
      callback('callback', true);
      if (clients.size === 0) {
        log.info('message: Removed all clients. Process will exit');
        process.exit(0);
      }
    });
  };

  that.rovMessage = (args, callback) => {
    replManager.processRpcMessage(args, callback);
  };

  that.addExternalInput = (erizoControllerId, streamId, url, label, callbackRpc) => {
    updateUptimeInfo();
    if (!publisherManager.has(streamId)) {
      const client = getOrCreateClient(erizoControllerId, url);
      const ei = new ExternalInput(url, streamId, label, threadPool);
      publisherManager.add(streamId, ei);
      const answer = ei.init();
      // We add the connection manually to the client
      client.addConnection(ei);
      if (answer >= 0) {
        callbackRpc('callback', 'success');
      } else {
        callbackRpc('callback', answer);
      }
    } else {
      log.warn(`message: Publisher already set, code: ${WARN_CONFLICT}, ` +
        `streamId: ${streamId}`);
    }
  };

  that.addExternalOutput = (streamId, url, options) => {
    updateUptimeInfo();
    if (publisherManager.has(streamId)) {
      publisherManager.getPublisherById(streamId).addExternalOutput(url, options);
    }
  };

  that.removeExternalOutput = (streamId, url) => {
    if (publisherManager.has(streamId)) {
      log.info('message: Stopping ExternalOutput, ' +
        `id: ${publisherManager.getPublisherById(streamId).getExternalOutput(url).id}`);
      publisherManager.getPublisherById(streamId).removeExternalOutput(url);
    }
  };

  that.processConnectionMessage = (erizoControllerId, clientId, connectionId, msg,
    callbackRpc = () => {}) => {
    let error;
    const client = clients.get(clientId);
    if (!client) {
      log.warn('message: Process Connection message to unknown clientId, ' +
        `clientId: ${clientId}, connectionId: ${connectionId}`);
      error = 'client-not-found';
    }

    const connection = client.getConnection(connectionId);
    if (!connection) {
      log.warn('message: Process Connection message to unknown connectionId, ' +
        `clientId: ${clientId}, connectionId: ${connectionId}`);
      error = 'connection-not-found';
    }

    if (error) {
      callbackRpc('callback', { error });
      return Promise.resolve();
    }

    log.info('message: Process Connection message, ' +
      `clientId: ${clientId}, connectionId: ${connectionId},`,
    logger.objectToLog(client.options), logger.objectToLog(client.options.metadata));
    if (msg.type === 'failed') {
      client.forceCloseConnection(connectionId);
    }

    return connection.onSignalingMessage(msg).then(() => {
      callbackRpc('callback', {});
    });
  };

  that.processStreamMessage = (erizoControllerId, clientId, streamId, msg) => {
    let node;
    if (!publisherManager.has(streamId)) {
      log.warn('message: Process Stream message stream not found, ' +
        `clientId: ${clientId}, streamId: ${streamId}`);
      return;
    }
    const publisher = publisherManager.getPublisherById(streamId);

    if (publisher.clientId === clientId) {
      node = publisher;
    } else if (publisher.hasSubscriber(clientId)) {
      node = publisher.getSubscriber(clientId);
    } else {
      log.warn('message: Process Stream message stream not found, ' +
        `clientId: ${clientId}, streamId: ${streamId}`);
      return;
    }
    log.info('message: Process Stream message, ' +
      `clientId: ${clientId}, streamId: ${streamId},`,
    logger.objectToLog(node.options), logger.objectToLog(node.options.metadata));

    node.onStreamMessage(msg);
  };

  /*
   * Adds a publisher to the room. This creates a new OneToManyProcessor
   * and a new WebRtcConnection. This WebRtcConnection will be the publisher
   * of the OneToManyProcessor.
   */
  that.addPublisher = (erizoControllerId, clientId, streamId, options, callbackRpc) => {
    updateUptimeInfo();
    let publisher;
    log.info('addPublisher, clientId', clientId, 'streamId', streamId);
    const client = getOrCreateClient(erizoControllerId, clientId, options);

    if (!publisherManager.has(streamId)) {
      // eslint-disable-next-line no-param-reassign
      options.publicIP = that.publicIP;
      // eslint-disable-next-line no-param-reassign
      options.privateRegexp = that.privateRegexp;
      const connection = client.getOrCreateConnection(options);
      log.info('message: Adding publisher, ',
        `clientId: ${clientId}, `,
        `streamId: ${streamId},`,
        logger.objectToLog(options),
        logger.objectToLog(options.metadata));
      publisher = new Publisher(clientId, streamId, connection, options);
      publisherManager.add(streamId, publisher);
      publisher.initMediaStream();
      publisher.on('callback', onAdaptSchemeNotify.bind(this, callbackRpc));
      publisher.on('periodic_stats', onPeriodicStats.bind(this, streamId, undefined));
      publisher.promise.then(() => {
        connection.init(options.createOffer);
      });
      connection.onInitialized.then(() => {
        callbackRpc('callback', { type: 'initializing', connectionId: connection.id });
      });
      connection.onReady.then(() => {
        callbackRpc('callback', { type: 'ready' });
      });
      connection.onStarted.then(() => {
        callbackRpc('callback', { type: 'started' });
      });
      if (options.createOffer) {
        let onEvent;
        if (options.trickleIce) {
          onEvent = connection.onInitialized;
        } else {
          onEvent = connection.onGathered;
        }
        onEvent.then(() => {
          connection.sendOffer();
        });
      }
    } else {
      publisher = publisherManager.getPublisherById(streamId);
      if (publisher.numSubscribers === 0) {
        log.warn('message: publisher already set but no subscribers will ignore, ',
          `code: ${WARN_CONFLICT}, streamId: ${streamId},`,
          logger.objectToLog(options.metadata));
      } else {
        log.warn('message: publisher already set has subscribers will ignore, ' +
          `code: ${WARN_CONFLICT}, streamId: ${streamId}`);
      }
    }
  };

  /*
   * Adds a subscriber to the room. This creates a new WebRtcConnection.
   * This WebRtcConnection will be added to the subscribers list of the
   * OneToManyProcessor.
   */
  that.addSubscriber = (erizoControllerId, clientId, streamId, options, callbackRpc, requestId) => {
    updateUptimeInfo();
    PerformanceStats.mark(requestId, PerformanceStats.Marks.SUBSCRIBE_REQUEST_RECEIVED);

    const publisher = publisherManager.getPublisherById(streamId);
    if (publisher === undefined) {
      log.warn('message: addSubscriber to unknown publisher, ',
        `code: ${WARN_NOT_FOUND}, streamId: ${streamId}, `,
        `clientId: ${clientId},`,
        logger.objectToLog(options),
        logger.objectToLog(options.metadata));
      // We may need to notify the clients
      return;
    }
    let subscriber = publisher.getSubscriber(clientId);
    const client = getOrCreateClient(erizoControllerId, clientId, options);
    if (subscriber !== undefined) {
      log.warn('message: Duplicated subscription will resubscribe, ' +
        `code: ${WARN_CONFLICT}, streamId: ${streamId}, ` +
        `clientId: ${clientId},`, logger.objectToLog(options.metadata));
      that.removeSubscriber(clientId, streamId);
    }
    // eslint-disable-next-line no-param-reassign
    options.publicIP = that.publicIP;
    // eslint-disable-next-line no-param-reassign
    options.privateRegexp = that.privateRegexp;
    const connection = client.getOrCreateConnection(options);
    // eslint-disable-next-line no-param-reassign
    options.label = publisher.label;
    subscriber = publisher.addSubscriber(clientId, connection, options);

    subscriber.initMediaStream(options.offerFromErizo);
    if (options.offerFromErizo) {
      subscriber.copySdpInfoFromPublisher();
    }

    subscriber.on('callback', onAdaptSchemeNotify.bind(this, callbackRpc, 'callback'));
    subscriber.on('periodic_stats', onPeriodicStats.bind(this, clientId, streamId));

    if (options.offerFromErizo) {
      subscriber.promise
        .then(() => PerformanceStats.mark(requestId,
          PerformanceStats.Marks.SUBSCRIBE_STREAM_CREATED))
        .then(() => connection.init({ audio: true, video: true, bundle: true }))
        .then(() => PerformanceStats.mark(requestId,
          PerformanceStats.Marks.SUBSCRIBE_CONNECTION_INIT))
        .then(() => connection.onGathered)
        .then(() => PerformanceStats.mark(requestId,
          PerformanceStats.Marks.SUBSCRIBE_CANDIDATES_GATHERED))
        .then(() => connection.sendOffer(requestId))
        .then(() => PerformanceStats.mark(requestId,
          PerformanceStats.Marks.SUBSCRIBE_RESPONSE_SENT));
    } else {
      subscriber.promise
        .then(() => PerformanceStats.mark(requestId,
          PerformanceStats.Marks.SUBSCRIBE_STREAM_CREATED))
        .then(() => connection.init(options.createOffer))
        .then(() => PerformanceStats.mark(requestId,
          PerformanceStats.Marks.SUBSCRIBE_CONNECTION_INIT))
        .then(() => PerformanceStats.mark(requestId,
          PerformanceStats.Marks.SUBSCRIBE_RESPONSE_SENT));
    }

    connection.onInitialized.then(() => {
      callbackRpc('callback', { type: 'initializing', connectionId: connection.id });
    });
    connection.onReady.then(() => {
      callbackRpc('callback', { type: 'ready' });
    });
    connection.onStarted.then(() => {
      callbackRpc('callback', { type: 'started' });
    });
    if (options.createOffer) {
      let onEvent;
      if (options.trickleIce) {
        onEvent = connection.onInitialized;
      } else {
        onEvent = connection.onGathered;
      }
      onEvent.then(() => {
        connection.sendOffer();
      });
    }
  };

  /*
   * Adds multiple subscribers to the room.
   */
  that.addMultipleSubscribers = (erizoControllerId, clientId, streamIds, options, callbackRpc) => {
    if (!options.singlePC) {
      log.warn('message: addMultipleSubscribers not compatible with no single PC, clientId:', clientId);
      callbackRpc('callback', { type: 'error' });
      return;
    }

    const knownPublishers = streamIds.map(streamId => publisherManager.getPublisherById(streamId))
      .filter(pub =>
        pub !== undefined &&
                                        !pub.getSubscriber(clientId));
    if (knownPublishers.length === 0) {
      log.warn('message: addMultipleSubscribers to unknown publisher, ',
        `code: ${WARN_NOT_FOUND}, streamIds: ${streamIds}, `,
        `clientId: ${clientId},`,
        logger.objectToLog(options.metadata));
      callbackRpc('callback', { type: 'error' });
      return;
    }

    log.debug('message: addMultipleSubscribers to publishers, ',
      `streamIds: ${knownPublishers}, `,
      `clientId: ${clientId},`,
      logger.objectToLog(options.metadata));

    const client = getOrCreateClient(erizoControllerId, clientId, options);
    // eslint-disable-next-line no-param-reassign
    options.publicIP = that.publicIP;
    // eslint-disable-next-line no-param-reassign
    options.privateRegexp = that.privateRegexp;
    const connection = client.getOrCreateConnection(options);
    const promises = [];
    knownPublishers.forEach((publisher) => {
      const streamId = publisher.streamId;
      // eslint-disable-next-line no-param-reassign
      options.label = publisher.label;
      const subscriber = publisher.addSubscriber(clientId, connection, options);
      subscriber.initMediaStream(true);
      subscriber.copySdpInfoFromPublisher();
      promises.push(subscriber.promise);
      subscriber.on('callback', onAdaptSchemeNotify.bind(this, callbackRpc, 'callback'));
      subscriber.on('periodic_stats', onPeriodicStats.bind(this, clientId, streamId));
    });

    const knownStreamIds = knownPublishers.map(pub => pub.streamId);

    const constraints = {
      audio: true,
      video: true,
      bundle: true,
    };

    connection.init(constraints);
    promises.push(connection.createOfferPromise);
    Promise.all(promises)
      .then(() => {
        log.debug('message: autoSubscription waiting for gathering event', connection.alreadyGathered, connection.onGathered);
        return connection.onGathered;
      })
      .then(() => {
        callbackRpc('callback', { type: 'multiple-initializing', connectionId: connection.id, streamIds: knownStreamIds, context: 'auto-streams-subscription', options });
        connection.sendOffer();
      });
  };

  /*
   * Removes multiple subscribers from the room.
   */
  that.removeMultipleSubscribers = (clientId, streamIds, callbackRpc) => {
    const knownPublishers = streamIds.map(streamId => publisherManager.getPublisherById(streamId))
      .filter(pub =>
        pub !== undefined &&
                                        pub.getSubscriber(clientId));
    if (knownPublishers.length === 0) {
      log.warn('message: removeMultipleSubscribers from unknown publisher, ' +
        `code: ${WARN_NOT_FOUND}, streamIds: ${streamIds}, ` +
        `clientId: ${clientId}`);
      callbackRpc('callback', { type: 'error' });
      return;
    }

    log.debug('message: removeMultipleSubscribers from publishers, ' +
        `streamIds: ${knownPublishers}, ` +
        `clientId: ${clientId}`);

    const client = clients.get(clientId);
    if (!client) {
      callbackRpc('callback', { type: 'error' });
    }

    let connection;

    const promises = [];
    knownPublishers.forEach((publisher) => {
      if (publisher && publisher.hasSubscriber(clientId)) {
        const subscriber = publisher.getSubscriber(clientId);
        connection = subscriber.connection;
        promises.push(closeNode(subscriber, false));
        publisher.removeSubscriber(clientId);
      }
    });

    const knownStreamIds = knownPublishers.map(pub => pub.streamId);

    Promise.all(promises)
      .then(() => connection.onGathered)
      .then(() => {
        callbackRpc('callback', {
          type: 'multiple-removal',
          connectionId: connection.id,
          streamIds: knownStreamIds,
          context: 'auto-streams-unsubscription' });
        connection.sendOffer();
      });
  };

  /*
   * Removes a publisher from the room. This also deletes the associated OneToManyProcessor.
   */
  that.removePublisher = (clientId, streamId, callback = () => {}) =>
    new Promise((resolve) => {
      const publisher = publisherManager.getPublisherById(streamId);
      if (publisher !== undefined) {
        log.info(`message: Removing publisher, id: ${clientId}, streamId: ${streamId},`,
          logger.objectToLog(publisher.options), logger.objectToLog(publisher.options.metadata));
        publisher.forEachSubscriber((subscriberId, subscriber) => {
          log.info(`message: Removing subscriber, id: ${subscriberId},`,
            logger.objectToLog(subscriber.options),
            logger.objectToLog(subscriber.options.metadata));
          closeNode(subscriber);
          publisher.removeSubscriber(subscriberId);
        });
        publisher.removeExternalOutputs().then(() => {
          publisherManager.remove(streamId);
          closeNode(publisher).then(() => {
            publisher.muxer.close((message) => {
              log.info('message: muxer closed succesfully, ',
                `id: ${streamId},`,
                logger.objectToLog(message));
              const count = publisherManager.getPublisherCount();
              log.debug(`message: remaining publishers, publisherCount: ${count}`);
              callback('callback', true);
              resolve();
              if (count === 0) {
                log.info('message: Removed all publishers. Process is empty.');
              }
            });
          });
        });
      } else {
        log.warn('message: removePublisher that does not exist, ' +
          `code: ${WARN_NOT_FOUND}, id: ${streamId}`);
        resolve();
      }
    });

  /*
   * Removes a subscriber from the room.
   * This also removes it from the associated OneToManyProcessor.
   */
  that.removeSubscriber = (clientId, streamId, callback = () => {}, requestId = undefined) => {
    const publisher = publisherManager.getPublisherById(streamId);
    if (publisher && publisher.hasSubscriber(clientId)) {
      PerformanceStats.mark(requestId, PerformanceStats.Marks.UNSUBSCRIBE_REQUEST_RECEIVED);
      const subscriber = publisher.getSubscriber(clientId);
      log.info(`message: removing subscriber, streamId: ${subscriber.streamId}, ` +
        `clientId: ${clientId},`,
      logger.objectToLog(subscriber.options), logger.objectToLog(subscriber.options.metadata));
      return closeNode(subscriber, true, requestId).then(() => {
        publisher.removeSubscriber(clientId);
        log.info(`message: subscriber node Closed, streamId: ${subscriber.streamId}`);
        callback('callback', true);
        PerformanceStats.mark(requestId, PerformanceStats.Marks.UNSUBSCRIBE_RESPONSE_SENT);
      });
    }
    log.warn(`message: removeSubscriber no publisher has this subscriber, clientId: ${clientId}, streamId: ${streamId}`);
    callback('callback', true);
    return Promise.resolve();
  };

  /*
   * Removes all the subscribers related with a client.
   */
  that.removeSubscriptions = (clientId) => {
    log.info('message: removing subscriptions, clientId:', clientId);
    // we go through all the connections in the client and we close them
    const closePromises = [];
    that.publisherManager.forEach((publisher) => {
      const subscriber = publisher.getSubscriber(clientId);
      if (subscriber) {
        log.debug('message: removing subscription, ' +
          'id:', subscriber.clientId, ',',
        logger.objectToLog(subscriber.options), logger.objectToLog(subscriber.options.metadata));
        closePromises.push(closeNode(subscriber).then(() => {
          publisher.removeSubscriber(clientId);
        }));
      }
    });
    return Promise.all(closePromises);
  };

  that.setClientStreamPriorityStrategy = (clientId, strategyId) => {
    log.debug(`message: Trying to set streamPriorityStrategy to for client ${clientId}`);
    if (clients.has(clientId)) {
      log.info(`message: updating streamPriorityStrategy in client ${clientId} to ${strategyId}`);
      clients.get(clientId).setStreamPriorityStrategy(strategyId);
    }
  };

  that.getStreamStats = (streamId, callbackRpc) => {
    const stats = {};
    let publisher;
    log.debug(`message: Requested stream stats, streamID: ${streamId}`);
    const promises = [];
    if (streamId && publisherManager.has(streamId)) {
      publisher = publisherManager.getPublisherById(streamId);
      promises.push(publisher.getStats('publisher', stats));

      publisher.forEachSubscriber((subscriberId, subscriber) => {
        promises.push(
          subscriber.getStats(
            subscriberId,
            stats));
      });
      Promise.all(promises).then(() => {
        callbackRpc('callback', stats);
      });
    }
  };

  that.subscribeToStats = (streamId, timeout, interval, callbackRpc) => {
    let publisher;
    log.debug(`message: Requested subscription to stream stats, streamId: ${streamId}`);

    if (streamId && publisherManager.has(streamId)) {
      publisher = publisherManager.getPublisherById(streamId);

      if (global.config.erizoController.reportSubscriptions &&
          global.config.erizoController.reportSubscriptions.maxSubscriptions > 0) {
        if (timeout > global.config.erizoController.reportSubscriptions.maxTimeout) {
          // eslint-disable-next-line no-param-reassign
          timeout = global.config.erizoController.reportSubscriptions.maxTimeout;
        }
        if (interval < global.config.erizoController.reportSubscriptions.minInterval) {
          // eslint-disable-next-line no-param-reassign
          interval = global.config.erizoController.reportSubscriptions.minInterval;
        }
        if (statsSubscriptions[streamId]) {
          log.debug(`message: Renewing subscription to stream: ${streamId}`);
          clearTimeout(statsSubscriptions[streamId].timeout);
          clearInterval(statsSubscriptions[streamId].interval);
        } else if (Object.keys(statsSubscriptions).length <
            global.config.erizoController.reportSubscriptions.maxSubscriptions) {
          statsSubscriptions[streamId] = {};
        }

        if (!statsSubscriptions[streamId]) {
          log.debug('message: Max Subscriptions limit reached, ignoring message');
          return;
        }

        statsSubscriptions[streamId].interval = setInterval(() => {
          const promises = [];
          const stats = {};

          stats.streamId = streamId;
          promises.push(publisher.getStats('publisher', stats));
          publisher.forEachSubscriber((subscriberId, subscriber) => {
            promises.push(subscriber.getStats(subscriberId, stats));
          });
          Promise.all(promises).then(() => {
            amqper.broadcast('stats_subscriptions', stats);
          });
        }, interval * 1000);

        statsSubscriptions[streamId].timeout = setTimeout(() => {
          clearInterval(statsSubscriptions[streamId].interval);
          delete statsSubscriptions[streamId];
        }, timeout * 1000);

        callbackRpc('success');
      } else {
        log.debug('message: Report subscriptions disabled by config, ignoring message');
      }
    } else {
      log.debug(`message: stream not found - ignoring message, streamId: ${streamId}`);
    }
  };

  const sumArrays = (array1, array2) => array2.map((a, i) => a + array1[i]);

  that.getAndResetMetrics = () => {
    const metrics = Object.assign({}, that.metrics);
    metrics.totalConnections = 0;
    metrics.connectionLevels = Array(10).fill(0);
    metrics.publishers = publisherManager.getPublisherCount();
    metrics.streamDelayDistribution = Array(10).fill(0);
    metrics.streamDurationDistribution = Array(10).fill(0);
    let subscribers = 0;
    publisherManager.forEach((publisher) => {
      subscribers += publisher.numSubscribers;
      const streamDurationDistribution = publisher.getDurationDistribution();
      const streamDelayDistribution = publisher.getDelayDistribution();
      publisher.resetStats();
      metrics.streamDurationDistribution =
        sumArrays(metrics.streamDurationDistribution, streamDurationDistribution);
      metrics.streamDelayDistribution =
        sumArrays(metrics.streamDelayDistribution, streamDelayDistribution);
      publisher.forEachSubscriber((clientId, subscriber) => {
        const duration = subscriber.getDurationDistribution();
        const delay = subscriber.getDelayDistribution();
        subscriber.resetStats();
        metrics.streamDurationDistribution =
          sumArrays(metrics.streamDurationDistribution, duration);
        metrics.streamDelayDistribution =
          sumArrays(metrics.streamDelayDistribution, delay);
      });
    });
    metrics.subscribers = subscribers;

    metrics.connectionDelayDistribution = Array(10).fill(0);
    metrics.connectionDurationDistribution = Array(10).fill(0);
    metrics.durationDistribution = threadPool.getDurationDistribution();
    metrics.delayDistribution = threadPool.getDelayDistribution();
    threadPool.resetStats();

    clients.forEach((client) => {
      const connections = client.getConnections();
      metrics.totalConnections += connections.length;

      connections.forEach((connection) => {
        const connectionDurationDistribution = connection.getDurationDistribution();
        const connectionDelayDistribution = connection.getDelayDistribution();
        connection.resetStats();
        metrics.connectionDurationDistribution =
          sumArrays(metrics.connectionDurationDistribution, connectionDurationDistribution);
        metrics.connectionDelayDistribution =
          sumArrays(metrics.connectionDelayDistribution, connectionDelayDistribution);
        const level = connection.qualityLevel;
        if (level >= 0 && level < metrics.connectionLevels.length) {
          metrics.connectionLevels[level] += 1;
        }
      });
    });
    initMetrics();
    return metrics;
  };

  that.computeEventLoopLags = () => ({
    min: histogram.min / 1e9,
    max: histogram.max / 1e9,
    mean: histogram.mean / 1e9,
    stddev: histogram.stddev / 1e9,
    median: histogram.percentile(50) / 1e9,
    p95: histogram.percentile(90) / 1e9,
    p99: histogram.percentile(99) / 1e9,
  });

  that.computePerformanceStats = () => PerformanceStats.process();

  return that;
};
