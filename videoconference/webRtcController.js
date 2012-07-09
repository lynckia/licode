var addon = require('./build/Release/addon');


var subscribers = {}; //id (muxer): array de subscribers
var publishers = {}; //id: muxer

exports.addPublisher = function(from, msg, callback) {

	if(publishers[from] === undefined) {

		var muxer = new addon.OneToManyProcessor();

		var roap = msg;

		console.log("\n************Adding publisher peer_id ", from);
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

		publishers[from] = muxer;
		subscribers[from] = new Array();
		
		//console.log('Publishers: ', publishers);

		callback('publisher', from, answer);

		

	} else {
		console.log("Publisher already set for", from);
	}
}

exports.addSubscriber = function(from, to, msg, callback) {

	//console.log('Publishers: ', publishers);

	if(from !== undefined && publishers[to] !== undefined && subscribers[to].indexOf(from) == -1 &&msg.match('OFFER') != null) {

		var roap = msg;
				
		console.log("\n************Adding subscriber from ", from, 'to ', to);
		var newConn = new addon.WebRtcConnection();
		newConn.init();
		
		publishers[to].addSubscriber(newConn);														

		var remoteSdp = getSdp(roap);
		//console.log('SDP remote: ', remoteSdp);

		newConn.setRemoteSdp(remoteSdp);

		var localSdp = newConn.getLocalSdp();
		//console.log('SDP local: ', localSdp);

		var answer = getRoap(localSdp, roap);

		subscribers[to].push(from);

		callback(to, from, answer);
	}
}


exports.deleteCheck = function (from) {
/*
	var i = subscribers.indexOf(from);

	if(i != -1) {
		subscribers.splice(i, 1);
	}	
*/
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