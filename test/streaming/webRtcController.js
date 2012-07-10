var addon = require('./../../erizoAPI/build/Release/addon');;

var muxer = new addon.OneToManyProcessor();

var subscribers = new Array();

exports.processMsg = function(from, to, msg, callback) {

	if (to == 'publisher'){
		if(!muxer.hasPublisher()) {

			var roap = msg;

			console.log("Adding publisher peer_id ", from);
			var newConn = new addon.WebRtcConnection();
			newConn.init();
			newConn.setAudioReceiver(muxer);
			newConn.setVideoReceiver(muxer);
			muxer.setPublisher(newConn);

			var remoteSdp = getSdp(roap);
			//console.log('SDP remote: ', remoteSdp);

			newConn.setRemoteSdp(remoteSdp);

			var localSdp = newConn.getLocalSdp();
			//console.log('SDP local: ', localSdp);

			var answer = getRoap(localSdp, roap);

			callback('publisher', from, answer);


		} else {
			console.log("Publisher already set");
			
		}
		return;
	}
	if (to == 'subscriber'){

		if(subscribers.indexOf(from) == -1) {

			subscribers.push(from);

			var roap = msg;
			
			console.log("Adding subscriber peer_id ", from);
			var newConn = new addon.WebRtcConnection();
			newConn.init();
			muxer.addSubscriber(newConn, from);

			var remoteSdp = getSdp(roap);
			//console.log('SDP remote: ', remoteSdp);

			newConn.setRemoteSdp(remoteSdp);

			var localSdp = newConn.getLocalSdp();
			//console.log('SDP local: ', localSdp);

			var answer = getRoap(localSdp, roap);

			callback('subscriber', from, answer);
		}


		return;
	}
}

exports.deleteCheck = function (from) {

	var i = subscribers.indexOf(from);

	if(i != -1) {
		muxer.removeSubscriber(from);
		subscribers.splice(i, 1);
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

	var answererSessionId = "106";

	var reg2 = new RegExp(/^.*offererSessionId\":(...).*$/);
	var offererSessionId = offerRoap.match(reg2)[1];

	//console.log('SDP envío: ', sdp);

	var answer = ('\n{\n \"messageType\":\"ANSWER\",\n');

	answer += ' \"sdp\":\"' + sdp + '\",\n';
	answer += ' \"offererSessionId\":' + offererSessionId + ',\n';
	answer += ' \"answererSessionId\":' + answererSessionId + ',\n \"seq\" : 1\n}\n';

	return answer;

}
