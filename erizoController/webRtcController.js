var addon = require('./../erizoAPI/build/Release/addon');

var subscribers = {}; //id (muxer): array de subscribers
var publishers = {}; //id: muxer

exports.WebRtcController = function() {

    var that = {};

    that.addPublisher = function(from, msg, callback) {

        if(publishers[from] === undefined) {

            console.log("Adding publisher peer_id ", from);

            var roap = msg;
            var muxer = new addon.OneToManyProcessor();
            var newConn = new addon.WebRtcConnection();
            
            newConn.init();
            newConn.setAudioReceiver(muxer);
            newConn.setVideoReceiver(muxer);
            muxer.setPublisher(newConn);

            var remoteSdp = getSdp(roap);
            newConn.setRemoteSdp(remoteSdp);
            //console.log('SDP remote: ', remoteSdp);

            var localSdp = newConn.getLocalSdp();
            //console.log('SDP local: ', localSdp);

            var answer = getRoap(localSdp, roap);

            publishers[from] = muxer;
            subscribers[from] = new Array();
            
            //console.log('Publishers: ', publishers);
            //console.log('Subscribers: ', publishers);
            callback(answer);

        } else {
            console.log("Publisher already set for", from);
        }
    }

    that.addSubscriber = function(from, to, msg, callback) {

        if(publishers[to] !== undefined && subscribers[to].indexOf(from) === -1 && msg.match('OFFER') !== null) {

            console.log("Adding subscriber from ", from, 'to ', to);
            
            var roap = msg;
            var newConn = new addon.WebRtcConnection();
            
            newConn.init();
            publishers[to].addSubscriber(newConn, from);                                                        

            var remoteSdp = getSdp(roap);
            newConn.setRemoteSdp(remoteSdp);
            //console.log('SDP remote: ', remoteSdp);

            var localSdp = newConn.getLocalSdp();
            //console.log('SDP local: ', localSdp);

            var answer = getRoap(localSdp, roap);

            subscribers[to].push(from);

            //console.log('Publishers: ', publishers);
            //console.log('Subscribers: ', subscribers);
            callback(answer);
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