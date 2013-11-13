/*global window, console, navigator*/

var Erizo = Erizo || {};

Erizo.sessionId = 103;

Erizo.Connection = function (spec) {
    "use strict";
    var that = {};

    spec.session_id = (Erizo.sessionId += 1);

    // Check which WebRTC Stack is installed.
    that.browser = "";

    if (typeof module !== 'undefined' && module.exports) {
        L.Logger.error('Publish/subscribe video/audio streams not supported in erizofc yet');
        that = Erizo.FcStack(spec);
    } else if (window.navigator.userAgent.match("Firefox") !== null) {
        // Firefox
        that.browser = "mozilla";
        that = Erizo.FirefoxStack(spec);
    } else if (window.navigator.appVersion.match(/Chrome\/([\w\W]*?)\./)[1] <= 31) {
        // Google Chrome Stable.
        L.Logger.debug("Stable!");
        that = Erizo.ChromeStableStack(spec);
        that.browser = "chrome-stable";
    } else if (window.navigator.userAgent.toLowerCase().indexOf("chrome")>=0) {
        // Google Chrome Canary.
        L.Logger.debug("Canary!");
        that = Erizo.ChromeCanaryStack(spec);
        that.browser = "chrome-canary";
    }  else if (window.navigator.appVersion.match(/Bowser\/([\w\W]*?)\./)[1] === "25") {
        // Bowser
        that.browser = "bowser";
    } else {
        // None.
        that.browser = "none";
        throw "WebRTC stack not available";
    }

    return that;
};

Erizo.GetUserMedia = function (config, callback, error) {
    "use strict";

    navigator.getMedia = ( navigator.getUserMedia ||
                       navigator.webkitGetUserMedia ||
                       navigator.mozGetUserMedia ||
                       navigator.msGetUserMedia);

    if (typeof module !== 'undefined' && module.exports) {
        L.Logger.error('Video/audio streams not supported in erizofc yet');
    } else {
        navigator.getMedia(config, callback, error);
    }
};
