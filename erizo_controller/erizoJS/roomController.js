/*global require, exports, , setInterval, clearInterval*/

var addon = require('./../../erizoAPI/build/Release/addon');
var config = require('./../../licode_config');
var logger = require('./../common/logger').logger;

exports.RoomController = function () {
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
        initWebRtcConnection,
        getSdp,
        getRoap;

    // PRIVATE FUNCTIONS

    /*
     * Given a WebRtcConnection waits for the state CANDIDATES_GATHERED for set remote SDP. 
     */
    initWebRtcConnection = function (id, wrtc, sdp, callback) {
        logger.info("Initializing WebRTCConnection");
        wrtc.init( function (newStatus){
          var localSdp, answer;
          logger.info("webrtc Addon status" + newStatus );
          if (newStatus === 102 && !sdpDelivered) {
            localSdp = wrtc.getLocalSdp();
            answer = getRoap(localSdp, roap);
            callback("callback", answer);
            sdpDelivered = true;
          }
          if (newStatus === 103) {
            callback("onReady");
          }
        });

        var roap = sdp,
            remoteSdp = getSdp(roap);
        wrtc.setRemoteSdp(remoteSdp);

        wrtc.getStats(function (newStats){
          //logger.info("newStats for id " + id + "\n" + newStats);
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

    // PUBLIC FUNCTIONS
    /*
     * Adds and external input to the room. Typically with an URL of type: "rtsp://host"
     */
    that.addExternalInput = function (publisher_id, url, callback) {

        if (publishers[publisher_id] === undefined) {

            logger.info("Adding external input peer_id ", publisher_id);

            var muxer = new addon.OneToManyProcessor(),
                ei = new addon.ExternalInput(url);

            publishers[publisher_id] = muxer;
            subscribers[publisher_id] = [];

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
            logger.info("Publisher already set for", publisher_id);
        }
    };

    /*
     * Adds and external output to the room. Typically a file to which it will record the publisher's stream.
     */
    that.addExternalOutput = function (publisher_id, url) {
        if (publishers[publisher_id] !== undefined) {
            logger.info("Adding ExternalOutput to " + publisher_id + " url " + url);
            var externalOutput = new addon.ExternalOutput(url);
            externalOutput.init();
            publishers[publisher_id].addExternalOutput(externalOutput, url);
            externalOutputs[url] = externalOutput;
        }

    };

    /*
     * Removes an external output.
     */
    that.removeExternalOutput = function (publisher_id, url) {
      if (externalOutputs[url] !== undefined && publishers[publisher_id]!=undefined) {
        logger.info("Subscribing ExternalOutput: url " + url);
        publishers[publisher_id].removeSubscriber(url);
        delete externalOutputs[url];
      }
    };

    /*
     * Adds a publisher to the room. This creates a new OneToManyProcessor
     * and a new WebRtcConnection. This WebRtcConnection will be the publisher
     * of the OneToManyProcessor.
     */
    that.addPublisher = function (publisher_id, sdp, callback) {

        if (publishers[publisher_id] === undefined) {

            logger.info("Adding publisher peer_id ", publisher_id, " with sdp: ", sdp);

            var muxer = new addon.OneToManyProcessor(),
                wrtc = new addon.WebRtcConnection(true, true, config.erizo.stunserver, config.erizo.stunport, config.erizo.minport, config.erizo.maxport);

            publishers[publisher_id] = muxer;
            subscribers[publisher_id] = [];

            wrtc.setAudioReceiver(muxer);
            wrtc.setVideoReceiver(muxer);
            muxer.setPublisher(wrtc);

            initWebRtcConnection(publisher_id, wrtc, sdp, callback);

            //logger.info('Publishers: ', publishers);
            //logger.info('Subscribers: ', subscribers);

        } else {
            logger.info("Publisher already set for", publisher_id);
        }
    };

    /*
     * Adds a subscriber to the room. This creates a new WebRtcConnection.
     * This WebRtcConnection will be added to the subscribers list of the
     * OneToManyProcessor.
     */
    that.addSubscriber = function (subscriber_id, publisher_id, audio, video, sdp, callback) {

        if (publishers[publisher_id] !== undefined && subscribers[publisher_id].indexOf(subscriber_id) === -1 && sdp.match('OFFER') !== null) {

            logger.info("Adding subscriber ", subscriber_id, ' to ', publisher_id);

            if (audio === undefined) audio = true;
            if (video === undefined) video = true;

            var wrtc = new addon.WebRtcConnection(audio, video, config.erizo.stunserver, config.erizo.stunport, config.erizo.minport, config.erizo.maxport);

            subscribers[publisher_id].push(subscriber_id);
            publishers[publisher_id].addSubscriber(wrtc, subscriber_id);
            console.log(wrtc, publishers[publisher_id], subscriber_id);

            initWebRtcConnection(subscriber_id, wrtc, sdp, callback);
//            waitForFIR(wrtc, publisher_id);

            //logger.info('Publishers: ', publishers);
            //logger.info('Subscribers: ', subscribers);
        }
    };

    /*
     * Removes a publisher from the room. This also deletes the associated OneToManyProcessor.
     */
    that.removePublisher = function (publisher_id) {

        if (subscribers[publisher_id] !== undefined && publishers[publisher_id] !== undefined) {
            logger.info('Removing muxer', publisher_id);
            publishers[publisher_id].close();
            logger.info('Removing subscribers', publisher_id);
            delete subscribers[publisher_id];
            logger.info('Removing publisher', publisher_id);
            delete publishers[publisher_id];
            logger.info('Removed all');

            // We end this process.
            process.exit(0);
        }
    };

    /*
     * Removes a subscriber from the room. This also removes it from the associated OneToManyProcessor.
     */
    that.removeSubscriber = function (subscriber_id, publisher_id) {

        var index = subscribers[publisher_id].indexOf(subscriber_id);
        if (index !== -1) {
            logger.info('Removing subscriber ', subscriber_id, 'to muxer ', publisher_id);
            publishers[publisher_id].removeSubscriber(subscriber_id);
            subscribers[publisher_id].splice(index, 1);
        }
    };

    /*
     * Removes all the subscribers related with a client.
     */
    that.removeSubscriptions = function (subscriber_id) {

        var publisher_id, index;

        logger.info('Removing subscriptions of ', subscriber_id);
        for (publisher_id in subscribers) {
            if (subscribers.hasOwnProperty(publisher_id)) {
                index = subscribers[publisher_id].indexOf(subscriber_id);
                if (index !== -1) {
                    logger.info('Removing subscriber ', subscriber_id, 'to muxer ', publisher_id);
                    publishers[publisher_id].removeSubscriber(subscriber_id);
                    subscribers[publisher_id].splice(index, 1);
                }
            }
        }
    };

    return that;
};
