/*global require, exports, setInterval*/
'use strict';
var logger = require('./../common/logger').logger;
var EventEmitter = require('events').EventEmitter;

// Logger
var log = logger.getLogger('RoomController');

const MAX_ERIZOS_PER_ROOM = 100;

class ErizoList extends EventEmitter {
  constructor(maxErizos = MAX_ERIZOS_PER_ROOM) {
    super();
    this.maxErizos = maxErizos;
    this.erizos = new Array(maxErizos);
    this.erizos.fill(1);
    this.erizos = this.erizos.map(() => {
      return {
        pending: false,
        erizoId: undefined,
        agentId: undefined,
        internalId: undefined,
        publishers: [],
        kaCount: 0,
      };
    });
  }

  findById(erizoId) {
    return this.erizos.find((erizo) => {
      return erizo.erizoId === erizoId;
    });
  }

  getInternalPosition(position) {
    return position % this.maxErizos;
  }

  isPending(position) {
    return this.erizos[this.getInternalPosition(position)].pending;
  }

  get(position) {
    return this.erizos[this.getInternalPosition(position)];
  }

  forEachExisting(task) {
    this.erizos.forEach((erizo) => {
      if (erizo.erizoId) {
        task(erizo);
      }
    });
  }

  deleteById(erizoId) {
    const erizo = this.findById(erizoId);
    erizo.pending = false;
    erizo.erizoId = undefined;
    erizo.agentId = undefined;
    erizo.internalId = undefined;
    erizo.publishers = [];
    erizo.kaCount = 0;
  }

  markAsPending(position) {
    this.erizos[this.getInternalPosition(position)].pending = true;
  }

  set(position, erizoId, agentId, internalId) {
    const erizo = this.erizos[this.getInternalPosition(position)];
    erizo.pending = false;
    erizo.erizoId = erizoId;
    erizo.agentId = agentId;
    erizo.internalId = internalId;
    this.emit(position, erizo);
  }
}

exports.RoomController = function (spec) {
    var that = {},

        getErizoJS,

        // {id: array of subscribers}
        subscribers = {},
        // {id: erizoJS_id}
        publishers = {},

        maxErizosUsedByRoom = spec.maxErizosUsedByRoom || 
                                global.config.erizoController.maxErizosUsedByRoom,

        erizos = new ErizoList(maxErizosUsedByRoom),
        currentErizo = 0,

        // {id: ExternalOutput}
        externalOutputs = {};

    var amqper = spec.amqper;
    var ecch = spec.ecch;

    var KEEPALIVE_INTERVAL = 5*1000;
    var TIMEOUT_LIMIT = 2;
    var MAX_ERIZOJS_RETRIES = 3;

    var eventListeners = [];

    var dispatchEvent = function(type, evt) {
        for (var eventId in eventListeners) {
            eventListeners[eventId](type, evt);
        }

    };

    var callbackFor = function(erizoId) {

        return function(ok) {
            const erizo = erizos.findById(erizoId);
            if (!erizo) return;

            if (ok !== true) {
                erizo.kaCount ++;

                if (erizo.kaCount > TIMEOUT_LIMIT) {
                    if (erizo.publishers.length > 0){
                        log.error('message: ErizoJS timed out will be removed, ' +
                                  'erizoId: ' + erizoId + ', ' +
                                  'publishersAffected: ' + erizo.publishers.length);
                        for (var p in erizo.publishers) {
                            dispatchEvent('unpublish', erizo.publishers[p]);
                        }

                    } else {
                        log.debug('message: empty erizoJS removed, erizoId: ' + erizoId);
                    }
                    ecch.deleteErizoJS(erizoId);
                    erizos.deleteById(erizoId);
                }
            } else {
                erizo.kaCount = 0;
            }
        };
    };

    var sendKeepAlive = function() {
        erizos.forEachExisting(erizo => {
            const erizoId = erizo.erizoId;
            amqper.callRpc('ErizoJS_' + erizoId, 'keepAlive', [], {callback: callbackFor(erizoId)});
        });
    };

    setInterval(sendKeepAlive, KEEPALIVE_INTERVAL);

    const waitForErizoInfoIfPending = (position, callback) => {
      if (erizos.isPending(position)) {
        erizos.once(erizos.getInternalPosition(position), () => {
          getErizoJS(callback, position);
        });
        return true;
      }
      return false;
    };

    getErizoJS = function(callback, previousPosition = undefined) {
      let agentId;
      let internalId;
      const erizoPosition = previousPosition !== undefined ? previousPosition : currentErizo++;

      if (waitForErizoInfoIfPending(erizoPosition, callback)) {
        return;
      }

      const erizo = erizos.get(erizoPosition);
      if (!erizo.erizoId) {
        erizos.markAsPending(erizoPosition);
      } else {
        agentId = erizo.agentId;
        internalId = erizo.internalId;
      }

      log.debug('message: Getting ErizoJS, agentId: ' + agentId + ', internalId: ' + internalId);
      ecch.getErizoJS(agentId, internalId, function(erizoId, agentId, internalId) {
        const erizo = erizos.get(erizoPosition);
        if (!erizo.erizoId && erizoId !== 'timeout') {
          erizos.set(erizoPosition, erizoId, agentId, internalId);
        } else if (erizo.erizoId) {
          erizo.agentId = agentId;
          erizo.internalId = internalId;
        }
        callback(erizoId, agentId, internalId);
      });
    };

    var getErizoQueue = function(streamId) {
        return 'ErizoJS_' + publishers[streamId];
    };

    that.addEventListener = function(eventListener) {
        eventListeners.push(eventListener);
    };

    that.addExternalInput = function (publisherId, url, callback) {

        if (publishers[publisherId] === undefined) {

            log.info('message: addExternalInput,  streamId: ' + publisherId + ', url:' + url);

            getErizoJS(function(erizoId) {
                // then we call its addPublisher method.
    	        var args = [publisherId, url];

                // Track publisher locally
                publishers[publisherId] = erizoId;
                subscribers[publisherId] = [];

                amqper.callRpc(getErizoQueue(publisherId), 'addExternalInput', args,
                               {callback: callback}, 20000);

                erizos.findById(erizoId).publishers.push(publisherId);

            });
        } else {
            log.info('message: addExternalInput publisher already set, ' +
                     'streamId: ' + publisherId + ', url: ' + url);
        }
    };

    that.addExternalOutput = function (publisherId, url, options, callback) {
        if (publishers[publisherId] !== undefined) {
            log.info('message: addExternalOuput, streamId: ' + publisherId + ', url:' + url);

            var args = [publisherId, url, options];

            amqper.callRpc(getErizoQueue(publisherId), 'addExternalOutput', args, undefined);

            // Track external outputs
            externalOutputs[url] = publisherId;

            callback('success');
        } else {
            callback('error');
        }

    };

    that.removeExternalOutput = function (url, callback) {
        var publisherId = externalOutputs[url];

        if (publisherId !== undefined && publishers[publisherId] !== undefined) {
            log.info('removeExternalOutput, url: ' + url);

            var args = [publisherId, url];
            amqper.callRpc(getErizoQueue(publisherId), 'removeExternalOutput', args, undefined);

            // Remove track
            delete externalOutputs[url];
            callback(true);
        } else {
            callback(null, 'This stream is not being recorded');
        }
    };

    that.processSignaling = function (clientId, streamId, msg) {

        if (publishers[streamId] !== undefined) {
            var args = [clientId, streamId, msg];
            amqper.callRpc(getErizoQueue(streamId), 'processSignaling', args, {});

        }
    };

    /*
     * Adds a publisher to the room. This creates a new OneToManyProcessor
     * and a new WebRtcConnection. This WebRtcConnection will be the publisher
     * of the OneToManyProcessor.
     */
    that.addPublisher = function (clientId, streamId, options, callback, retries) {
        if (retries === undefined) {
            retries = 0;
        }

        if (publishers[streamId] === undefined) {

            log.info('message: addPublisher, ' +
                     'clientId ' + clientId + ', ' +
                     'streamId: ' + streamId + ', ' +
                     logger.objectToLog(options) + ', ' +
                     logger.objectToLog(options.metadata));

            // We create a new ErizoJS with the streamId.
            getErizoJS(function(erizoId, agentId) {

                if (erizoId === 'timeout') {
                    log.error('message: addPublisher ErizoAgent timeout, streamId: ' +
                              streamId + ', ' + logger.objectToLog(options.metadata));
                    callback('timeout-agent');
                    return;
                }
                log.info('message: addPublisher erizoJs assigned, ' +
                        'erizoId: ' + erizoId + ', streamId: ', streamId +
                         ', ' + logger.objectToLog(options.metadata));
                // Track publisher locally
                // then we call its addPublisher method.
                var args = [clientId, streamId, options];
                publishers[streamId] = erizoId;
                subscribers[streamId] = [];

                amqper.callRpc(getErizoQueue(streamId), 'addPublisher', args,
                              {callback: function (data){
                    if (data === 'timeout'){
                        if (retries < MAX_ERIZOJS_RETRIES){
                            log.warn('message: addPublisher ErizoJS timeout, ' +
                                     'streamId: ' + streamId + ', ' +
                                     'erizoId: ' + getErizoQueue(streamId) + ', ' +
                                     'retries: ' + retries + ', ' +
                                     logger.objectToLog(options.metadata));
                            publishers[streamId] = undefined;
                            retries++;
                            that.addPublisher(clientId, streamId, options, callback, retries);
                            return;
                        }
                        log.warn('message: addPublisher ErizoJS timeout no retry, ' +
                                 'retries: ' + retries +
                                 'streamId: ' + streamId + ', ' +
                                 'erizoId: ' + getErizoQueue(streamId) + ', ' +
                                 logger.objectToLog(options.metadata));
                        var erizo = erizos.findById(publishers[streamId]);
                        if (erizo !== undefined) {
                           var index = erizo.publishers.indexOf(streamId);
                           erizo.publishers.splice(index, 1);
                        }
                        callback('timeout-erizojs');
                        return;
                    } else {
                        if (data.type === 'initializing') {
                            data.agentId = agentId;
                            data.erizoId = erizoId;
                        }
                        callback(data);
                    }
                }});

                erizos.findById(erizoId).publishers.push(streamId);
            });

        } else {
            log.warn('message: addPublisher already set, streamId: ' + streamId +
                     ', ' + logger.objectToLog(options.metadata));
        }
    };

    /*
     * Adds a subscriber to the room. This creates a new WebRtcConnection.
     * This WebRtcConnection will be added to the subscribers list of the
     * OneToManyProcessor.
     */
    that.addSubscriber = function (clientId, streamId, options, callback, retries) {
        if (clientId === null){
          callback('Error: null clientId');
          return;
        }
        if (retries === undefined)
            retries = 0;

        if (publishers[streamId] !== undefined &&
            subscribers[streamId].indexOf(clientId) === -1) {
            log.info('message: addSubscriber, ' +
                     'streamId: ' + streamId + ', ' +
                     'clientId: ' + clientId + ', ' +
                     logger.objectToLog(options) + ', ' +
                     logger.objectToLog(options.metadata));

            if (options.audio === undefined) options.audio = true;
            if (options.video === undefined) options.video = true;

            var args = [clientId, streamId, options];

            amqper.callRpc(getErizoQueue(streamId, undefined), 'addSubscriber', args,
                           {callback: function (data){
                if (!publishers[streamId] && !subscribers[streamId]){
                    log.warn('message: addSubscriber rpc callback has arrived after ' +
                             'publisher is removed, ' +
                             'streamId: ' + streamId + ', ' +
                             'clientId: ' + clientId + ', ' +
                             logger.objectToLog(options.metadata));
                    callback('timeout');
                    return;
                }
                if (data === 'timeout'){
                    if (retries < MAX_ERIZOJS_RETRIES){
                        retries++;
                        log.warn('message: addSubscriber ErizoJS timeout, ' +
                                 'clientId: ' + clientId + ', ' +
                                 'streamId: ' + streamId + ', ' +
                                 'erizoId: ' + getErizoQueue(streamId) + ', ' +
                                 'retries: ' + retries + ', ' +
                                 logger.objectToLog(options.metadata));
                        that.addSubscriber(clientId, streamId, options, callback, retries);
                        return;
                    }
                    log.warn('message: addSubscriber ErizoJS timeout no retry, ' +
                             'clientId: ' + clientId + ', ' +
                             'streamId: ' + streamId + ', ' +
                             'erizoId: ' + getErizoQueue(streamId) + ', ' +
                             logger.objectToLog(options.metadata));
                    callback('timeout');
                    return;
                }else if (data.type === 'initializing'){
                    subscribers[streamId].push(clientId);
                }
                data.erizoId = publishers[streamId];
                callback(data);
            }});
        }
    };

    /*
     * Removes a publisher from the room. This also deletes the associated OneToManyProcessor.
     */
    that.removePublisher = function (clientId, streamId) {

        if (subscribers[streamId] !== undefined && publishers[streamId]!== undefined) {
            log.info('message: removePublisher, ' +
                     'streamId: ' + streamId + ', ' +
                     'erizoId: ' + getErizoQueue(streamId));

            var args = [clientId, streamId];
            amqper.callRpc(getErizoQueue(streamId), 'removePublisher', args, undefined);
            const erizo = erizos.findById(publishers[streamId]);

            if (erizo) {
                var index = erizo.publishers.indexOf(streamId);
                erizo.publishers.splice(index, 1);
            } else {
                log.warn('message: removePublisher was already removed, ' +
                         'streamId: ' + streamId + ', ' +
                         'erizoId: ' + getErizoQueue(streamId));
            }

            delete subscribers[streamId];
            delete publishers[streamId];
            log.debug('message: removedPublisher, ' +
                      'streamId: ' + streamId + ', ' +
                      'publishersLeft: ' + Object.keys(publishers).length );
        }
    };

    /*
     * Removes a subscriber from the room.
     * This also removes it from the associated OneToManyProcessor.
     */
    that.removeSubscriber = function (subscriberId, streamId) {
        if(subscribers[streamId]!==undefined){
            var index = subscribers[streamId].indexOf(subscriberId);
            if (index !== -1) {
                log.info('message: removeSubscriber, ' +
                         'clientId: ' + subscriberId + ', ' +
                         'streamId: ' + streamId);

                var args = [subscriberId, streamId];
                amqper.callRpc(getErizoQueue(streamId), 'removeSubscriber', args, undefined);

                subscribers[streamId].splice(index, 1);
            }
        } else {
            log.warn('message: removeSubscriber not found, ' +
                     'clientId: ' + subscriberId + ', ' +
                     'streamId: ' + streamId);
        }
    };

    /*
     * Removes all the subscribers related with a client.
     */
    that.removeSubscriptions = function (subscriberId) {

        var streamId, index;

        log.info('message: removeSubscriptions, clientId: ' + subscriberId);


        for (streamId in subscribers) {
            if (subscribers.hasOwnProperty(streamId)) {
                index = subscribers[streamId].indexOf(subscriberId);
                if (index !== -1) {
                    log.debug('message: removeSubscriptions, ' +
                              'clientId: ' + subscriberId + ', ' +
                              'streamId: ' + streamId);

                    var args = [subscriberId, streamId];
            		amqper.callRpc(getErizoQueue(streamId), 'removeSubscriber', args, undefined);

            		// Remove tracks
                    subscribers[streamId].splice(index, 1);
                }
            }
        }
    };

    that.getStreamStats = function (streamId, callback) {
        if (publishers[streamId]) {
            var args = [streamId];
            var theId = getErizoQueue(streamId);
            log.debug('Get stats for publisher ', streamId, 'theId', theId);
            amqper.callRpc(getErizoQueue(streamId), 'getStreamStats', args, {
              callback: function(data) {
                callback(data);
            }});
            return;
        }
    };

    return that;
};
