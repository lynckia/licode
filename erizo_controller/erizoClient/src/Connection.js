var Erizo = Erizo || {};

Erizo.sessionId = 103;

Erizo.Connection = function (spec) {
	"use strict";
	var that = {};

	spec.session_id = ++Erizo.sessionId;

	// Check which WebRTC Stack is installed.
	that.browser = "";
	if (window.navigator.appVersion.match(/Chrome\/(.*?)\./)[1] === "23") {
		// Google Chrome Stable.
		console.log("Stable!");
		that = Erizo.ChromeStableStack(spec);
		that.browser = "chrome-stable";
	} else if (window.navigator.appVersion.match(/Chrome\/(.*?)\./)[1] === "25") {
		// Google Chrome Canary.
		console.log("Canary!");
		that = Erizo.ChromeCanaryStack(spec);
		that.browser = "chrome-canary";
	} else if (webkitPeerConnection !== undefined) {
		// Bowser
		that.browser = "bowser";
	} else if (mozPeerConnection !== undefined) {
		// Firefox
		that.browser = "mozilla";
	} else {
		// None.
		that.browser = "none";
		throw "WebRTC stack not available";
	}

	return that;
};