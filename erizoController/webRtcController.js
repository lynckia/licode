var addon = require('./../erizoAPI/build/Release/addon');

exports.WebRtcController = function() {

    var that = {};

    that.subscribers = {}; //id (muxer): array de subscribers
    that.publishers = {}; //id: muxer

    that.addPublisher = function(from, sdp, callback) {

        if(that.publishers[from] === undefined) {

            console.log("Adding publisher peer_id ", from);

            var roap = sdp;
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

            that.publishers[from] = muxer;
            that.subscribers[from] = new Array();

            callback(answer);
             
            //console.log('Publishers: ', that.publishers);
            //console.log('Subscribers: ', that.subscribers);

        } else {
            console.log("Publisher already set for", from);
        }
    }

    that.addSubscriber = function(from, to, sdp, callback) {

        if(that.publishers[to] !== undefined && that.subscribers[to].indexOf(from) === -1 && sdp.match('OFFER') !== null) {

            console.log("Adding subscriber from ", from, 'to ', to);

            var roap = sdp;
            var newConn = new addon.WebRtcConnection();
            
            newConn.init();
            that.publishers[to].addSubscriber(newConn, from);                                                        

            var remoteSdp = getSdp(roap);
            newConn.setRemoteSdp(remoteSdp);
            //console.log('SDP remote: ', remoteSdp);

            var localSdp = newConn.getLocalSdp();
            //console.log('SDP local: ', localSdp);

            var answer = getRoap(localSdp, roap);
                        
            that.subscribers[to].push(from);

            callback(answer);

            //console.log('Publishers: ', that.publishers);
            //console.log('Subscribers: ', that.subscribers);
        }
    }

    that.removePublisher = function(from) {

        if(that.subscribers[from] != undefined && that.publishers[from] != undefined) {
            console.log('Removing muxer', from);
            that.publishers[from].close();
            delete that.subscribers[from];
            delete that.publishers[from];    
        }
    }

    that.removeSubscriber = function(from, to) {

        var index = that.subscribers[to].indexOf(from);
        if (index != -1) {
            console.log('Removing subscriber ', from, 'to muxer ', to);
            that.publishers[to].removeSubscriber(from);
            that.subscribers[to].splice(index, 1);
        }
    }

    that.removeClient = function(from) {

        console.log('Removing client ', from);
        for(var key in that.subscribers) {
            var index = that.subscribers[key].indexOf(from);
            if (index != -1) {
                console.log('Removing subscriber ', from, 'to muxer ', key);
                that.publishers[key].removeSubscriber(from);
                that.subscribers[key].splice(index, 1);
            };
        }

        if(that.subscribers[from] != undefined && that.publishers[from] != undefined) {
            console.log('Removing muxer', from);
            that.publishers[from].close();
            delete that.subscribers[from];
            delete that.publishers[from];    
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