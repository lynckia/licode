
ErizoPeerConnection = function (signalingCallback) {

	var that = this;

	try {

		return new webkitPeerConnection("STUN stun.l.google.com:19302", function (offer) {
			
			console.log('PeerConnection BOWSER');

			var sdp = offer.substring(4);

			var reg5 = new RegExp(/\\r\\n/g);
			var sdp = sdp.replace(reg5, '\n');

			var username = sdp.match(/(username) \w+/)[0].substring(9);
			var pass = sdp.match(/(password) \w+/)[0].substring(9);

			var reg1 = new RegExp("(?: name)(.+)(?:" +pass+" )", "gm");

			var sdp = sdp.replace(reg1, ' ');

			var info = 'a=ice-ufrag:' + username + '\\r\\na=ice-pwd:' + pass + '\\r\\nc=IN';
			var reg2 = new RegExp(/(c=IN)/g);
			var sdp = sdp.replace(reg2, info);

			var reg4 = new RegExp(/\n/g);
			var sdp = sdp.replace(reg4, '\\r\\n');

			signalingCallback(sdp, offer);
		});
		

	} catch (e) {
		console.log('PeerConnection CHROME');
		return new RoapConnection("STUN stun.l.google.com:19302", signalingCallback);
	}

};

ErizoParseAnswer = function(answer) {

	var newAnswer = {};

	var username = answer.match(/(a=ice-ufrag:)(.+?)(\\r\\n)/)[2];
	var pass = answer.match(/(a=ice-pwd:)(.+?)(\\r\\n)/)[2];

	var reg1 = new RegExp(/(a=ice-ufrag:)(.+?)(\\r\\n)/g);
	var reg2 = new RegExp(/(a=ice-pwd:)(.+?)(\\r\\n)/g);

	answer = answer.replace(reg1, '');
	answer = answer.replace(reg2, '');

	newAnswer.answer = answer;

	var reg3 = new RegExp(/(generation)/g);

	var info1 = 'name rtp network_name en0 username ' + username + ' password ' + pass + ' magia';

	var info2 = 'name video_rtp network_name en0 username ' + username + ' password ' + pass + ' magia';

	var matches = answer.match(reg3);

	for (var i = 0; i < matches.length; i++) {
		if (i < matches.length/2) {
			answer = answer.replace(matches[i], info1);
		} else {
			answer = answer.replace(matches[i], info2);
		}
	}

	var reg3 = new RegExp(/(magia)/g);

	answer = answer.replace(reg3, 'generation');




	var matches2 = answer.match(/(a=candidate:)(.+?)(generation 0)/g);
	var matches3 = answer.match(/(generation 0)/g);

	for (var i = 0; i < matches2.length; i++) {

		var candidate = matches2[i];
		candidate = candidate.replace(/(rtp)/, 'rtcp');

		candidate = candidate.replace(/(generation)/, 'magia');

		var info3 = 'magia 0\\r\\n' + candidate;

		answer = answer.replace(matches3[i], info3);

	}

	answer = answer.replace(reg3, 'generation');




	//---------------------------------------------

	var reg10 = new RegExp(/(a=sendrecv\\r\\n)/g);

	answer = answer.replace(reg10, '');

	var reg11 = new RegExp(/(a=mid:video\\r\\n)/g);

	answer = answer.replace(reg11, 'a=mid:video\\r\\na=fmtp:103 profile-level-id=42C00B;packetization-mode=1\\r\\n');

	//---------------------------------------------

	answer = 'SDP ' + answer;

	return answer;

}

ErizoGetUserMedia = function (config, callback) {

	try	{

		navigator.webkitGetUserMedia("audio, video", callback);
		console.log('GetUserMedia BOWSER');

	} catch (e) {

		console.log('GetUserMedia CHROME');
		navigator.webkitGetUserMedia(config, callback);

	}

};