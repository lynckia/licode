var addon = require('./../../erizoAPI/build/Release/addon');

exports.WebRtcController = function() {

    var that = {};

    // {id: array of subscribers}
    var subscribers = {}; 

    // {id: OneToManyProcessor}
    var publishers = {}; 

    var INTERVAL_TIME_SDP = 100;
    var INTERVAL_TIME_FIR = 100;

    /*
     * Adds a publisher to the room. This creates a new OneToManyProcessor
     * and a new WebRtcConnection. This WebRtcConnection will be the publisher
     * of the OneToManyProcessor.
     */
    that.addPublisher = function(from, sdp, callback) {

        if(publishers[from] === undefined) {

            console.log("Adding publisher peer_id ", from);

            var muxer = new addon.OneToManyProcessor();
            var wrtc = new addon.WebRtcConnection();

            publishers[from] = muxer;
            subscribers[from] = new Array();

            wrtc.setAudioReceiver(muxer);
            wrtc.setVideoReceiver(muxer);
            muxer.setPublisher(wrtc);

            initWebRtcConnection(wrtc, sdp, callback);

            //console.log('Publishers: ', publishers);
            //console.log('Subscribers: ', subscribers);

        } else {
            console.log("Publisher already set for", from);
        }
    }

    /*
     * Adds a subscriber to the room. This creates a new WebRtcConnection. 
     * This WebRtcConnection will be added to the subscribers list of the
     * OneToManyProcessor.
     */
    that.addSubscriber = function(from, to, sdp, callback) {

        if(publishers[to] !== undefined && subscribers[to].indexOf(from) === -1 && sdp.match('OFFER') !== null) {

            console.log("Adding subscriber from ", from, 'to ', to);
            
            var wrtc = new addon.WebRtcConnection();

            subscribers[to].push(from);
            publishers[to].addSubscriber(wrtc, from); 

            initWebRtcConnection(wrtc, sdp, callback);
            waitForFIR(wrtc, to);

            //console.log('Publishers: ', publishers);
            //console.log('Subscribers: ', subscribers);
        }
    }

    /*
     * Removes a publisher from the room. This also deletes the associated OneToManyProcessor.
     */
    that.removePublisher = function(from) {

        if(subscribers[from] != undefined && publishers[from] != undefined) {
            console.log('Removing muxer', from);
            publishers[from].close();
            delete subscribers[from];
            delete publishers[from];    
        }
    }

    /*
     * Removes a subscriber from the room. This also removes it from the associated OneToManyProcessor.
     */
    that.removeSubscriber = function(from, to) {

        var index = subscribers[to].indexOf(from);
        if (index != -1) {
            console.log('Removing subscriber ', from, 'to muxer ', to);
            publishers[to].removeSubscriber(from);
            subscribers[to].splice(index, 1);
        }
    }

    /*
     * Removes a client from the session. This removes the publisher and all the subscribers related.
     */
    that.removeClient = function(from, streamId) {

        console.log('Removing client ', from);
        for(var key in subscribers) {
            var index = subscribers[key].indexOf(from);
            if (index != -1) {
                console.log('Removing subscriber ', from, 'to muxer ', key);
                publishers[key].removeSubscriber(from);
                subscribers[key].splice(index, 1);
            };
        }

        if(subscribers[streamId] != undefined && publishers[streamId] != undefined) {
            console.log('Removing muxer', streamId);
            publishers[streamId].close();
            delete subscribers[streamId];
            delete publishers[streamId];    
        }
    }

    /*
     * Given a WebRtcConnection waits for the state READY for ask it to send a FIR packet to its publisher. 
     */
    var waitForFIR = function(wrtc, to) {

        if(publishers[to] !== undefined) {
            var intervarId = setInterval(function() {

                var state = wrtc.getCurrentState();

                if(state > 2) {

                    publishers[to].sendFIR();                    
                    clearInterval(intervarId);
                }

            }, INTERVAL_TIME_FIR);
        }
    }

    /*
     * Given a WebRtcConnection waits for the state CANDIDATES_GATHERED for set remote SDP. 
     */
    var initWebRtcConnection = function(wrtc, sdp, callback) {

        wrtc.init();
                       
        var roap = sdp;                                            
        var remoteSdp = getSdp(roap);

        var intervarId = setInterval(function() {

            var state = wrtc.getCurrentState();

            if(state > 0) {

                wrtc.setRemoteSdp(remoteSdp);
                //console.log('SDP remote: ', remoteSdp);
                var localSdp = wrtc.getLocalSdp();
                //console.log('SDP local: ', localSdp);

                var answer = getRoap(localSdp, roap);
                callback(answer);

                clearInterval(intervarId);
            }

        }, INTERVAL_TIME_SDP);
    }

    /*
     * Gets SDP from roap message.
     */
    var getSdp = function(roap) {

        var reg1 = new RegExp(/^.*sdp\":\"(.*)\",.*$/);
        var sdp = roap.match(reg1)[1];

        var reg2 = new RegExp(/\\r\\n/g);
        var sdp = sdp.replace(reg2, '\n');

        return sdp;

    }

    /*
     * Gets roap message from SDP.
     */
    var getRoap = function (sdp, offerRoap) {

        var reg1 = new RegExp(/\n/g);
        sdp = sdp.replace(reg1, '\\r\\n');

        var offererSessionId = offerRoap.match(/("offererSessionId":)(.+?)(,)/)[0];

        //var reg2 = new RegExp(/^.*offererSessionId\":(...).*$/);
        //var offererSessionId = offerRoap.match(reg2)[1];

        var answererSessionId = "106";
        var answer = ('\n{\n \"messageType\":\"ANSWER\",\n');
        answer += ' \"sdp\":\"' + sdp + '\",\n';
        //answer += ' \"offererSessionId\":' + offererSessionId + ',\n';
        answer += ' ' + offererSessionId + '\n';
        answer += ' \"answererSessionId\":' + answererSessionId + ',\n \"seq\" : 1\n}\n';

        return answer;
    }

    return that;
}