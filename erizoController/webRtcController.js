var addon = require('./../erizoAPI/build/Release/addon');

exports.WebRtcController = function() {

    var that = {};

    var subscribers = {}; //id (muxer): array de subscribers
    var publishers = {}; //id: muxer

    var INTERVAL_TIME = 200;

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

    that.addSubscriber = function(from, to, sdp, callback) {

        if(publishers[to] !== undefined && subscribers[to].indexOf(from) === -1 && sdp.match('OFFER') !== null) {

            console.log("Adding subscriber from ", from, 'to ', to);
            
            var wrtc = new addon.WebRtcConnection();

            subscribers[to].push(from);
            publishers[to].addSubscriber(wrtc, from); 

            initWebRtcConnection(wrtc, sdp, callback);

            //console.log('Publishers: ', publishers);
            //console.log('Subscribers: ', subscribers);
        }
    }

    that.removePublisher = function(from) {

        if(subscribers[from] != undefined && publishers[from] != undefined) {
            console.log('Removing muxer', from);
            publishers[from].close();
            delete subscribers[from];
            delete publishers[from];    
        }
    }

    that.removeSubscriber = function(from, to) {

        var index = subscribers[to].indexOf(from);
        if (index != -1) {
            console.log('Removing subscriber ', from, 'to muxer ', to);
            publishers[to].removeSubscriber(from);
            subscribers[to].splice(index, 1);
        }
    }

    that.removeClient = function(from) {

        console.log('Removing client ', from);
        for(var key in subscribers) {
            var index = subscribers[key].indexOf(from);
            if (index != -1) {
                console.log('Removing subscriber ', from, 'to muxer ', key);
                publishers[key].removeSubscriber(from);
                subscribers[key].splice(index, 1);
            };
        }

        if(subscribers[from] != undefined && publishers[from] != undefined) {
            console.log('Removing muxer', from);
            publishers[from].close();
            delete subscribers[from];
            delete publishers[from];    
        }
    }

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

        }, INTERVAL_TIME);
    }

    var getSdp = function(roap) {

        var reg1 = new RegExp(/^.*sdp\":\"(.*)\",.*$/);
        var sdp = roap.match(reg1)[1];

        var reg2 = new RegExp(/\\r\\n/g);
        var sdp = sdp.replace(reg2, '\n');

        return sdp;

    }

    var getRoap = function (sdp, offerRoap) {

        var reg1 = new RegExp(/\n/g);
        sdp = sdp.replace(reg1, '\\r\\n');

        var reg2 = new RegExp(/^.*offererSessionId\":(...).*$/);
        var offererSessionId = offerRoap.match(reg2)[1];

        var answererSessionId = "106";
        var answer = ('\n{\n \"messageType\":\"ANSWER\",\n');
        answer += ' \"sdp\":\"' + sdp + '\",\n';
        answer += ' \"offererSessionId\":' + offererSessionId + ',\n';
        answer += ' \"answererSessionId\":' + answererSessionId + ',\n \"seq\" : 1\n}\n';

        return answer;
    }

    return that;
}