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
    	ecch.getErizoJS(function(erizo_id) {
            log.info("Using Erizo", erizo_id);
            if (!erizos[erizo_id] && erizo_id !== 'timeout') {
                erizos[erizo_id] = {publishers: [], ka_count: 0};
            }
            callback(erizo_id);
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

/*  Prototype method to add a branch to a erizo Tree
    var shouldAddSurrogate = function(publisher_id, subscriber_id, options, callback){
        //Create new erizoJS, assign it as surrogate of publisher_id (subscribe), return id
        log.info("Adding a surrogate");
        getErizoJS(function(id){
            log.info("got a new Erizo with id", id);
            
//            log.info("Adding subscriber ", id, ' to publisher ', publisher_id);
            var options = {};
            if (options.audio === undefined) options.audio = true;
            if (options.video === undefined) options.video = true;

            
            var argsAddPub = [publisher_id, 300, true];
            log.info("Adding publisher to surrogate", "ErizoJS_"+id, "args",argsAddPub);
            var statusBranch = false;
            var statusRoot = false;
            var branchQueue = [];
            var rootQueue = [];
            log.info("calling addPublisher on branch");
            amqper.callRpc("ErizoJS_"+id, "addPublisher", argsAddPub, {callback: function(msg){
                log.info("Message from branch", msg.type);
                if (msg.type==='started'){
                    statusbranch = 'started';
                    log.info("branch Started");
                    if (branchQueue.length){
                        while (branchQueue.length>0){
                            var themessage = branchQueue.shift();
                            var argus = [publisher_id, undefined, themessage];
                            console.log("Queued msg from Erizoroot, sending to Erizobranch", argus);
                            amqper.callRpc("ErizoJS_"+id, "processSignaling", argus, {});
                        }

                    }

                }else if (msg.type === 'offer' || msg.type ==='candidate'){
                    var argsSignaling = [publisher_id, id, msg];
                    console.log("msg from Erizobranch, should to Erizoroot?");
                    if (statusroot){
                        console.log("Yes, sending", argsSignaling);
                        amqper.callRpc(getErizoQueue(publisher_id, id), "processSignaling", argsSignaling, {});
                    }else{
                        console.log("No, queuing", argsSignaling);
                        rootQueue.push(msg);
                    }
                }else if (msg.type === 'ready'){
                    log.info("Surrogate ready, firing after a second");
                    setTimeout (callback(id), 1000);
                }
                
            }});

            log.info("calling addSubscriber on root");
            var argsAddSub = [id, publisher_id, options];
            amqper.callRpc(getErizoQueue(publisher_id), "addSubscriber", argsAddSub, {callback: function(mess){
                log.info("Message from root", mess.type);
                if (mess.type==='started'){
                    statusroot = 'started';
                    log.info("root Started");
                    if (rootQueue.length){
                        while (rootQueue.length>0){
                            var themessage = rootQueue.shift();
                            var argsSignaling = [publisher_id, id, themessage];
                            console.log("Queued msg from Erizobranch, sending to Erizoroot", argsSignaling);
                            amqper.callRpc(getErizoQueue(publisher_id, id), "processSignaling", argsSignaling, {});
                        }

                    }

                }else if (mess.type === 'answer' || mess.type ==='candidate'){
                    if (mess.type === 'answer'){
                        mess.type = 'offer';
                    }
                    var argus = [publisher_id, subscriber_id, mess];

                    console.log("signalling from Erizoroot, should send to branch?");
                    if (statusbranch){
                        console.log("Yes, sending", argus);
                        amqper.callRpc("ErizoJS_"+id, "processSignaling", argus, {});
                    }else{
                        console.log("No, queuing", argus);
                        branchQueue.push(mess);
                    }
                }
            }});
            subscribers[publisher_id].push({id:subscriber_id, surrogate_id:id});
            // Track subscriber locally

        });

    }
*/
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
                publishers[publisher_id] = {};
                publishers[publisher_id].main = erizo_id;
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

//            log.info("Sending signaling mess to erizoJS of st ", streamId, ' of peer ', peerId);

            var args = [streamId, peerId, msg];

            amqper.callRpc(getErizoQueue(streamId), "processSignaling", args, {});

        }
    };

    /*
     * Adds a publisher to the room. This creates a new OneToManyProcessor
     * and a new WebRtcConnection. This WebRtcConnection will be the publisher
     * of the OneToManyProcessor.
     */
    that.addPublisher = function (publisher_id, options, callback) {

        if (publishers[publisher_id] === undefined) {

            log.info("Adding publisher peer_id ", publisher_id, "minVideoBW", options.minVideoBW);

            // We create a new ErizoJS with the publisher_id.
            getErizoJS(function(erizo_id) {

                if (erizo_id === 'timeout') {
                    log.error('No Agents Available');
                    callback('timeout');
                    return;
                }
            	log.info("Got Erizo to publish in", erizo_id);
                // Track publisher locally
                publishers[publisher_id] = erizo_id;
                subscribers[publisher_id] = [];
                
                // then we call its addPublisher method.
                var args = [publisher_id, options.minVideoBW, options.createOffer];
                //TODO: Possible race condition if we got an old id
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

//            if (true){

                if (options.audio === undefined) options.audio = true;
                if (options.video === undefined) options.video = true;

                var args = [subscriber_id, publisher_id, options];

                subscribers[publisher_id].push({id:subscriber_id});
                amqper.callRpc(getErizoQueue(publisher_id, undefined), "addSubscriber", args, {callback: callback});
/*            }else{ // Prototype for erizo_trees
                shouldAddSurrogate (publisher_id, subscriber_id, options, function(new_id){
                    log.info("Surrogate ?", new_id);

                    if (options.audio === undefined) options.audio = true;
                    if (options.video === undefined) options.video = true;

                    var args = [subscriber_id, publisher_id, options];

                    subscribers[publisher_id].push({id:subscriber_id, surrogate:new_id});
                    amqper.callRpc(getErizoQueue(publisher_id, subscriber_id), "addSubscriber", args, {callback: callback});
                    // Track subscriber locally
                });
                

            }
            */
        }
    };

    /*
     * Removes a publisher from the room. This also deletes the associated OneToManyProcessor.
     */
    that.removePublisher = function (publisher_id) {

        if (subscribers[publisher_id] !== undefined && publishers[publisher_id]!== undefined) {

            var args = [publisher_id];
            amqper.callRpc(getErizoQueue(publisher_id), "removePublisher", args, undefined);

            // Remove tracks
            // TODO: Remove all surrogates
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
