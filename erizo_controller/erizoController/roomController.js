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

    var KEEPALIVE_INTERVAL = 5*1000;
    var TIMEOUT_LIMIT = 2;

    var eventListeners = [];

    var callbackFor = function(erizo_id) {

        return function(ok) {
            if (!erizos[erizo_id]) return;

            if (ok !== true) {
                erizos[erizo_id].ka_count ++;

                if (erizos[erizo_id].ka_count > TIMEOUT_LIMIT) {

                    for (var p in erizos[erizo_id].publishers) {
                        dispatchEvent("unpublish", erizos[erizo_id].publishers[p]);
                    }
                    amqper.callRpc("ErizoAgent", "deleteErizoJS", [erizo_id], {callback: function(){}}); 
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
    	amqper.callRpc("ErizoAgent", "createErizoJS", [], {callback: function(erizo_id) {
            log.info("Using Erizo", erizo_id);
            if (!erizos[erizo_id] && erizo_id !== 'timeout') {
                erizos[erizo_id] = {publishers: [], ka_count: 0};
            }
            callback(erizo_id);
        }});
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

            log.info("Adding external input peer_id ", publisher_id);

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
            log.info("Publisher already set for", publisher_id);
        }
    };

    that.addExternalOutput = function (publisher_id, url, callback) {
        if (publishers[publisher_id] !== undefined) {
            log.info("Adding ExternalOutput to " + publisher_id + " url " + url);

            var args = [publisher_id, url];

            amqper.callRpc(getErizoQueue(publisher_id), "addExternalOutput", args, undefined);

            // Track external outputs
            externalOutputs[url] = publisher_id;

            // Track publisher locally
            // publishers[publisher_id] = publisher_id;
            // subscribers[publisher_id] = [];
            callback('success');
        } else {
            callback('error');
        }

    };

    that.removeExternalOutput = function (url, callback) {
        var publisher_id = externalOutputs[url];

        if (publisher_id !== undefined && publishers[publisher_id] != undefined) {
            log.info("Stopping ExternalOutput: url " + url);

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

            //log.info("Sending signaling mess to erizoJS of st ", streamId, ' of peer ', peerId);

            var args = [streamId, peerId, msg];

            amqper.callRpc(getErizoQueue(streamId), "processSignaling", args, {});

        }
    };

    /*
     * Adds a publisher to the room. This creates a new OneToManyProcessor
     * and a new WebRtcConnection. This WebRtcConnection will be the publisher
     * of the OneToManyProcessor.
     */
    that.addPublisher = function (publisher_id, callback) {

        if (publishers[publisher_id] === undefined) {

            log.info("Adding publisher peer_id ", publisher_id);

            // We create a new ErizoJS with the publisher_id.
            getErizoJS(function(erizo_id) {

                if (erizo_id === 'timeout') {
                    log.error('No Agents Available');
                    callback('timeout');
                    return;
                }
            	log.info("Erizo got");
                // Track publisher locally
                publishers[publisher_id] = erizo_id;
                subscribers[publisher_id] = [];
                
                // then we call its addPublisher method.
                var args = [publisher_id];
                amqper.callRpc(getErizoQueue(publisher_id), "addPublisher", args, {callback: callback});

                erizos[erizo_id].publishers.push(publisher_id);
            });

        } else {
            log.info("Publisher already set for", publisher_id);
        }
    };

    /*
     * Adds a subscriber to the room. This creates a new WebRtcConnection.
     * This WebRtcConnection will be added to the subscribers list of the
     * OneToManyProcessor.
     */
    that.addSubscriber = function (subscriber_id, publisher_id, options, callback) {
        if (subscriber_id === null){
          callback("Error: null subscriber_id");
          return;
        }

        if (publishers[publisher_id] !== undefined && subscribers[publisher_id].indexOf(subscriber_id) === -1) {

            log.info("Adding subscriber ", subscriber_id, ' to publisher ', publisher_id);

            if (options.audio === undefined) options.audio = true;
            if (options.video === undefined) options.video = true;

            var args = [subscriber_id, publisher_id, options];

            amqper.callRpc(getErizoQueue(publisher_id), "addSubscriber", args, {callback: callback});

            // Track subscriber locally
            subscribers[publisher_id].push(subscriber_id);
        }
    };

    /*
     * Removes a publisher from the room. This also deletes the associated OneToManyProcessor.
     */
    that.removePublisher = function (publisher_id) {

        if (subscribers[publisher_id] !== undefined && publishers[publisher_id] !== undefined) {

            var args = [publisher_id];
            amqper.callRpc(getErizoQueue(publisher_id), "removePublisher", args, undefined);

            // Remove tracks
            var index = erizos[publishers[publisher_id]].publishers.indexOf(publisher_id);
            erizos[publishers[publisher_id]].publishers.splice(index, 1);

            log.info('Removing subscribers', publisher_id);
            delete subscribers[publisher_id];
            log.info('Removing publisher', publisher_id);
            delete publishers[publisher_id];
            log.info('Removed all');
            log.info('Removing muxer', publisher_id, ' muxers left ', Object.keys(publishers).length );
        }
    };

    /*
     * Removes a subscriber from the room. This also removes it from the associated OneToManyProcessor.
     */
    that.removeSubscriber = function (subscriber_id, publisher_id) {

        var index = subscribers[publisher_id].indexOf(subscriber_id);
        if (index !== -1) {
            log.info('Removing subscriber ', subscriber_id, 'to muxer ', publisher_id);

            var args = [subscriber_id, publisher_id];
            amqper.callRpc(getErizoQueue(publisher_id), "removeSubscriber", args, undefined);

            // Remove track
            subscribers[publisher_id].splice(index, 1);
        }
    };

    /*
     * Removes all the subscribers related with a client.
     */
    that.removeSubscriptions = function (subscriber_id) {

        var publisher_id, index;

        log.info('Removing subscriptions of ', subscriber_id);


        for (publisher_id in subscribers) {
            if (subscribers.hasOwnProperty(publisher_id)) {
                index = subscribers[publisher_id].indexOf(subscriber_id);
                if (index !== -1) {
                    log.info('Removing subscriber ', subscriber_id, 'to muxer ', publisher_id);

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
