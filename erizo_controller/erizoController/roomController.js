/*global require, exports, setInterval*/
'use strict';
var logger = require('./common/logger').logger;

// Logger
var log = logger.getLogger('RoomController');

exports.RoomController = function (spec) {
    var that = {},
        // {id: array of subscribers}
        subscribers = {},
        // {id: erizoJS_id}
        publishers = {},
        // {erizoJS_id: {publishers: [ids], kaCount: count}}
        erizos = {},

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
            if (!erizos[erizoId]) return;

            if (ok !== true) {
                erizos[erizoId].kaCount ++;

                if (erizos[erizoId].kaCount > TIMEOUT_LIMIT) {
                    if (erizos[erizoId].publishers.length > 0){
                        log.error('message: ErizoJS timed out will be removed, ' +
                                  'erizoId: ' + erizoId + ', ' +
                                  'publishersAffected: ' + erizos[erizoId].publishers.length);
                        for (var p in erizos[erizoId].publishers) {
                            dispatchEvent('unpublish', erizos[erizoId].publishers[p]);
                        }

                    } else {
                        log.debug('message: empty erizoJS removed, erizoId: ' + erizoId);
                    }
                    ecch.deleteErizoJS(erizoId);
                    delete erizos[erizoId];
                }
            } else {
                erizos[erizoId].kaCount = 0;
            }
        };
    };

    var sendKeepAlive = function() {
        for (var e in erizos) {
            amqper.callRpc('ErizoJS_' + e, 'keepAlive', [], {callback: callbackFor(e)});
        }
    };

    setInterval(sendKeepAlive, KEEPALIVE_INTERVAL);

    var getErizoJS = function(callback) {
    	ecch.getErizoJS(function(erizoId, agentId) {
            if (!erizos[erizoId] && erizoId !== 'timeout') {
                erizos[erizoId] = {publishers: [], kaCount: 0};
            }
            callback(erizoId, agentId);
        });
    };

    var getErizoQueue = function(publisherId) {
        return 'ErizoJS_' + publishers[publisherId];
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
                               {callback: callback});

                erizos[erizoId].publishers.push(publisherId);

            });
        } else {
            log.info('message: addExternalInput publisher already set, ' +
                     'streamId: ' + publisherId + ', url: ' + url);
        }
    };

    that.addExternalOutput = function (publisherId, url, callback) {
        if (publishers[publisherId] !== undefined) {
            log.info('message: addExternalOuput, streamId: ' + publisherId + ', url:' + url);

            var args = [publisherId, url];

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

    that.processSignaling = function (streamId, peerId, msg) {

        if (publishers[streamId] !== undefined) {
            var args = [streamId, peerId, msg];
            amqper.callRpc(getErizoQueue(streamId), 'processSignaling', args, {});

        }
    };

    /*
     * Adds a publisher to the room. This creates a new OneToManyProcessor
     * and a new WebRtcConnection. This WebRtcConnection will be the publisher
     * of the OneToManyProcessor.
     */
    that.addPublisher = function (publisherId, options, callback, retries) {
        if (retries === undefined) {
            retries = 0;
        }

        if (publishers[publisherId] === undefined) {

            log.info('message: addPublisher, ' +
                     'streamId: ' + publisherId + ', ' +
                     logger.objectToLog(options) + ', ' +
                     logger.objectToLog(options.metadata));

            // We create a new ErizoJS with the publisherId.
            getErizoJS(function(erizoId, agentId) {

                if (erizoId === 'timeout') {
                    log.error('message: addPublisher ErizoAgent timeout, streamId: ' +
                              publisherId + ', ' + logger.objectToLog(options.metadata));
                    callback('timeout-agent');
                    return;
                }
                log.info('message: addPublisher erizoJs assigned, ' +
                        'erizoId: ' + erizoId + ', streamId: ', publisherId +
                         ', ' + logger.objectToLog(options.metadata));
                // Track publisher locally
                // then we call its addPublisher method.
                var args = [publisherId, options];
                publishers[publisherId] = erizoId;
                subscribers[publisherId] = [];

                amqper.callRpc(getErizoQueue(publisherId), 'addPublisher', args,
                              {callback: function (data){
                    if (data === 'timeout'){
                        if (retries < MAX_ERIZOJS_RETRIES){
                            log.warn('message: addPublisher ErizoJS timeout, ' +
                                     'streamId: ' + publisherId + ', ' +
                                     'erizoId: ' + getErizoQueue(publisherId) + ', ' +
                                     'retries: ' + retries + ', ' +
                                     logger.objectToLog(options.metadata));
                            publishers[publisherId] = undefined;
                            retries++;
                            that.addPublisher(publisherId, options, callback, retries);
                            return;
                        }
                        log.warn('message: addPublisher ErizoJS timeout no retry, ' +
                                 'retries: ' + retries +
                                 'streamId: ' + publisherId + ', ' +
                                 'erizoId: ' + getErizoQueue(publisherId) + ', ' +
                                 logger.objectToLog(options.metadata));
                        var erizo = erizos[publishers[publisherId]];
                        if (erizo !== undefined) {
                           var index = erizo.publishers.indexOf(publisherId);
                           erizo.publishers.splice(index, 1);
                        }
                        callback('timeout-erizojs');
                        return;
                    } else {
                        if (data.type === 'initializing') {
                            data.agentId = agentId;
                        }
                        callback(data);
                    }
                }});

                erizos[erizoId].publishers.push(publisherId);
            });

        } else {
            log.warn('message: addPublisher already set, streamId: ' + publisherId +
                     ', ' + logger.objectToLog(options.metadata));
        }
    };

    /*
     * Adds a subscriber to the room. This creates a new WebRtcConnection.
     * This WebRtcConnection will be added to the subscribers list of the
     * OneToManyProcessor.
     */
    that.addSubscriber = function (subscriberId, publisherId, options, callback, retries) {
        if (subscriberId === null){
          callback('Error: null subscriberId');
          return;
        }
        if (retries === undefined)
            retries = 0;

        if (publishers[publisherId] !== undefined &&
            subscribers[publisherId].indexOf(subscriberId) === -1) {
            log.info('message: addSubscriber, ' +
                     'streamId: ' + publisherId + ', ' +
                     'clientId: ' + subscriberId + ', ' +
                     logger.objectToLog(options) + ', ' +
                     logger.objectToLog(options.metadata));

            if (options.audio === undefined) options.audio = true;
            if (options.video === undefined) options.video = true;

            var args = [subscriberId, publisherId, options];

            amqper.callRpc(getErizoQueue(publisherId, undefined), 'addSubscriber', args,
                           {callback: function (data){
                if (!publishers[publisherId] && !subscribers[publisherId]){
                    log.warn('message: addSubscriber rpc callback has arrived after ' +
                             'publisher is removed, ' +
                             'streamId: ' + publisherId + ', ' +
                             'clientId: ' + subscriberId + ', ' +
                             logger.objectToLog(options.metadata));
                    callback('timeout');
                    return;
                }
                if (data === 'timeout'){
                    if (retries < MAX_ERIZOJS_RETRIES){
                        retries++;
                        log.warn('message: addSubscriber ErizoJS timeout, ' +
                                 'clientId: ' + subscriberId + ', ' +
                                 'streamId: ' + publisherId + ', ' +
                                 'erizoId: ' + getErizoQueue(publisherId) + ', ' +
                                 'retries: ' + retries + ', ' +
                                 logger.objectToLog(options.metadata));
                        that.addSubscriber(subscriberId, publisherId, options, callback, retries);
                        return;
                    }
                    log.warn('message: addSubscriber ErizoJS timeout no retry, ' +
                             'clientId: ' + subscriberId + ', ' +
                             'streamId: ' + publisherId + ', ' +
                             'erizoId: ' + getErizoQueue(publisherId) + ', ' +
                             logger.objectToLog(options.metadata));
                    callback('timeout');
                    return;
                }else if (data.type === 'initializing'){
                    subscribers[publisherId].push(subscriberId);
                }
                callback(data);
            }});
        }
    };

    /*
     * Removes a publisher from the room. This also deletes the associated OneToManyProcessor.
     */
    that.removePublisher = function (publisherId) {

        if (subscribers[publisherId] !== undefined && publishers[publisherId]!== undefined) {
            log.info('message: removePublisher, ' +
                     'publisherId: ' + publisherId + ', ' +
                     'erizoId: ' + getErizoQueue(publisherId));

            var args = [publisherId];
            amqper.callRpc(getErizoQueue(publisherId), 'removePublisher', args, undefined);

            if (erizos[publishers[publisherId]] !== undefined) {
                var index = erizos[publishers[publisherId]].publishers.indexOf(publisherId);
                erizos[publishers[publisherId]].publishers.splice(index, 1);
            } else {
                log.warn('message: removePublisher was already removed, ' +
                         'publisherId: ' + publisherId + ', ' +
                         'erizoId: ' + getErizoQueue(publisherId));
            }

            delete subscribers[publisherId];
            delete publishers[publisherId];
            log.debug('message: removedPublisher, ' +
                      'publisherId: ' + publisherId + ', ' +
                      'publishersLeft: ' + Object.keys(publishers).length );
        }
    };

    /*
     * Removes a subscriber from the room.
     * This also removes it from the associated OneToManyProcessor.
     */
    that.removeSubscriber = function (subscriberId, publisherId) {
        if(subscribers[publisherId]!==undefined){
            var index = subscribers[publisherId].indexOf(subscriberId);
            if (index !== -1) {
                log.info('message: removeSubscriber, ' +
                         'clientId: ' + subscriberId + ', ' +
                         'streamId: ' + publisherId);

                var args = [subscriberId, publisherId];
                amqper.callRpc(getErizoQueue(publisherId), 'removeSubscriber', args, undefined);

                subscribers[publisherId].splice(index, 1);
            }
        } else {
            log.warn('message: removeSubscriber not found, ' +
                     'clientId: ' + subscriberId + ', ' +
                     'streamId: ' + publisherId);
        }
    };

    /*
     * Removes all the subscribers related with a client.
     */
    that.removeSubscriptions = function (subscriberId) {

        var publisherId, index;

        log.info('message: removeSubscriptions, clientId: ' + subscriberId);


        for (publisherId in subscribers) {
            if (subscribers.hasOwnProperty(publisherId)) {
                index = subscribers[publisherId].indexOf(subscriberId);
                if (index !== -1) {
                    log.debug('message: removeSubscriptions, ' +
                              'clientId: ' + subscriberId + ', ' +
                              'streamId: ' + publisherId);

                    var args = [subscriberId, publisherId];
            		amqper.callRpc(getErizoQueue(publisherId), 'removeSubscriber', args, undefined);

            		// Remove tracks
                    subscribers[publisherId].splice(index, 1);
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
