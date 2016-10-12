/*global require, exports, console, setInterval, clearInterval*/

var logger = require('./../common/logger').logger;

// Logger
var log = logger.getLogger("RoomController");

exports.RoomController = function (spec) {
    "use strict";

    var that = {},
        // {id: array of subscribers}
        subscribers = {},
        // {id: erizoJS_id}
        publishers = {},
        // {erizoJS_id: {publishers: [ids], ka_count: count}}
        erizos = {},

        // {id: ExternalOutput}
        externalOutputs = {};

    var amqper = spec.amqper;
    var ecch = spec.ecch;

    var KEEPALIVE_INTERVAL = 5*1000;
    var TIMEOUT_LIMIT = 2;
    var MAX_ERIZOJS_RETRIES = 0;

    var eventListeners = [];

    var callbackFor = function(erizo_id) {

        return function(ok) {
            if (!erizos[erizo_id]) return;

            if (ok !== true) {
                erizos[erizo_id].ka_count ++;

                if (erizos[erizo_id].ka_count > TIMEOUT_LIMIT) {
                    if (erizos[erizo_id].publishers.length > 0){
                        log.error("message: ErizoJS timed out will be removed, erizoId: " + erizo_id + ", publishersAffected:", erizos[erizo_id].publishers);
                        for (var p in erizos[erizo_id].publishers) {
                            dispatchEvent("unpublish", erizos[erizo_id].publishers[p]);
                        }

                    } else {
                        log.debug("message: empty erizoJS removed, erizoId: " + erizo_id);
                    }
                    ecch.deleteErizoJS(erizo_id);
                    delete erizos[erizo_id];
                }
            } else {
                erizos[erizo_id].ka_count = 0;
            }
        }
    };

    var sendKeepAlive = function() {
        for (var e in erizos) {
            amqper.callRpc("ErizoJS_" + e, "keepAlive", [], {callback: callbackFor(e)});
        }
    };

    var keepAliveLoop = setInterval(sendKeepAlive, KEEPALIVE_INTERVAL);

    var getErizoJS = function(callback) {
    	ecch.getErizoJS(function(erizo_id, agent_id) {
            if (!erizos[erizo_id] && erizo_id !== 'timeout') {
                erizos[erizo_id] = {publishers: [], ka_count: 0};
            }
            callback(erizo_id, agent_id);
        });
    };

    var getErizoQueue = function(publisher_id) {
        return "ErizoJS_" + publishers[publisher_id];
    };

    var dispatchEvent = function(type, event) {
        for (var event_id in eventListeners) {
            eventListeners[event_id](type, event);    
        }
        
    };

    that.addEventListener = function(eventListener) {
        eventListeners.push(eventListener);
    };

    that.addExternalInput = function (publisher_id, url, callback) {

        if (publishers[publisher_id] === undefined) {

            log.info("message: addExternalInput,  streamId: " + publisher_id + ", url:" + url);

            getErizoJS(function(erizo_id) {
                // then we call its addPublisher method.
    	        var args = [publisher_id, url];

                // Track publisher locally
                publishers[publisher_id] = erizo_id;
                subscribers[publisher_id] = [];
    	        
                amqper.callRpc(getErizoQueue(publisher_id), "addExternalInput", args, {callback: callback});

                erizos[erizo_id].publishers.push(publisher_id);

            });
        } else {
            log.info("message: addExternalOutput publisher already set, streamId: " + publisher_id + ", url: " + url);
        }
    };

    that.addExternalOutput = function (publisher_id, url, callback) {
        if (publishers[publisher_id] !== undefined) {
            log.info('message: addExternalOuput, streamId: ' + publisher_id + ', url:' + url);

            var args = [publisher_id, url];

            amqper.callRpc(getErizoQueue(publisher_id), "addExternalOutput", args, undefined);

            // Track external outputs
            externalOutputs[url] = publisher_id;

            callback('success');
        } else {
            callback('error');
        }

    };

    that.removeExternalOutput = function (url, callback) {
        var publisher_id = externalOutputs[url];

        if (publisher_id !== undefined && publishers[publisher_id] != undefined) {
            log.info("removeExternalOutput, url: " + url);

            var args = [publisher_id, url];
            amqper.callRpc(getErizoQueue(publisher_id), "removeExternalOutput", args, undefined);

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
            amqper.callRpc(getErizoQueue(streamId), "processSignaling", args, {});

        }
    };


    /*
     * Adds a publisher to the room. This creates a new OneToManyProcessor
     * and a new WebRtcConnection. This WebRtcConnection will be the publisher
     * of the OneToManyProcessor.
     */
    that.addPublisher = function (publisher_id, options, callback, retries) {
        if (retries === undefined)
            retries = 0;

        if (publishers[publisher_id] === undefined) {

            log.info("message: addPublisher, streamId: " + publisher_id + ", options: " + JSON.stringify(options));;

            // We create a new ErizoJS with the publisher_id.
            getErizoJS(function(erizo_id, agent_id) {

                if (erizo_id === 'timeout') {
                    log.error('message: addPublisher ErizoAgent timeout, streamId: ' + publisher_id); 
                    callback('timeout-agent');
                    return;
                }
            	log.debug("message: addPublisher erizoJs assigned, erizoId: " + erizo_id + ", streamId: ", publisher_id);
                // Track publisher locally
                // then we call its addPublisher method.
                var args = [publisher_id, options];
                publishers[publisher_id] = erizo_id;
                subscribers[publisher_id] = [];
                
                amqper.callRpc(getErizoQueue(publisher_id), "addPublisher", args, {callback: function (data){
                    if (data === 'timeout'){
                        if (retries < MAX_ERIZOJS_RETRIES){
                            log.warn("message: addPublisher ErizoJS timeout, streamId: " + publisher_id + ", erizoId: " + getErizoQueue(publisher_id) + ", retries: " + retries);
                            publishers[publisher_id] = undefined;
                            retries++;
                            that.addPublisher(publisher_id, options,callback, retries);
                            return;
                        }
                        log.warn("message: addPublisher ErizoJS timeout no retry, streamId: " + publisher_id + ", erizoId: " + getErizoQueue(publisher_id));
                        var index = erizos[publishers[publisher_id]].publishers.indexOf(publisher_id);
                        erizos[publishers[publisher_id]].publishers.splice(index, 1);
                        callback('timeout-erizojs');
                        return;
                    }else{
                        if (data.type === 'initializing') {
                            data.agent_id = agent_id;
                        }
                        callback(data);
                    }
                }});

                erizos[erizo_id].publishers.push(publisher_id);
            });

        } else {
            log.warn("message: addPublisher already set, streamId: " + publisher_id );
        }
    };

    /*
     * Adds a subscriber to the room. This creates a new WebRtcConnection.
     * This WebRtcConnection will be added to the subscribers list of the
     * OneToManyProcessor.
     */
    that.addSubscriber = function (subscriber_id, publisher_id, options, callback, retries) {
        if (subscriber_id === null){
          callback("Error: null subscriber_id");
          return;
        }
        if (retries === undefined)
            retries = 0;

        if (publishers[publisher_id] !== undefined && subscribers[publisher_id].indexOf(subscriber_id) === -1) {
            log.info("message: addSubscriber streamId: " + publisher_id + ", clientId: " + subscriber_id + ", options: " + JSON.stringify(options));;

            if (options.audio === undefined) options.audio = true;
            if (options.video === undefined) options.video = true;

            var args = [subscriber_id, publisher_id, options];
            
            amqper.callRpc(getErizoQueue(publisher_id, undefined), "addSubscriber", args, {callback: function (data){
                if (!publishers[publisher_id] && !subscribers[publisher_id]){
                    log.warn("message: addSubscriber rpc callback has arrived after publisher is removed, streamId: " + publisher_id + ", clientId: " + subscriber_id);
                    callback('timeout');
                    return;
                }
                if (data === 'timeout'){
                    if (retries < MAX_ERIZOJS_RETRIES){
                        retries++;
                        log.warn("message: addSubscriber ErizoJS timeout, clientId: " + subscriber_id + ",streamId: " + publisher_id + ", erizoId: " + getErizoQueue(publisher_id) + ", retries: " + retries);
                        that.addSubscriber(subscriber_id, publisher_id, options, callback, retries);
                        return;
                    }
                    log.warn("message: addSubscriber ErizoJS timeout no retry, clientId: " + subscriber_id + ",streamId: " + publisher_id + ", erizoId: " + getErizoQueue(publisher_id));
                    callback('timeout');
                    return;
                }else if (data.type === 'initializing'){
                    subscribers[publisher_id].push(subscriber_id);
                }
                callback(data);
            }});
        }
    };

    /*
     * Removes a publisher from the room. This also deletes the associated OneToManyProcessor.
     */
    that.removePublisher = function (publisher_id) {

        if (subscribers[publisher_id] !== undefined && publishers[publisher_id]!== undefined) {
            log.info("message: removePublisher, publisherId: " + publisher_id + ", erizoId: " + getErizoQueue(publisher_id));

            var args = [publisher_id];
            amqper.callRpc(getErizoQueue(publisher_id), "removePublisher", args, undefined);

            if (erizos[publishers[publisher_id]]!== undefined){
                var index = erizos[publishers[publisher_id]].publishers.indexOf(publisher_id);
                erizos[publishers[publisher_id]].publishers.splice(index, 1);
            }else{
                log.warn("message: removePublisher was already removed, publisherId: " + publisher_id + ", erizoId: " + getErizoQueue(publisher_id));
            }
            
            delete subscribers[publisher_id];
            delete publishers[publisher_id];
            log.debug("message: removedPublisher, publisherId: " + publisher_id + ", publishersLeft: " + Object.keys(publishers).length );
        }
    };

    /*
     * Removes a subscriber from the room. This also removes it from the associated OneToManyProcessor.
     */
    that.removeSubscriber = function (subscriber_id, publisher_id) {
        if(subscribers[publisher_id]!==undefined){
            var index = subscribers[publisher_id].indexOf(subscriber_id);
            if (index !== -1) {
                log.info('message: removeSubscriber, clientId: ' + subscriber_id + ', streamId: ' + publisher_id);

                var args = [subscriber_id, publisher_id];
                amqper.callRpc(getErizoQueue(publisher_id), "removeSubscriber", args, undefined);

                subscribers[publisher_id].splice(index, 1);
            }
        } else {
            log.warn("message: removeSubscriber not found, clientId: " + subscriber_id + ", streamId: " + publisher_id);
        }
    };

    /*
     * Removes all the subscribers related with a client.
     */
    that.removeSubscriptions = function (subscriber_id) {

        var publisher_id, index;

        log.info('message: removeSubscriptions, clientId: ' + subscriber_id);


        for (publisher_id in subscribers) {
            if (subscribers.hasOwnProperty(publisher_id)) {
                index = subscribers[publisher_id].indexOf(subscriber_id);
                if (index !== -1) {
                    log.debug('message: removeSubscriptions, clientId: ' + subscriber_id + ', streamId: ' + publisher_id);

                    var args = [subscriber_id, publisher_id];
            		amqper.callRpc(getErizoQueue(publisher_id), "removeSubscriber", args, undefined);

            		// Remove tracks
                    subscribers[publisher_id].splice(index, 1);
                }
            }
        }
    };

    return that;
};
