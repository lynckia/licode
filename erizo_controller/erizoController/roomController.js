/* global require, exports, setInterval */

/* eslint-disable no-param-reassign */

const logger = require('./../common/logger').logger;
const ErizoList = require('./models/ErizoList').ErizoList;

const StreamStates = require('./models/Stream').StreamStates;

// Logger
const log = logger.getLogger('RoomController');

exports.RoomController = (spec) => {
  const that = {};
  const maxErizosUsedByRoom = spec.maxErizosUsedByRoom ||
                                global.config.erizoController.maxErizosUsedByRoom;
  const erizos = new ErizoList(maxErizosUsedByRoom);
  // {id: ExternalOutput}
  const amqper = spec.amqper;
  const ecch = spec.ecch;
  const erizoControllerId = spec.erizoControllerId;
  const streamManager = spec.streamManager;
  const KEEPALIVE_INTERVAL = 5 * 1000;
  const TIMEOUT_LIMIT = 2;
  const MAX_ERIZOJS_RETRIES = 3;
  const eventListeners = [];

  let getErizoJS;
  let currentErizo = 0;

  const dispatchEvent = (type, evt) => {
    eventListeners.forEach((eventListener) => {
      eventListener(type, evt);
    });
  };

  const callbackFor = function callbackFor(erizoId) {
    return (ok) => {
      const erizo = erizos.findById(erizoId);
      if (!erizo) return;

      if (ok !== true) {
        erizo.kaCount += 1;

        if (erizo.kaCount > TIMEOUT_LIMIT) {
          const streamsInErizo = streamManager.getPublishersInErizoId(erizoId);
          if (streamsInErizo.length > 0) {
            log.error('message: ErizoJS timed out will be removed, ' +
              `erizoId: ${erizoId}, ` +
              `publishersAffected: ${streamsInErizo.length}`);
            streamsInErizo.forEach((publisher) => {
              dispatchEvent('unpublish', publisher.id);
            });
          } else {
            log.debug(`message: empty erizoJS removed, erizoId: ${erizoId}`);
          }
          ecch.deleteErizoJS(erizoId);
          erizos.deleteById(erizoId);
        }
      } else {
        erizo.kaCount = 0;
      }
    };
  };

  const sendKeepAlive = () => {
    erizos.forEachUniqueErizo((erizo) => {
      const erizoId = erizo.erizoId;
      amqper.callRpc(`ErizoJS_${erizoId}`, 'keepAlive', [], { callback: callbackFor(erizoId) });
    });
  };

  setInterval(sendKeepAlive, KEEPALIVE_INTERVAL);

  const waitForErizoInfoIfPending = (position, callback) => {
    if (erizos.isPending(position)) {
      log.debug(`message: Waiting for new ErizoId, position: ${position}`);
      erizos.onErizoReceived(position, () => {
        log.debug(`message: ErizoId received so trying again, position: ${position}`);
        getErizoJS(callback, position);
      });
      return true;
    }
    return false;
  };

  getErizoJS = (callback, previousPosition = undefined) => {
    let agentId;
    let erizoIdForAgent;
    const erizoPosition = previousPosition !== undefined ? previousPosition : currentErizo += 1;

    if (waitForErizoInfoIfPending(erizoPosition, callback)) {
      return;
    }

    const erizo = erizos.get(erizoPosition);
    if (!erizo.erizoId) {
      erizos.markAsPending(erizoPosition);
    } else {
      agentId = erizo.agentId;
      erizoIdForAgent = erizo.erizoIdForAgent;
    }

    log.debug(`message: Getting ErizoJS, agentId: ${agentId}, ` +
            `erizoIdForAgent: ${erizoIdForAgent}`);
    ecch.getErizoJS(agentId, erizoIdForAgent, (gotErizoId, gotAgentId, gotErizoIdForAgent) => {
      const theErizo = erizos.get(erizoPosition);
      if (!theErizo.erizoId && gotErizoId !== 'timeout') {
        erizos.set(erizoPosition, gotErizoId, gotAgentId, gotErizoIdForAgent);
      } else if (theErizo.erizoId) {
        theErizo.agentId = gotAgentId;
        theErizo.erizoIdForAgent = gotErizoIdForAgent;
      }
      callback(gotErizoId, gotAgentId, gotErizoIdForAgent);
    });
  };

  const getErizoQueueFromStreamId = (streamId) => {
    const erizoId = streamManager.getErizoIdForPublishedStreamId(streamId);
    return `ErizoJS_${erizoId}`;
  };
  const getErizoQueueFromErizoId = erizoId => `ErizoJS_${erizoId}`;


  that.addEventListener = (eventListener) => {
    eventListeners.push(eventListener);
  };

  that.addExternalInput = (clientId, publisherId, url, label, callback) => {
    if (streamManager.getPublishedStreamState(publisherId) === StreamStates.PUBLISHER_CREATED) {
      log.info(`message: addExternalInput, clientId ${clientId}, streamId: ${publisherId}, url:${url}`);
      getErizoJS((erizoId) => {
        if (erizoId === 'timeout') {
          log.error(`message: addExternalInput ErizoAgent timeout, streamId: ${publisherId}`);
          callback('timeout-agent');
          return;
        }
        streamManager.updateErizoIdForPublishedStream(publisherId, erizoId);
        const args = [erizoControllerId, publisherId, url, label];
        amqper.callRpc(getErizoQueueFromStreamId(publisherId), 'addExternalInput', args,
          { callback }, 20000);
      });
    } else {
      log.warn(`message: addExternalInput already in progress, clientId: ${clientId}, streamId: ${publisherId},  url: ${url}`);
    }
  };

  that.addExternalOutput = (publisherId, url, options, callback) => {
    if (streamManager
      .getPublishedStreamById(publisherId)
      .getExternalOutputSubscriberState(url) === StreamStates.SUBSCRIBER_CREATED) {
      log.info(`message: addExternalOuput, streamId: ${publisherId}, url:${url}`);

      const args = [publisherId, url, options];
      amqper.callRpc(getErizoQueueFromStreamId(publisherId), 'addExternalOutput', args, undefined);
      callback(true);
    } else {
      log.warn(`message: addExternalOutput already in progress, publisherId: ${publisherId}, url: ${url} `,
        logger.objectToLog(options.metadata));
    }
  };

  that.removeExternalOutput = (publisherId, url, callback) => {
    log.info(`removeExternalOutput, url: ${url}`);

    const args = [publisherId, url];
    amqper.callRpc(getErizoQueueFromStreamId(publisherId), 'removeExternalOutput', args, undefined);
    callback(true);
  };

  that.processConnectionMessageFromClient = (erizoId, clientId, connectionId, msg, callback) => {
    const args = [erizoControllerId, clientId, connectionId, msg];
    amqper.callRpc(getErizoQueueFromErizoId(erizoId), 'processConnectionMessage', args, { callback });
  };

  that.processStreamMessageFromClient = (erizoId, clientId, streamId, msg) => {
    const args = [erizoControllerId, clientId, streamId, msg];
    amqper.callRpc(getErizoQueueFromErizoId(erizoId), 'processStreamMessage', args, {});
  };

  /*
     * Adds a publisher to the room. This creates a new OneToManyProcessor
     * and a new WebRtcConnection. This WebRtcConnection will be the publisher
     * of the OneToManyProcessor.
     */
  that.addPublisher = (clientId, streamId, options, callback, retries) => {
    if (retries === undefined) {
      retries = 0;
    }

    if (streamManager.getPublishedStreamState(streamId) === StreamStates.PUBLISHER_CREATED) {
      log.info('message: addPublisher, ',
        `clientId ${clientId}, `,
        `streamId: ${streamId}`,
        logger.objectToLog(options),
        logger.objectToLog(options.metadata));

      // We create a new ErizoJS with the streamId.
      getErizoJS((erizoId, agentId) => {
        if (erizoId === 'timeout') {
          log.error(`message: addPublisher ErizoAgent timeout, streamId: ${streamId}`,
            logger.objectToLog(options.metadata));
          callback('timeout-agent');
          return;
        }
        log.info('message: addPublisher erizoJs assigned, ',
          `erizoId: ${erizoId}, streamId: ${streamId}`,
          logger.objectToLog(options.metadata));
        // Track publisher locally
        // then we call its addPublisher method.
        const args = [erizoControllerId, clientId, streamId, options];
        streamManager.updateErizoIdForPublishedStream(streamId, erizoId);

        amqper.callRpc(getErizoQueueFromStreamId(streamId), 'addPublisher', args,
          { callback: (data) => {
            if (data === 'timeout') {
              if (retries < MAX_ERIZOJS_RETRIES) {
                log.warn('message: addPublisher ErizoJS timeout, ',
                  `streamId: ${streamId}, `,
                  `erizoId: ${getErizoQueueFromStreamId(streamId)}, `,
                  `retries: ${retries}, `,
                  logger.objectToLog(options.metadata));
                retries += 1;
                callback('timeout-erizojs-retry');
                streamManager.updateErizoIdForPublishedStream(streamId, null);
                that.addPublisher(clientId, streamId, options, callback, retries);
                return;
              }
              log.warn('message: addPublisher ErizoJS timeout no retry, ',
                `retries: ${retries}, streamId: ${streamId}, `,
                `erizoId: ${getErizoQueueFromStreamId(streamId)},`,
                logger.objectToLog(options.metadata));
              streamManager.updateErizoIdForPublishedStream(streamId, null);
              callback('timeout-erizojs');
            } else {
              // TODO (javier): Check new path for this
              if (data.type === 'initializing') {
                data.agentId = agentId;
                data.erizoId = erizoId;
              }
              callback(data);
            }
          } });
      });
    } else {
      log.warn(`message: addPublisher already in progress, clientId: ${clientId}, streamId: ${streamId} `,
        logger.objectToLog(options.metadata));
    }
  };

  /*
     * Adds a subscriber to the room. This creates a new WebRtcConnection.
     * This WebRtcConnection will be added to the subscribers list of the
     * OneToManyProcessor.
     */
  that.addSubscriber = (clientId, streamId, options, callback, retries) => {
    if (clientId === null) {
      log.warn('message: addSubscriber null clientId, ',
        `streamId: ${streamId}, `,
        `clientId: ${clientId},`,
        logger.objectToLog(options.metadata));
      callback('Error: null clientId');
      return;
    }
    if (retries === undefined) { retries = 0; }

    if (streamManager
      .getPublishedStreamById(streamId)
      .getAvSubscriberState(clientId) === StreamStates.SUBSCRIBER_CREATED) {
      log.info('message: addSubscriber, ',
        `streamId: ${streamId}, `,
        `clientId: ${clientId},`,
        logger.objectToLog(options),
        logger.objectToLog(options.metadata));

      if (options.audio === undefined) options.audio = true;
      if (options.video === undefined) options.video = true;

      const args = [erizoControllerId, clientId, streamId, options];

      amqper.callRpc(getErizoQueueFromStreamId(streamId), 'addSubscriber', args,
        { callback: (data) => {
          if (!streamManager.hasPublishedStream(streamId)) {
            log.warn('message: addSubscriber rpc callback has arrived after ',
              'publisher is removed, ',
              `streamId: ${streamId}, `,
              `clientId: ${clientId},`,
              logger.objectToLog(options.metadata));
            callback('timeout');
            return;
          }
          if (data === 'timeout') {
            if (retries < MAX_ERIZOJS_RETRIES) {
              retries += 1;
              log.warn('message: addSubscriber ErizoJS timeout, ',
                `clientId: ${clientId}, `,
                `streamId: ${streamId}, `,
                `erizoId: ${getErizoQueueFromStreamId(streamId)}, `,
                `retries: ${retries},`,
                logger.objectToLog(options.metadata));
              that.addSubscriber(clientId, streamId, options, callback, retries);
              return;
            }
            log.error('message: addSubscriber ErizoJS timeout no retry, ',
              `clientId: ${clientId}, `,
              `streamId: ${streamId}, `,
              `erizoId: ${getErizoQueueFromStreamId(streamId)},`,
              logger.objectToLog(options.metadata));
            callback('timeout');
            return;
          }
          log.info('message: addSubscriber finished, ',
            `streamId: ${streamId}, `,
            `clientId: ${clientId},`,
            logger.objectToLog(options),
            logger.objectToLog(options.metadata));
          data.erizoId = streamManager.getErizoIdForPublishedStreamId(streamId);
          callback(data);
        } });
    } else {
      log.warn(`message: addSubscriber already in progress, clientId: ${clientId}, streamId: ${streamId} `,
        logger.objectToLog(options.metadata));
    }
  };

  that.addMultipleSubscribers = (clientId, streamIds, options, callback, retries) => {
    if (clientId === null) {
      log.warn('message: addMultipleSubscribers null clientId, ',
        `streams: ${streamIds.length}, `,
        `clientId: ${clientId},`,
        logger.objectToLog(options.metadata));
      callback('Error: null clientId');
      return;
    }

    if (retries === undefined) { retries = 0; }


    if (streamIds.length === 0) {
      return;
    }

    const erizoIds = Array.from(new Set(streamIds.map(streamId =>
      getErizoQueueFromStreamId(streamId))));

    erizoIds.forEach((erizoId) => {
      const streamIdsInErizo = streamIds.filter(streamId =>
        getErizoQueueFromStreamId(streamId) === erizoId);
      const args = [erizoControllerId, clientId, streamIdsInErizo, options];
      log.info('message: addMultipleSubscribers, ',
        `streams: ${streamIdsInErizo}, `,
        `clientId: ${clientId},`,
        logger.objectToLog(options),
        logger.objectToLog(options.metadata));

      amqper.callRpc(erizoId, 'addMultipleSubscribers', args,
        { callback: (data) => {
          if (data === 'timeout') {
            if (retries < MAX_ERIZOJS_RETRIES) {
              retries += 1;
              log.warn('message: addMultipleSubscribers ErizoJS timeout, ',
                `clientId: ${clientId}, `,
                `streams: ${streamIdsInErizo}, `,
                `erizoId: ${erizoId}, `,
                `retries: ${retries},`,
                logger.objectToLog(options.metadata));
              that.addMultipleSubscribers(clientId, streamIdsInErizo, options, callback, retries);
              return;
            }
            log.warn('message: addMultipleSubscribers ErizoJS timeout no retry, ',
              `clientId: ${clientId}, `,
              `streams: ${streamIdsInErizo.length}, `,
              `erizoId: ${erizoId},`,
              logger.objectToLog(options.metadata));
            callback('timeout');
            return;
          }
          log.info('message: addMultipleSubscribers finished, ',
            `streams: ${streamIdsInErizo}, `,
            `clientId: ${clientId},`,
            logger.objectToLog(options),
            logger.objectToLog(options.metadata));
          data.erizoId = streamManager.getErizoIdForPublishedStreamId(streamIdsInErizo[0]);
          callback(data);
        },
        });
    });
  };

  /*
     * Removes a publisher from the room. This also deletes the associated OneToManyProcessor.
     */
  that.removePublisher = (clientId, streamId, callback) => {
    log.info('message: removePublisher, ' +
      `streamId: ${streamId}, ` +
      `erizoId: ${getErizoQueueFromStreamId(streamId)}`);

    const args = [clientId, streamId];
    amqper.callRpc(getErizoQueueFromStreamId(streamId), 'removePublisher', args, {
      callback: () => {
        log.debug('message: removedPublisher, ' +
          `streamId: ${streamId}, ` +
          `publishersLeft: ${streamManager.publishedStreams.size}`);
        if (callback) {
          callback(true);
        }
      } });
  };

  /*
     * Removes a subscriber from the room.
     * This also removes it from the associated OneToManyProcessor.
     */
  that.removeSubscriber = (subscriberId, streamId, callback = () => {}) => {
    log.info('message: removeSubscriber, ' +
      `clientId: ${subscriberId}, ` +
      `streamId: ${streamId}`);

    const args = [subscriberId, streamId];
    amqper.callRpc(getErizoQueueFromStreamId(streamId), 'removeSubscriber', args, {
      callback: (message) => {
        log.info('message: removeSubscriber finished, ' +
          `response: ${message}, ` +
          `clientId: ${subscriberId}, ` +
          `streamId: ${streamId}`);
        callback(true);
      } });
  };

  /*
   * Removes a subscriber from the room.
   * This also removes it from the associated OneToManyProcessor.
   */
  that.removeMultipleSubscribers = (subscriberId, streamIds, callback) => {
    const erizoIds = Array.from(new Set(streamIds.map(streamId =>
      getErizoQueueFromStreamId(streamId))));

    erizoIds.forEach((erizoId) => {
      const streamIdsInErizo = streamIds.filter(streamId =>
        getErizoQueueFromStreamId(streamId) === erizoId);
      log.info('message: removeMultipleSubscribers, ' +
                       `clientId: ${subscriberId}, ` +
                       `streamIds: ${streamIdsInErizo}`);
      const args = [subscriberId, streamIdsInErizo];
      amqper.callRpc(erizoId, 'removeMultipleSubscribers', args, {
        callback: (data) => {
          log.info('message: removeMultipleSubscribers finished, ' +
                    `clientId: ${subscriberId}, ` +
                    `streamIds: ${streamIds}`);
          callback(data);
        },
      });
    });
  };


  that.getStreamStats = (streamId, callback) => {
    if (!streamManager.hasPublishedStream(streamId)) {
      log.warn('message: getStreamStats publisher not found, ' +
        `streamId: ${streamId}`);
      return;
    }
    const args = [streamId];
    const theId = getErizoQueueFromStreamId(streamId);
    log.debug('Get stats for publisher ', streamId, 'theId', theId);
    amqper.callRpc(getErizoQueueFromStreamId(streamId), 'getStreamStats', args, {
      callback: (data) => {
        callback(data);
      } });
  };

  that.removeClient = (clientId) => {
    log.info(`message: removeClient clientId ${clientId}`);
    erizos.forEachUniqueErizo((erizo) => {
      const erizoId = erizo.erizoId;
      const args = [clientId];
      log.info(`message: removeClient - calling ErizoJS to remove client, erizoId: ${erizoId}, clientId: ${clientId}`);
      amqper.callRpc(`ErizoJS_${erizoId}`, 'removeClient', args, {
        callback: (result) => {
          log.info(`message: removeClient - result from erizoJS ${erizo.erizoId}, result ${result}`);
        } });
    });
  };

  return that;
};
