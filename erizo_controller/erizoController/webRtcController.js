/*global require, exports, console, setInterval, clearInterval*/

var addon = require('./../../erizoAPI/build/Release/addon');
var config = require('./../../licode_config');

exports.WebRtcController = function () {
    "use strict";

    var that = {},
        // {id: array of subscribers}
        subscribers = {},
        // {id: OneToManyProcessor}
        publishers = {},

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

                if (wrtc.getCurrentState() >= 2 && publishers[to].getPublisherState() >=2) {

                    publishers[to].sendFIR();
                    clearInterval(intervarId);
                }

            }, INTERVAL_TIME_FIR);
        }
    };

    /*
     * Given a WebRtcConnection waits for the state CANDIDATES_GATHERED for set remote SDP.
     */
    initWebRtcConnection = function (wrtc, sdp, callback, onReady) {

        wrtc.init();

        var roap = sdp,
            remoteSdp = getSdp(roap);

        console.log('SDP remote: ', remoteSdp);

        wrtc.setRemoteSdp(remoteSdp);

        var intervarId = setInterval(function () {

                var state = wrtc.getCurrentState(), localSdp, answer;

                if (state == 1) {
                    console.log('Get local SDP');
                    localSdp = wrtc.getLocalSdp();

                    answer = getRoap(localSdp, roap);
                    callback(answer);

                    
                }
                if (state == 2) {
                    if (onReady != undefined) {
                        onReady();
                    }
                }

                if (state >= 2) {
                    clearInterval(intervarId);
                }


            }, INTERVAL_TIME_SDP);
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

    /*
     * Adds a publisher to the room. This creates a new OneToManyProcessor
     * and a new WebRtcConnection. This WebRtcConnection will be the publisher
     * of the OneToManyProcessor.
     */
    that.addPublisher = function (from, sdp, callback, onReady) {

        if (publishers[from] === undefined) {

            console.log("Adding publisher peer_id ", from);

            var muxer = new addon.OneToManyProcessor(),
                wrtc = new addon.WebRtcConnection();

            publishers[from] = muxer;
            subscribers[from] = [];

            wrtc.setAudioReceiver(muxer);
            wrtc.setVideoReceiver(muxer);
            muxer.setPublisher(wrtc);

            initWebRtcConnection(wrtc, sdp, callback, onReady);

            //console.log('Publishers: ', publishers);
            //console.log('Subscribers: ', subscribers);

        } else {
            console.log("Publisher already set for", from);
        }
    };

    /*
     * Adds a subscriber to the room. This creates a new WebRtcConnection.
     * This WebRtcConnection will be added to the subscribers list of the
     * OneToManyProcessor.
     */
    that.addSubscriber = function (from, to, sdp, callback) {

        if (publishers[to] !== undefined && subscribers[to].indexOf(from) === -1 && sdp.match('OFFER') !== null) {

            console.log("Adding subscriber from ", from, 'to ', to);

            var wrtc = new addon.WebRtcConnection();

            subscribers[to].push(from);
            publishers[to].addSubscriber(wrtc, from);

            initWebRtcConnection(wrtc, sdp, callback);
            waitForFIR(wrtc, to);

            //console.log('Publishers: ', publishers);
            //console.log('Subscribers: ', subscribers);
        }
    };

    /*
     * Removes a publisher from the room. This also deletes the associated OneToManyProcessor.
     */
    that.removePublisher = function (from) {

        if (subscribers[from] !== undefined && publishers[from] !== undefined) {
            console.log('Removing muxer', from);
            publishers[from].close();
            console.log('Removing subscribers', from);
            delete subscribers[from];
            console.log('Removing publisher', from);
            delete publishers[from];
            console.log('Removed all');
        }
    };

    /*
     * Removes a subscriber from the room. This also removes it from the associated OneToManyProcessor.
     */
    that.removeSubscriber = function (from, to) {

        var index = subscribers[to].indexOf(from);
        if (index !== -1) {
            console.log('Removing subscriber ', from, 'to muxer ', to);
            publishers[to].removeSubscriber(from);
            subscribers[to].splice(index, 1);
        }
    };

    /*
     * Removes a client from the session. This removes the publisher and all the subscribers related.
     */
    that.removeClient = function (from, streamId) {

        var key, index;

        console.log('Removing client ', from);
        for (key in subscribers) {
            if (subscribers.hasOwnProperty(key)) {
                index = subscribers[key].indexOf(from);
                if (index !== -1) {
                    console.log('Removing subscriber ', from, 'to muxer ', key);
                    publishers[key].removeSubscriber(from);
                    subscribers[key].splice(index, 1);
                }
            }
        }

        if (subscribers[streamId] !== undefined && publishers[streamId] !== undefined) {
            console.log('Removing muxer', streamId);
            publishers[streamId].close();
            delete subscribers[streamId];
            delete publishers[streamId];
        }
    };

    return that;
};
