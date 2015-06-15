/*global require, exports, , setInterval, clearInterval*/

var addon = require('./../../erizoAPI/build/Release/addon');
var logger = require('./../common/logger').logger;
var amqper = require('./../common/amqper');

// Logger
var log = logger.getLogger("ErizoJSController");

exports.ErizoJSController = function (spec) {
    "use strict";

    var that = {},
        // {id: {subsId1: wrtc1, subsId2: wrtc2}}
        subscribers = {},
        // {id: {muxer: OneToManyProcessor, wrtc: WebRtcConnection}
        publishers = {},

        // {id: ExternalOutput}
        externalOutputs = {},

        INTERVAL_TIME_SDP = 100,
        INTERVAL_TIME_KILL = 30*60*1000, // Timeout to kill itself after a timeout since the publisher leaves room.
        INTERVAL_STATS = 1000,
        MIN_RECOVER_BW = 50000,
        initWebRtcConnection,
        getSdp,
        calculateAverage,
        getRoap;


    var CONN_INITIAL = 101, CONN_STARTED = 102,CONN_GATHERED = 103, CONN_READY = 104, CONN_FINISHED = 105, CONN_CANDIDATE = 201, CONN_SDP = 202, CONN_FAILED = 500;

    /* BW Status
     * 0 - Stable 
     * 1 - Insufficient Bandwidth 
     * 2 - Trying recovery
     * 3 - Won't recover
     */

    var BW_STABLE = 0, BW_INSUFFICIENT = 1, BW_RECOVERING = 2, BW_WONTRECOVER = 3;

    calculateAverage = function (values) { 
      if (values.length === undefined)
        return 0;
      var cnt = values.length;
      var tot = parseInt(0);
      for (var i = 0; i < values.length; i++){
          tot+=parseInt(values[i]);
      }
      return Math.ceil(tot/cnt);
    };

    /*
     * Given a WebRtcConnection waits for the state CANDIDATES_GATHERED for set remote SDP.
     */
    initWebRtcConnection = function (wrtc, callback, id_pub, id_sub, browser) {

        wrtc.bwValues = [];
        var isReporting = true;
        var ticks = 0;
        var retries = 0;
        var ticksToTry = 0;
        var lastAverage, average, lastBWValue, toRecover;
        var nextRetry = 0;
        wrtc.bwStatus = BW_STABLE;
        

        if (wrtc.minVideoBW || GLOBAL.config.erizoController.report.rtcp_stats){
            if (wrtc.minVideoBW){
                wrtc.minVideoBW = wrtc.minVideoBW*1000; // We need it in bps
                wrtc.lowerThres = Math.floor(wrtc.minVideoBW*(1-0.2));
                wrtc.upperThres = Math.ceil(wrtc.minVideoBW);
            }
            var intervalId = setInterval(function () {
                var newStats = wrtc.getStats();
                if (newStats == null){
                    log.debug("Stopping stats");
                    clearInterval(intervalId);
                    return;
                }

                var theStats = JSON.parse(newStats);
                if (wrtc.minVideoBW){
                    
                    for (var i = 0; i < theStats.length; i++){ 
                        if(theStats[i].hasOwnProperty('bandwidth')){   // Only one stream should have bandwidth
                            lastBWValue = theStats[i].bandwidth;
                            wrtc.bwValues.push(lastBWValue);
                            if (wrtc.bwValues.length > 5){
                                wrtc.bwValues.shift();
                            }
                            average = calculateAverage(wrtc.bwValues);
                        }
                    }
                    toRecover = (average/4)<MIN_RECOVER_BW?(average/4):MIN_RECOVER_BW;
                    switch (wrtc.bwStatus){
                        case BW_STABLE:
                            if(average <= lastAverage && (average < wrtc.lowerThres)){
                                if (++ticks > 2){
                                    log.debug("STABLE STATE, Bandwidth is insufficient, moving to state BW_INSUFFICIENT", average, "lowerThres", wrtc.lowerThres);
                                    wrtc.bwStatus = BW_INSUFFICIENT;
                                    wrtc.setFeedbackReports(false, toRecover);
                                    ticks = 0;
                                    callback('callback', {type:'bandwidthAlert', message:'insufficient', bandwidth: average});
                                }
                            }                            
                            break;
                        case BW_INSUFFICIENT:
                            if(average > wrtc.upperThres){
                                log.debug("BW_INSUFFICIENT State: we have recovered", average, "lowerThres", wrtc.lowerThres);
                                ticks = 0;
                                nextRetry = 0;
                                retries = 0;
                                wrtc.bwStatus = BW_STABLE;
                                wrtc.setFeedbackReports(true, 0);
                                callback('callback', {type:'bandwidthAlert', message:'recovered', bandwidth: average});
                            }
                            else if (retries>=3){
                                log.debug("BW_INSUFFICIENT State: moving to won't recover", average, "lowerThres", wrtc.lowerThres);
                                wrtc.bwStatus = BW_WONTRECOVER; 
                            }
                            else if (nextRetry === 0){  //schedule next retry
                                nextRetry = ticks + 20;
                            }
                            else if (++ticks == nextRetry){  // next retry is in order
                                wrtc.bwStatus = BW_RECOVERING;
                                ticksToTry = ticks + 10;
                                wrtc.setFeedbackReports (false, average);                                
                            }
                            break;
                        case BW_RECOVERING:
                            log.debug("In recovering state lastValue", lastBWValue, "lastAverage", lastAverage, "lowerThres", wrtc.lowerThres);
                            if(average > wrtc.upperThres){ 
                                log.debug("BW_RECOVERING State: we have recovered", average, "lowerThres", wrtc.lowerThres);
                                ticks = 0;
                                nextRetry = 0;
                                retries = 0;
                                wrtc.bwStatus = BW_STABLE;
                                wrtc.setFeedbackReports(true, 0);
                                callback('callback', {type:'bandwidthAlert', message:'recovered', bandwidth: average});
                            }
                            else if (average> lastAverage){ //we are recovering
                                log.debug("BW_RECOVERING State: we have improved, more trying time", average, "lowerThres", wrtc.lowerThres);
                                wrtc.setFeedbackReports(false, average*(1+0.3));
                                ticksToTry=ticks+10;

                            }
                            else if (++ticks >= ticksToTry){ //finish this retry
                                log.debug("BW_RECOVERING State: Finished this retry", retries, average, "lowerThres", wrtc.lowerThres);
                                ticksToTry = 0;
                                nextRetry = 0;
                                retries++;
                                wrtc.bwStatus = BW_INSUFFICIENT;
                                wrtc.setFeedbackReports (false, toRecover);
                            }
                            break;
                        case BW_WONTRECOVER:
                            log.debug("BW_WONTRECOVER", average, "lowerThres", wrtc.lowerThres);
                            ticks = 0;
                            nextRetry = 0;
                            retries = 0;
                            average = 0;
                            lastAverage = 0;
                            wrtc.bwStatus = BW_STABLE;
                            wrtc.minVideoBW = false;                      
                            wrtc.setFeedbackReports (false, 1);
                            callback('callback', {type:'bandwidthAlert', message:'wont-recover', bandwidth: average});
                            break;
                        default:
                            log.error("Unknown BW status");
                    }
                    lastAverage = average;
                }
                if (GLOBAL.config.erizoController.report.rtcp_stats) {
                    wrtc.getStats(function (newStats){
                        var timeStamp = new Date();
                        amqper.broadcast('stats', {pub: id_pub, subs: id_sub, stats: theStats, timestamp:timeStamp.getTime()});
                    });
                }
            }, INTERVAL_STATS);
        }

        wrtc.init( function (newStatus, mess){
            log.info("webrtc Addon status ", newStatus, mess, "id pub", id_pub, "id_sub", id_sub );

            if (GLOBAL.config.erizoController.report.connection_events) {
                var timeStamp = new Date();
                amqper.broadcast('event', {pub: id_pub, subs: id_sub, type: 'connection_status', status: newStatus, timestamp:timeStamp.getTime()});
            }

            switch(newStatus) {
                case CONN_INITIAL:
                    callback('callback', {type: 'started'});
                    break;

                case CONN_SDP:
                case CONN_GATHERED:
//                    log.debug('Sending SDP', mess);
                    mess = mess.replace(that.privateRegexp, that.publicIP);
                    callback('callback', {type: 'answer', sdp: mess});
                    break;
                    
                case CONN_CANDIDATE:
                    mess = mess.replace(that.privateRegexp, that.publicIP);
                    callback('callback', {type: 'candidate', candidate: mess});
                    break;

                case CONN_FAILED:
                    callback('callback', {type: 'failed', sdp: mess});
                    break;

                case CONN_READY:
                    // If I'm a subscriber and I'm bowser, I ask for a PLI
                    if (id_sub && browser === 'bowser') {
                        log.info('SENDING PLI from ', id_pub, ' to BOWSER ', id_sub);
                        publishers[id_pub].wrtc.generatePLIPacket();
                    }
                    callback('callback', {type: 'ready'});
                    break;
            }
        });
        log.info("initializing");

        callback('callback', {type: 'initializing'});
    };

    /*
     * Gets SDP from roap message.
     */
    getSdp = function (roap) {

        var reg1 = new RegExp(/^.*sdp\":\"(.*)\",.*$/),
            sdp = roap.match(reg1)[1],
            reg2 = new RegExp(/\\r\\n/g);

        sdp = sdp.replace(reg2, '\n');

        return sdp;

    };

    /*
     * Gets roap message from SDP.
     */
    getRoap = function (sdp, offerRoap) {

        var reg1 = new RegExp(/\n/g),
            offererSessionId = offerRoap.match(/("offererSessionId":)(.+?)(,)/)[0],
            answererSessionId = "106",
            answer = ('\n{\n \"messageType\":\"ANSWER\",\n');

        sdp = sdp.replace(reg1, '\\r\\n');

        //var reg2 = new RegExp(/^.*offererSessionId\":(...).*$/);
        //var offererSessionId = offerRoap.match(reg2)[1];

        answer += ' \"sdp\":\"' + sdp + '\",\n';
        //answer += ' \"offererSessionId\":' + offererSessionId + ',\n';
        answer += ' ' + offererSessionId + '\n';
        answer += ' \"answererSessionId\":' + answererSessionId + ',\n \"seq\" : 1\n}\n';

        return answer;
    };

    that.addExternalInput = function (from, url, callback) {

        if (publishers[from] === undefined) {

            log.info("Adding external input peer_id ", from);

            var muxer = new addon.OneToManyProcessor(),
                ei = new addon.ExternalInput(url);

            publishers[from] = {muxer: muxer};
            subscribers[from] = {};

            ei.setAudioReceiver(muxer);
            ei.setVideoReceiver(muxer);
            muxer.setExternalPublisher(ei);

            var answer = ei.init();

            if (answer >= 0) {
                callback('callback', 'success');
            } else {
                callback('callback', answer);
            }

        } else {
            log.info("Publisher already set for", from);
        }
    };

    that.addExternalOutput = function (to, url) {
        if (publishers[to] !== undefined) {
            log.info("Adding ExternalOutput to " + to + " url " + url);
            var externalOutput = new addon.ExternalOutput(url);
            externalOutput.init();
            publishers[to].muxer.addExternalOutput(externalOutput, url);
            externalOutputs[url] = externalOutput;
        }
    };

    that.removeExternalOutput = function (to, url) {
      if (externalOutputs[url] !== undefined && publishers[to] !== undefined) {
        log.info("Stopping ExternalOutput: url " + url);
        publishers[to].muxer.removeSubscriber(url);
        delete externalOutputs[url];
      }
    };

    that.processSignaling = function (streamId, peerId, msg) {
        log.info("Process Signaling message: ", streamId, peerId, msg);
        if (publishers[streamId] !== undefined) {

            if (subscribers[streamId][peerId]) {
                if (msg.type === 'offer') {
                    subscribers[streamId][peerId].setRemoteSdp(msg.sdp);
                } else if (msg.type === 'candidate') {
                    subscribers[streamId][peerId].addRemoteCandidate(msg.candidate.sdpMid, msg.candidate.sdpMLineIndex , msg.candidate.candidate);
                } else if (msg.type === 'updatestream'){
                    subscribers[streamId][peerId].setRemoteSdp(msg.sdp);
                }
            } else {
                if (msg.type === 'offer') {
                    publishers[streamId].wrtc.setRemoteSdp(msg.sdp);
                } else if (msg.type === 'candidate') {
                    publishers[streamId].wrtc.addRemoteCandidate(msg.candidate.sdpMid, msg.candidate.sdpMLineIndex, msg.candidate.candidate);
                } else if (msg.type === 'updatestream'){
                    if (msg.sdp){
                        publishers[streamId].wrtc.setRemoteSdp(msg.sdp);
                    }
                    if (msg.minVideoBW){
                        log.debug("Updating minVideoBW to ", msg.minVideoBW);
                        publishers[streamId].minVideoBW = msg.minVideoBW;
                        for (var sub in subscribers[streamId]){
                            log.debug("sub", sub);
                            log.debug("updating subscriber BW from", subscribers[streamId][sub].minVideoBW, "to", msg.minVideoBW*1000 );
                            var theConn = subscribers[streamId][sub];
                            theConn.minVideoBW = msg.minVideoBW*1000; // We need it in bps
                            theConn.lowerThres = Math.floor(theConn.minVideoBW*(1-0.2));
                            theConn.upperThres = Math.ceil(theConn.minVideoBW*(1+0.1));
                        }
                    }
                }
            }
            
        }
    };

    /*
     * Adds a publisher to the room. This creates a new OneToManyProcessor
     * and a new WebRtcConnection. This WebRtcConnection will be the publisher
     * of the OneToManyProcessor.
     */
    that.addPublisher = function (from, minVideoBW, callback) {

        if (publishers[from] === undefined) {

            log.info("Adding publisher peer_id ", from, "minVideoBW", minVideoBW);

            var muxer = new addon.OneToManyProcessor(),
                wrtc = new addon.WebRtcConnection(true, true, GLOBAL.config.erizo.stunserver, GLOBAL.config.erizo.stunport, GLOBAL.config.erizo.minport, GLOBAL.config.erizo.maxport,false);

            publishers[from] = {muxer: muxer, wrtc: wrtc, minVideoBW: minVideoBW};
            subscribers[from] = {};

            wrtc.setAudioReceiver(muxer);
            wrtc.setVideoReceiver(muxer);
            muxer.setPublisher(wrtc);

            initWebRtcConnection(wrtc, callback, from);

            //log.info('Publishers: ', publishers);
            //log.info('Subscribers: ', subscribers);

        } else {
            log.info("Publisher already set for", from);
        }
    };

    /*
     * Adds a subscriber to the room. This creates a new WebRtcConnection.
     * This WebRtcConnection will be added to the subscribers list of the
     * OneToManyProcessor.
     */
    that.addSubscriber = function (from, to, options, callback) {

        if (publishers[to] !== undefined && subscribers[to][from] === undefined) {

            log.info("Adding subscriber from ", from, 'to ', to, 'audio', options.audio, 'video', options.video);

            var wrtc = new addon.WebRtcConnection(options.audio, options.video, GLOBAL.config.erizo.stunserver, GLOBAL.config.erizo.stunport, GLOBAL.config.erizo.minport, GLOBAL.config.erizo.maxport,false);

            subscribers[to][from] = wrtc;
            publishers[to].muxer.addSubscriber(wrtc, from);
            wrtc.minVideoBW = publishers[to].minVideoBW;

            initWebRtcConnection(wrtc, callback, to, from, options.browser);

            //log.info('Publishers: ', publishers);
            //log.info('Subscribers: ', subscribers);
        }
    };

    /*
     * Removes a publisher from the room. This also deletes the associated OneToManyProcessor.
     */
    that.removePublisher = function (from) {

        if (subscribers[from] !== undefined && publishers[from] !== undefined) {
            log.info('Removing muxer', from);
            for (var key in subscribers[from]) {
              if (subscribers[from].hasOwnProperty(key)){
                log.info("Iterating and closing ", key,  subscribers[from], subscribers[from][key]);
                subscribers[from][key].close();
              }
            }
            publishers[from].wrtc.close();
            publishers[from].muxer.close();
            log.info('Removing subscribers', from);
                
            delete subscribers[from];
            log.info('Removing publisher', from);
            delete publishers[from];
            var count = 0;
            for (var k in publishers) {
                if (publishers.hasOwnProperty(k)) {
                   ++count;
                }
            }
            log.info("Publishers: ", count);
            if (count === 0)  {
                log.info('Removed all publishers. Killing process.');
                process.exit(0);
            }
        }
    };

    /*
     * Removes a subscriber from the room. This also removes it from the associated OneToManyProcessor.
     */
    that.removeSubscriber = function (from, to) {

        if (subscribers[to][from]) {
            log.info('Removing subscriber ', from, 'to muxer ', to);
            subscribers[to][from].close();
            publishers[to].muxer.removeSubscriber(from);
            delete subscribers[to][from];
        }
    };

    /*
     * Removes all the subscribers related with a client.
     */
    that.removeSubscriptions = function (from) {

        var key;

        log.info('Removing subscriptions of ', from);
        for (key in subscribers) {
            if (subscribers.hasOwnProperty(key)) {
                 if (subscribers[to][from]) {
                    log.info('Removing subscriber ', from, 'to muxer ', key);
                    publishers[key].muxer.removeSubscriber(from);
                    delete subscribers[key][from];
                }
            }
        }
    };

    return that;
};
