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
    } else if (window.navigator.appVersion.match(/Chrome\/([\w\W]*?)\./)[1] >= 26) {
        // Google Chrome Stable.
        L.Logger.debug("Stable");
        that = Erizo.ChromeStableStack(spec);
        that.browser = "chrome-stable";
    } else if (window.navigator.userAgent.toLowerCase().indexOf("chrome")>=40) {
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

    if (config.screen){
      L.Logger.debug("Screen access requested");
      if (!window.navigator.appVersion.match(/Chrome\/([\w\W]*?)\./)[1] >= 34){
        error({code:"This browser does not support screen sharing"});
        return;
      }
      L.Logger.debug("Screen access on chrome stable, looking for extension");
      var extensionId = "okeephmleflklcdebijnponpabbmmgeo";
      try{
        chrome.runtime.sendMessage(extensionId,{getStream:true}, function (response){
          if (response==undefined){
            L.Logger.debug("Access to screen denied");
            var theError = {code:"Access to screen denied"};
            error(theError);
            return;
          }
          var theId = response.streamId;
          config = {video: {mandatory: {chromeMediaSource: 'desktop',  chromeMediaSourceId: theId }}};
          navigator.getMedia(config,callback,error);
        });
      } catch (e){
        L.Logger.debug("Lynckia screensharing plugin is not accessible ");
        var theError = {code:"no_plugin_present"};
        error(theError);
        return;
      }
    }else{
      if (typeof module !== 'undefined' && module.exports) {
        L.Logger.error('Video/audio streams not supported in erizofc yet');
      } else {
        navigator.getMedia(config, callback, error);
      }
    }
};

