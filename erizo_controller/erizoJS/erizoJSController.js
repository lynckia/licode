/*global require, exports, , setInterval, clearInterval*/

var addon = require('./../../erizoAPI/build/Release/addon');
var config = require('./../../licode_config');
var logger = require('./../common/logger').logger;
var rpc = require('./../common/rpc');

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
        INTERVAL_TIME_FIR = 100,
        INTERVAL_TIME_KILL = 30*60*1000, // Timeout to kill itself after a timeout since the publisher leaves room.
        waitForFIR,
        initWebRtcConnection,
        getSdp,
        getRoap;


    var CONN_INITIAL = 101, CONN_STARTED = 102, CONN_READY = 103, CONN_FINISHED = 104, CONN_CANDIDATE = 201, CONN_SDP = 202, CONN_FAILED = 500;


    /*
     * Given a WebRtcConnection waits for the state READY for ask it to send a FIR packet to its publisher. 
     */
    waitForFIR = function (wrtc, to) {

        if (publishers[to] !== undefined) {
            var intervarId = setInterval(function () {
              if (publishers[to]!==undefined){
                if (wrtc.getCurrentState() >= CONN_READY && publishers[to].muxer.getPublisherState() >=CONN_READY) {
                    logger.info("Sending FIR");
                    publishers[to].muxer.sendFIR();
                    clearInterval(intervarId);
                }
              }

            }, INTERVAL_TIME_FIR);
        }
    };

    /*
     * Given a WebRtcConnection waits for the state CANDIDATES_GATHERED for set remote SDP. 
     */
    initWebRtcConnection = function (wrtc, callback, id_pub, id_sub) {

        if (config.erizoController.sendStats) {
            wrtc.getStats(function (newStats){
                rpc.callRpc('stats_handler', 'stats', {pub: id_pub, subs: id_sub, stats: JSON.parse(newStats)});
            });
        }

        wrtc.init( function (newStatus, mess){
          logger.info("webrtc Addon status", newStatus, mess);
          
          // if (newStatus === 102 && !sdpDelivered) {
          //   localSdp = wrtc.getLocalSdp();
          //   answer = getRoap(localSdp, roap);
          //   callback('callback', answer);
          //   sdpDelivered = true;

          // }
          // if (newStatus === 103) {
          //   callback('onReady');
          // }

          if (newStatus == CONN_INITIAL) {
            callback('callback', {type: 'started'});

          } else if (newStatus == CONN_SDP) {
            logger.debug('Sending SDP', mess);
            callback('callback', {type: 'answer', sdp: mess});

          } else if (newStatus == CONN_CANDIDATE) {
            callback('callback', {type: 'candidate', candidate: mess});
          } else if (newStatus == CONN_STARTED) {
          }

        });
        logger.info("initializing");

        callback('callback', {type: 'initializing'});

        // var roap = sdp,
        //     remoteSdp = getSdp(roap);
        // wrtc.setRemoteSdp(remoteSdp);

        // var sdpDelivered = false;
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

            logger.info("Adding external input peer_id ", from);

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
            logger.info("Publisher already set for", from);
        }
    };

    that.addExternalOutput = function (to, url) {
        if (publishers[to] !== undefined) {
            logger.info("Adding ExternalOutput to " + to + " url " + url);
            var externalOutput = new addon.ExternalOutput(url);
            externalOutput.init();
            publishers[to].muxer.addExternalOutput(externalOutput, url);
            externalOutputs[url] = externalOutput;
        }

    };

    that.removeExternalOutput = function (to, url) {
      if (externalOutputs[url] !== undefined && publishers[to]!=undefined) {
        logger.info("Stopping ExternalOutput: url " + url);
        publishers[to].muxer.removeSubscriber(url);
        delete externalOutputs[url];
      }
    };

    that.processSignaling = function (streamId, peerId, msg) {
        logger.info("Process Signaling message: ", streamId, peerId, msg);
        if (publishers[streamId] !== undefined) {

            if (subscribers[streamId][peerId]) {
                if (msg.type === 'offer') {
                    subscribers[streamId][peerId].setRemoteSdp(msg.sdp);
                } else if (msg.type === 'candidate') {
                    subscribers[streamId][peerId].addRemoteCandidate(msg.candidate.sdpMid, msg.candidate.candidate);
                } 
            } else {
                if (msg.type === 'offer') {
                    publishers[streamId].wrtc.setRemoteSdp(msg.sdp);
                } else if (msg.type === 'candidate') {
                    publishers[streamId].wrtc.addRemoteCandidate(msg.candidate.sdpMid, msg.candidate.candidate);
                } 
            }
            
        }
    };

    /*
     * Adds a publisher to the room. This creates a new OneToManyProcessor
     * and a new WebRtcConnection. This WebRtcConnection will be the publisher
     * of the OneToManyProcessor.
     */
    that.addPublisher = function (from, callback) {

        if (publishers[from] === undefined) {

            logger.info("Adding publisher peer_id ", from);

            var muxer = new addon.OneToManyProcessor(),
                wrtc = new addon.WebRtcConnection(true, true, config.erizo.stunserver, config.erizo.stunport, config.erizo.minport, config.erizo.maxport);

            publishers[from] = {muxer: muxer, wrtc: wrtc};
            subscribers[from] = {};

            wrtc.setAudioReceiver(muxer);
            wrtc.setVideoReceiver(muxer);
            muxer.setPublisher(wrtc);

            initWebRtcConnection(wrtc, callback, from);

            //logger.info('Publishers: ', publishers);
            //logger.info('Subscribers: ', subscribers);

        } else {
            logger.info("Publisher already set for", from);
        }
    };

    /*
     * Adds a subscriber to the room. This creates a new WebRtcConnection.
     * This WebRtcConnection will be added to the subscribers list of the
     * OneToManyProcessor.
     */
    that.addSubscriber = function (from, to, audio, video, callback) {

        if (publishers[to] !== undefined && subscribers[to][from] === undefined) {

            logger.info("Adding subscriber from ", from, 'to ', to, 'audio', audio, 'video', video);

            var wrtc = new addon.WebRtcConnection(audio, video, config.erizo.stunserver, config.erizo.stunport, config.erizo.minport, config.erizo.maxport);

            subscribers[to][from] = wrtc;
            publishers[to].muxer.addSubscriber(wrtc, from);

            initWebRtcConnection(wrtc, callback, to, from);
            waitForFIR(wrtc, to);

            //logger.info('Publishers: ', publishers);
            //logger.info('Subscribers: ', subscribers);
        }
    };

    /*
     * Removes a publisher from the room. This also deletes the associated OneToManyProcessor.
     */
    that.removePublisher = function (from) {

        if (subscribers[from] !== undefined && publishers[from] !== undefined) {
            logger.info('Removing muxer', from);
            publishers[from].muxer.close();
            logger.info('Removing subscribers', from);
            delete subscribers[from];
            logger.info('Removing publisher', from);
            delete publishers[from];
            logger.info('Removed all. Killing process.');
            process.exit(0);
        }
    };

    /*
     * Removes a subscriber from the room. This also removes it from the associated OneToManyProcessor.
     */
    that.removeSubscriber = function (from, to) {

        if (subscribers[to][from]) {
            logger.info('Removing subscriber ', from, 'to muxer ', to);
            publishers[to].muxer.removeSubscriber(from);
            delete subscribers[to][from];
        }
    };

    /*
     * Removes all the subscribers related with a client.
     */
    that.removeSubscriptions = function (from) {

        var key;

        logger.info('Removing subscriptions of ', from);
        for (key in subscribers) {
            if (subscribers.hasOwnProperty(key)) {
                if (subscribers[key][from]) {
                    logger.info('Removing subscriber ', from, 'to muxer ', key);
                    publishers[key].muxer.removeSubscriber(from);
                    delete subscribers[key][from];
                }
            }
        }
    };

    return that;
};
