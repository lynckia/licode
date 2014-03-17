/*global require, exports, console, setInterval, clearInterval*/

var addon = require('./../../erizoAPI/build/Release/addon');
var config = require('./../../licode_config');
var logger = require('./logger').logger;

exports.WebRtcController = function () {
    "use strict";

    var that = {},
        // {id: array of subscribers}
        subscribers = {},
        // {id: OneToManyProcessor}
        publishers = {},

        // {id: ExternalOutput}
        externalOutputs = {},

        INTERVAL_TIME_SDP = 100,
        INTERVAL_TIME_FIR = 100,
        waitForFIR,
        initWebRtcConnection,
        getSdp,
        getRoap;


    /*
     * Given a WebRtcConnection waits for the state READY for ask it to send a FIR packet to its publisher. 
     */
    waitForFIR = function (wrtc, to) {

        if (publishers[to] !== undefined) {
            var intervarId = setInterval(function () {
              if (publishers[to]!==undefined){
                if (wrtc.getCurrentState() >= 103 && publishers[to].getPublisherState() >=103) {
                    publishers[to].sendFIR();
                    clearInterval(intervarId);
                }
              }

            }, INTERVAL_TIME_FIR);
        }
    };

    /*
     * Given a WebRtcConnection waits for the state CANDIDATES_GATHERED for set remote SDP. 
     */
    initWebRtcConnection = function (wrtc, sdp, callback, onReady, id) {

        wrtc.init( function (newStatus){
          var localSdp, answer;
          console.log("webrtc Addon status" + newStatus );
          if (newStatus === 102 && !sdpDelivered) {
            localSdp = wrtc.getLocalSdp();
            answer = getRoap(localSdp, roap);
            callback(answer);
            sdpDelivered = true;

          }
          if (newStatus === 103) {
            if (onReady != undefined) {
              onReady();
            }
          }
        });

        var roap = sdp,
            remoteSdp = getSdp(roap);
        wrtc.setRemoteSdp(remoteSdp);

        wrtc.getStats(function (newStats){
          console.log("newStats for id " + id + "\n" + newStats);
        });
        var sdpDelivered = false;
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

            publishers[from] = muxer;
            subscribers[from] = [];

            ei.setAudioReceiver(muxer);
            ei.setVideoReceiver(muxer);
            muxer.setExternalPublisher(ei);

            var answer = ei.init();

            if (answer >= 0) {
                callback('success');
            } else {
                callback(answer);
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
            publishers[to].addExternalOutput(externalOutput, url);
            externalOutputs[url] = externalOutput;
        }

    };

    that.removeExternalOutput = function (to, url) {
      if (externalOutputs[url] !== undefined && publishers[to]!=undefined) {
        logger.info("Stopping ExternalOutput: url " + url);
        publishers[to].removeSubscriber(url);
        delete externalOutputs[url];
      }
    };

    /*
     * Adds a publisher to the room. This creates a new OneToManyProcessor
     * and a new WebRtcConnection. This WebRtcConnection will be the publisher
     * of the OneToManyProcessor.
     */
    that.addPublisher = function (from, sdp, callback, onReady) {

        if (publishers[from] === undefined) {

            logger.info("Adding publisher peer_id ", from);

            var muxer = new addon.OneToManyProcessor(),
                wrtc = new addon.WebRtcConnection(true, true, config.erizo.stunserver, config.erizo.stunport, config.erizo.minport, config.erizo.maxport);

            publishers[from] = muxer;
            subscribers[from] = [];

            wrtc.setAudioReceiver(muxer);
            wrtc.setVideoReceiver(muxer);
            muxer.setPublisher(wrtc);

            initWebRtcConnection(wrtc, sdp, callback, onReady, from);

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
    that.addSubscriber = function (from, to, audio, video, sdp, callback) {

        if (publishers[to] !== undefined && subscribers[to].indexOf(from) === -1 && sdp.match('OFFER') !== null) {

            logger.info("Adding subscriber from ", from, 'to ', to);

            if (audio === undefined) audio = true;
            if (video === undefined) video = true;

            var wrtc = new addon.WebRtcConnection(audio, video, config.erizo.stunserver, config.erizo.stunport, config.erizo.minport, config.erizo.maxport);

            subscribers[to].push(from);
            publishers[to].addSubscriber(wrtc, from);

            initWebRtcConnection(wrtc, sdp, callback, undefined, from);
//            waitForFIR(wrtc, to);

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
            publishers[from].close();
            logger.info('Removing subscribers', from);
            delete subscribers[from];
            logger.info('Removing publisher', from);
            delete publishers[from];
            logger.info('Removed all');
        }
    };

    /*
     * Removes a subscriber from the room. This also removes it from the associated OneToManyProcessor.
     */
    that.removeSubscriber = function (from, to) {

        var index = subscribers[to].indexOf(from);
        if (index !== -1) {
            logger.info('Removing subscriber ', from, 'to muxer ', to);
            publishers[to].removeSubscriber(from);
            subscribers[to].splice(index, 1);
        }
    };

    /*
     * Removes all the subscribers related with a client.
     */
    that.removeSubscriptions = function (from) {

        var key, index;

        logger.info('Removing subscriptions of ', from);
        for (key in subscribers) {
            if (subscribers.hasOwnProperty(key)) {
                index = subscribers[key].indexOf(from);
                if (index !== -1) {
                    logger.info('Removing subscriber ', from, 'to muxer ', key);
                    publishers[key].removeSubscriber(from);
                    subscribers[key].splice(index, 1);
                }
            }
        }
    };

    return that;
};
