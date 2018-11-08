/* global require, exports, setInterval, clearInterval, Promise */


const logger = require('./../common/logger').logger;
const amqper = require('./../common/amqper');
const Client = require('./models/Client').Client;
const Publisher = require('./models/Publisher').Publisher;
const ExternalInput = require('./models/Publisher').ExternalInput;

// Logger
const log = logger.getLogger('ErizoJSController');

exports.ErizoJSController = (threadPool, ioThreadPool) => {
  const that = {};
  // {streamId1: Publisher, streamId2: Publisher}
  const publishers = {};
  // {clientId: Client}
  const clients = new Map();
  const io = ioThreadPool;
  // {streamId: {timeout: timeout, interval: interval}}
  const statsSubscriptions = {};
  const WARN_NOT_FOUND = 404;
  const WARN_CONFLICT = 409;

  that.publishers = publishers;
  that.ioThreadPool = io;

  const forEachPublisher = (action) => {
    const publisherStreamIds = Object.keys(publishers);
    for (let i = 0; i < publisherStreamIds.length; i += 1) {
      action(publisherStreamIds[i], publishers[publisherStreamIds[i]]);
    }
  };

  const getOrCreateClient = (clientId, singlePC = false) => {
    log.debug(`getOrCreateClient with id ${clientId}`);
    let client = clients.get(clientId);
    if (client === undefined) {
      client = new Client(clientId, threadPool, ioThreadPool, !!singlePC);
      clients.set(clientId, client);
    }
    return client;
  };

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

  const onConnectionStatusEvent = (callbackRpc, clientId, streamId, connectionEvent, newStatus) => {
    if (newStatus && global.config.erizoController.report.connection_events) {
      const timeStamp = new Date();
      amqper.broadcast('event', { pub: streamId,
        subs: clientId,
        type: 'connection_status',
        status: newStatus,
        timestamp: timeStamp.getTime() });
    }
    callbackRpc('callback', connectionEvent);
  };

  const closeNode = (node) => {
    const clientId = node.clientId;
    const connection = node.connection;
    log.debug(`message: closeNode, clientId: ${node.clientId}, streamId: ${node.streamId}`);

    node.close();

    const client = clients.get(clientId);
    if (client === undefined) {
      log.debug('message: trying to close node with no associated client,' +
        `clientId: ${clientId}, streamId: ${node.streamId}`);
      return;
    }

    const remainingConnections = client.maybeCloseConnection(connection.id);
    if (remainingConnections === 0) {
      log.debug(`message: Removing empty client from list, clientId: ${client.id}`);
      clients.delete(client.id);
    }
  };

  that.addExternalInput = (streamId, url, callbackRpc) => {
    if (publishers[streamId] === undefined) {
      const client = getOrCreateClient(url);
      publishers[streamId] = new ExternalInput(url, streamId, threadPool);
      const ei = publishers[streamId];
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
    if (publishers[streamId]) publishers[streamId].addExternalOutput(url, options);
  };

  that.removeExternalOutput = (streamId, url) => {
    if (publishers[streamId] !== undefined) {
      log.info('message: Stopping ExternalOutput, ' +
        `id: ${publishers[streamId].getExternalOutput(url).id}`);
      publishers[streamId].removeExternalOutput(url);
    }
  };

  that.processSignaling = (clientId, streamId, msg) => {
    log.info('message: Process Signaling message, ' +
      `streamId: ${streamId}, clientId: ${clientId}`);
    if (publishers[streamId] !== undefined) {
      const publisher = publishers[streamId];
      if (publisher.hasSubscriber(clientId)) {
        const subscriber = publisher.getSubscriber(clientId);
        subscriber.onSignalingMessage(msg);
      } else {
        publisher.onSignalingMessage(msg);
      }
    }
  };

  /*
   * Adds a publisher to the room. This creates a new OneToManyProcessor
   * and a new WebRtcConnection. This WebRtcConnection will be the publisher
   * of the OneToManyProcessor.
   */
  that.addPublisher = (clientId, streamId, options, callbackRpc) => {
    let publisher;
    log.info('addPublisher, clientId', clientId, 'streamId', streamId);
    const client = getOrCreateClient(clientId, options.singlePC);

    if (publishers[streamId] === undefined) {
      // eslint-disable-next-line no-param-reassign
      options.publicIP = that.publicIP;
      // eslint-disable-next-line no-param-reassign
      options.privateRegexp = that.privateRegexp;
      const connection = client.getOrCreateConnection(options);
      log.info('message: Adding publisher, ' +
        `clientId: ${clientId}, ` +
        `streamId: ${streamId}`,
        logger.objectToLog(options),
        logger.objectToLog(options.metadata));
      publisher = new Publisher(clientId, streamId, connection, options);
      publishers[streamId] = publisher;
      publisher.initMediaStream();
      publisher.on('callback', onAdaptSchemeNotify.bind(this, callbackRpc));
      publisher.on('periodic_stats', onPeriodicStats.bind(this, streamId, undefined));
      publisher.on('status_event',
        onConnectionStatusEvent.bind(this, callbackRpc, clientId, streamId));
      const isNewConnection = connection.init(streamId);
      if (options.singlePC && !isNewConnection) {
        callbackRpc('callback', { type: 'initializing' });
      }
    } else {
      publisher = publishers[streamId];
      if (publisher.numSubscribers === 0) {
        log.warn('message: publisher already set but no subscribers will ignore, ' +
          `code: ${WARN_CONFLICT}, streamId: ${streamId}`,
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
  that.addSubscriber = (clientId, streamId, options, callbackRpc) => {
    const publisher = publishers[streamId];
    if (publisher === undefined) {
      log.warn('message: addSubscriber to unknown publisher, ' +
        `code: ${WARN_NOT_FOUND}, streamId: ${streamId}, ` +
        `clientId: ${clientId}`,
        logger.objectToLog(options.metadata));
      // We may need to notify the clients
      return;
    }
    let subscriber = publisher.getSubscriber(clientId);
    const client = getOrCreateClient(clientId, options.singlePC);
    if (subscriber !== undefined) {
      log.warn('message: Duplicated subscription will resubscribe, ' +
        `code: ${WARN_CONFLICT}, streamId: ${streamId}, ` +
        `clientId: ${clientId}`, logger.objectToLog(options.metadata));
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
    subscriber.initMediaStream();
    subscriber.on('callback', onAdaptSchemeNotify.bind(this, callbackRpc, 'callback'));
    subscriber.on('periodic_stats', onPeriodicStats.bind(this, clientId, streamId));
    subscriber.on('status_event',
      onConnectionStatusEvent.bind(this, callbackRpc, clientId, streamId));
    const isNewConnection = connection.init(subscriber.erizoStreamId);
    if (options.singlePC && !isNewConnection) {
      callbackRpc('callback', { type: 'initializing' });
    }
  };

  /*
   * Removes a publisher from the room. This also deletes the associated OneToManyProcessor.
   */
  that.removePublisher = (clientId, streamId, callback = () => {}) =>
    new Promise((resolve) => {
      const publisher = publishers[streamId];
      if (publisher !== undefined) {
        log.info(`message: Removing publisher, id: ${clientId}, streamId: ${streamId}`);
        publisher.forEachSubscriber((subscriberId, subscriber) => {
          log.info(`message: Removing subscriber, id: ${subscriberId}`);
          closeNode(subscriber);
          publisher.removeSubscriber(subscriberId);
        });
        publisher.removeExternalOutputs().then(() => {
          closeNode(publisher);
          delete publishers[streamId];
          publisher.muxer.close((message) => {
            log.info('message: muxer closed succesfully, ' +
              `id: ${streamId}`,
              logger.objectToLog(message));
            const count = Object.keys(publishers).length;
            log.debug(`message: remaining publishers, publisherCount: ${count}`);
            callback('callback', true);
            resolve();
            if (count === 0) {
              log.info('message: Removed all publishers. Killing process.');
              process.exit(0);
            }
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
  that.removeSubscriber = (clientId, streamId, callback = () => {}) => {
    const publisher = publishers[streamId];
    if (publisher && publisher.hasSubscriber(clientId)) {
      const subscriber = publisher.getSubscriber(clientId);
      log.info(`message: removing subscriber, streamId: ${subscriber.streamId}, ` +
        `clientId: ${clientId}`);
      closeNode(subscriber);
      publisher.removeSubscriber(clientId);
    }
    callback('callback', true);
  };

  /*
   * Removes all the subscribers related with a client.
   */
  that.removeSubscriptions = (clientId) => {
    log.info('message: removing subscriptions, clientId:', clientId);
    // we go through all the connections in the client and we close them
    forEachPublisher((publisherId, publisher) => {
      const subscriber = publisher.getSubscriber(clientId);
      if (subscriber) {
        log.debug('message: removing subscription, ' +
          'id:', subscriber.clientId);
        closeNode(subscriber);
        publisher.removeSubscriber(clientId);
      }
    });
  };

  that.getStreamStats = (streamId, callbackRpc) => {
    const stats = {};
    let publisher;
    log.debug(`message: Requested stream stats, streamID: ${streamId}`);
    const promises = [];
    if (streamId && publishers[streamId]) {
      publisher = publishers[streamId];
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

    if (streamId && publishers[streamId]) {
      publisher = publishers[streamId];

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

  return that;
};
