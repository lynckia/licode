/* global require, exports, setInterval */

/* eslint-disable no-param-reassign */

const logger = require('./../common/logger').logger;
const ErizoList = require('./models/ErizoList').ErizoList;

// Logger
const log = logger.getLogger('RoomController');

exports.RoomController = (spec) => {
  const that = {};
        // {id: array of subscribers}
  const subscribers = {};
        // {id: erizoJS_id}
  const publishers = {};
  const maxErizosUsedByRoom = spec.maxErizosUsedByRoom ||
                                global.config.erizoController.maxErizosUsedByRoom;
  const erizos = new ErizoList(maxErizosUsedByRoom);
        // {id: ExternalOutput}
  const externalOutputs = {};
  const amqper = spec.amqper;
  const ecch = spec.ecch;
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
          if (erizo.publishers.length > 0) {
            log.error('message: ErizoJS timed out will be removed, ' +
              `erizoId: ${erizoId}, ` +
              `publishersAffected: ${erizo.publishers.length}`);
            erizo.publishers.forEach((publisher) => {
              dispatchEvent('unpublish', publisher);
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
    erizos.forEachExisting((erizo) => {
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

  const getErizoQueue = streamId => `ErizoJS_${publishers[streamId]}`;

  that.addEventListener = (eventListener) => {
    eventListeners.push(eventListener);
  };

  that.addExternalInput = (publisherId, url, callback) => {
    if (publishers[publisherId] === undefined) {
      log.info(`message: addExternalInput,  streamId: ${publisherId}, url:${url}`);

      getErizoJS((erizoId) => {
                // then we call its addPublisher method.
        const args = [publisherId, url];

                // Track publisher locally
        publishers[publisherId] = erizoId;
        subscribers[publisherId] = [];

        amqper.callRpc(getErizoQueue(publisherId), 'addExternalInput', args,
                               { callback }, 20000);

        erizos.findById(erizoId).publishers.push(publisherId);
      });
    } else {
      log.info('message: addExternalInput publisher already set, ' +
                     `streamId: ${publisherId}, url: ${url}`);
    }
  };

  that.addExternalOutput = (publisherId, url, options, callback) => {
    if (publishers[publisherId] !== undefined) {
      log.info(`message: addExternalOuput, streamId: ${publisherId}, url:${url}`);

      const args = [publisherId, url, options];

      amqper.callRpc(getErizoQueue(publisherId), 'addExternalOutput', args, undefined);

            // Track external outputs
      externalOutputs[url] = publisherId;

      callback('success');
    } else {
      callback('error');
    }
  };

  that.removeExternalOutput = (url, callback) => {
    const publisherId = externalOutputs[url];

    if (publisherId !== undefined && publishers[publisherId] !== undefined) {
      log.info(`removeExternalOutput, url: ${url}`);

      const args = [publisherId, url];
      amqper.callRpc(getErizoQueue(publisherId), 'removeExternalOutput', args, undefined);

            // Remove track
      delete externalOutputs[url];
      callback(true);
    } else {
      log.warn('message: removeExternalOutput no publisher, ' +
                     `publisherId: ${publisherId}, ` +
                     `url: ${url}`);
      callback(null, 'This stream is not being recorded');
    }
  };

  that.processSignaling = (clientId, streamId, msg) => {
    if (publishers[streamId] !== undefined) {
      log.info('message: processSignaling, ' +
                     `clientId: ${clientId}, ` +
                     `streamId: ${streamId}`);
      const args = [clientId, streamId, msg];
      amqper.callRpc(getErizoQueue(streamId), 'processSignaling', args, {});
    } else {
      log.warn('message: processSignaling no publisher, ' +
                     `clientId: ${clientId}, ` +
                     `streamId: ${streamId}`);
    }
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

    if (publishers[streamId] === undefined) {
      log.info('message: addPublisher, ' +
                     `clientId ${clientId}, ` +
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
        log.info('message: addPublisher erizoJs assigned, ' +
                        `erizoId: ${erizoId}, streamId: ${streamId}`,
                         logger.objectToLog(options.metadata));
                // Track publisher locally
                // then we call its addPublisher method.
        const args = [clientId, streamId, options];
        publishers[streamId] = erizoId;
        subscribers[streamId] = [];

        amqper.callRpc(getErizoQueue(streamId), 'addPublisher', args,
          { callback: (data) => {
            if (data === 'timeout') {
              if (retries < MAX_ERIZOJS_RETRIES) {
                log.warn('message: addPublisher ErizoJS timeout, ' +
                                     `streamId: ${streamId}, ` +
                                     `erizoId: ${getErizoQueue(streamId)}, ` +
                                     `retries: ${retries}, `,
                                     logger.objectToLog(options.metadata));
                publishers[streamId] = undefined;
                retries += 1;
                that.addPublisher(clientId, streamId, options, callback, retries);
                return;
              }
              log.warn('message: addPublisher ErizoJS timeout no retry, ' +
                                 `retries: ${retries}, streamId: ${streamId}, ` +
                                 `erizoId: ${getErizoQueue(streamId)},`,
                                 logger.objectToLog(options.metadata));
              const erizo = erizos.findById(publishers[streamId]);
              if (erizo !== undefined) {
                const index = erizo.publishers.indexOf(streamId);
                erizo.publishers.splice(index, 1);
              }
              callback('timeout-erizojs');
            } else {
              if (data.type === 'initializing') {
                data.agentId = agentId;
                data.erizoId = erizoId;
              }
              callback(data);
            }
          } });

        erizos.findById(erizoId).publishers.push(streamId);
      });
    } else {
      log.warn(`message: addPublisher already set, clientId: ${clientId}, streamId: ${streamId} `,
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
      log.warn('message: addSubscriber null clientId, ' +
                       `streamId: ${streamId}, ` +
                       `clientId: ${clientId},`,
                       logger.objectToLog(options.metadata));
      callback('Error: null clientId');
      return;
    }
    if (retries === undefined) { retries = 0; }

    if (publishers[streamId] !== undefined &&
            subscribers[streamId].indexOf(clientId) === -1) {
      log.info('message: addSubscriber, ' +
                     `streamId: ${streamId}, ` +
                     `clientId: ${clientId},`,
                     logger.objectToLog(options),
                     logger.objectToLog(options.metadata));

      if (options.audio === undefined) options.audio = true;
      if (options.video === undefined) options.video = true;

      const args = [clientId, streamId, options];

      amqper.callRpc(getErizoQueue(streamId, undefined), 'addSubscriber', args,
        { callback: (data) => {
          if (!publishers[streamId] && !subscribers[streamId]) {
            log.warn('message: addSubscriber rpc callback has arrived after ' +
                             'publisher is removed, ' +
                             `streamId: ${streamId}, ` +
                             `clientId: ${clientId},`,
                             logger.objectToLog(options.metadata));
            callback('timeout');
            return;
          }
          if (data === 'timeout') {
            if (retries < MAX_ERIZOJS_RETRIES) {
              retries += 1;
              log.warn('message: addSubscriber ErizoJS timeout, ' +
                                 `clientId: ${clientId}, ` +
                                 `streamId: ${streamId}, ` +
                                 `erizoId: ${getErizoQueue(streamId)}, ` +
                                 `retries: ${retries},`,
                                 logger.objectToLog(options.metadata));
              that.addSubscriber(clientId, streamId, options, callback, retries);
              return;
            }
            log.warn('message: addSubscriber ErizoJS timeout no retry, ' +
                             `clientId: ${clientId}, ` +
                             `streamId: ${streamId}, ` +
                             `erizoId: ${getErizoQueue(streamId)},`,
                             logger.objectToLog(options.metadata));
            callback('timeout');
            return;
          } else if (data.type === 'initializing') {
            subscribers[streamId].push(clientId);
          }
          log.info('message: addSubscriber finished, ' +
                         `streamId: ${streamId}, ` +
                         `clientId: ${clientId},`,
                         logger.objectToLog(options),
                         logger.objectToLog(options.metadata));
          data.erizoId = publishers[streamId];
          callback(data);
        } });
    } else {
      let message = '';
      if (publishers[streamId] !== undefined) {
        message = 'publisher not found';
      }
      if (subscribers[streamId] && subscribers[streamId].indexOf(clientId) === -1) {
        message = 'subscriber already exists';
      }
      log.warn('message: addSubscriber can not be requested, ' +
                     `reason: ${message}` +
                     `streamId: ${streamId}, ` +
                     `clientId: ${clientId},`,
                     logger.objectToLog(options),
                     logger.objectToLog(options.metadata));
    }
  };

    /*
     * Removes a publisher from the room. This also deletes the associated OneToManyProcessor.
     */
  that.removePublisher = (clientId, streamId, callback) => {
    if (subscribers[streamId] !== undefined && publishers[streamId] !== undefined) {
      log.info('message: removePublisher, ' +
                     `streamId: ${streamId}, ` +
                     `erizoId: ${getErizoQueue(streamId)}`);

      const args = [clientId, streamId];
      amqper.callRpc(getErizoQueue(streamId), 'removePublisher', args, {
        callback: () => {
          const erizo = erizos.findById(publishers[streamId]);

          if (erizo) {
            const index = erizo.publishers.indexOf(streamId);
            erizo.publishers.splice(index, 1);
          } else {
            log.warn('message: removePublisher was already removed, ' +
                             `streamId: ${streamId}, ` +
                             `erizoId: ${getErizoQueue(streamId)}`);
          }

          delete subscribers[streamId];
          delete publishers[streamId];
          log.debug('message: removedPublisher, ' +
                          `streamId: ${streamId}, ` +
                          `publishersLeft: ${Object.keys(publishers).length}`);
          if (callback) {
            callback(true);
          }
        } });
    } else {
      let message = '';
      if (publishers[streamId] === undefined) {
        message = 'publisher not found';
      }
      if (subscribers[streamId] === undefined) {
        message = 'subscriber not found';
      }
      log.warn('message: removePublisher not found, ' +
                     `reason: ${message}, ` +
                     `clientId: ${clientId}, ` +
                     `streamId: ${streamId}`);
    }
  };

    /*
     * Removes a subscriber from the room.
     * This also removes it from the associated OneToManyProcessor.
     */
  that.removeSubscriber = (subscriberId, streamId, callback) => {
    if (subscribers[streamId] !== undefined) {
      const index = subscribers[streamId].indexOf(subscriberId);
      if (index !== -1) {
        log.info('message: removeSubscriber, ' +
                         `clientId: ${subscriberId}, ` +
                         `streamId: ${streamId}`);

        const args = [subscriberId, streamId];
        amqper.callRpc(getErizoQueue(streamId), 'removeSubscriber', args, {
          callback: (message) => {
            log.info('message: removeSubscriber finished, ' +
                             `response: ${message}, ` +
                             `clientId: ${subscriberId}, ` +
                             `streamId: ${streamId}`);
            const newIndex = subscribers[streamId].indexOf(subscriberId);
            subscribers[streamId].splice(newIndex, 1);
            callback(true);
          } });
      }
    } else {
      log.warn('message: removeSubscriber not found, ' +
                     `clientId: ${subscriberId}, ` +
                     `streamId: ${streamId}`);
    }
  };

    /*
     * Removes all the subscribers related with a client.
     */
  that.removeSubscriptions = (subscriberId) => {
    let index;

    log.info(`message: removeSubscriptions, clientId: ${subscriberId}`);

    const subscriberStreamIds = Object.keys(subscribers);
    for (let i = 0; i < subscriberStreamIds.length; i += 1) {
      const streamId = subscriberStreamIds[i];
      index = subscribers[streamId].indexOf(subscriberId);
      if (index !== -1) {
        log.debug('message: removeSubscriptions, ' +
            `clientId: ${subscriberId}, ` +
            `streamId: ${streamId}`);

        const args = [subscriberId, streamId];
        amqper.callRpc(getErizoQueue(streamId), 'removeSubscriber', args, undefined);

        // Remove tracks
        subscribers[streamId].splice(index, 1);
      }
    }
  };

  that.getStreamStats = (streamId, callback) => {
    if (publishers[streamId]) {
      const args = [streamId];
      const theId = getErizoQueue(streamId);
      log.debug('Get stats for publisher ', streamId, 'theId', theId);
      amqper.callRpc(getErizoQueue(streamId), 'getStreamStats', args, {
        callback: (data) => {
          callback(data);
        } });
    } else {
      log.warn('message: getStreamStats publisher not found, ' +
                     `streamId: ${streamId}`);
    }
  };

  return that;
};
