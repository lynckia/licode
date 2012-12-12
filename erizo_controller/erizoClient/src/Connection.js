/*global window, console, navigator*/

var Erizo = Erizo || {};

Erizo.sessionId = 103;

Erizo.Connection = function (spec) {
    "use strict";
    var that = {};

    spec.session_id = (Erizo.sessionId += 1);

    // Check which WebRTC Stack is installed.
    that.browser = "";
    if (window.navigator.appVersion.match(/Chrome\/([\w\W]*?)\./)[1] === "23") {
        // Google Chrome Stable.
        console.log("Stable!");
        that = Erizo.ChromeStableStack(spec);
        that.browser = "chrome-stable";
    } else if (window.navigator.appVersion.match(/Chrome\/([\w\W]*?)\./)[1] === "25") {
        // Google Chrome Canary.
        console.log("Canary!");
        that = Erizo.ChromeCanaryStack(spec);
        that.browser = "chrome-canary";
    } else if (window.navigator.appVersion.match(/Bowser\/([\w\W]*?)\./)[1] === "25") {
        // Bowser
        that.browser = "bowser";
    } else if (window.navigator.appVersion.match(/Mozilla\/([\w\W]*?)\./)[1] === "25") {
        // Firefox
        that.browser = "mozilla";
    } else {
        // None.
        that.browser = "none";
        throw "WebRTC stack not available";
    }

    return that;
};

Erizo.GetUserMedia = function (config, callback) {
    "use strict";

    try {
        navigator.webkitGetUserMedia("audio, video", callback);
        console.log('GetUserMedia BOWSER');
    } catch (e) {
        navigator.webkitGetUserMedia(config, callback);
        console.log('GetUserMedia CHROME');
    }

};