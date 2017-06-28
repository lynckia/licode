/* global L, window, chrome, navigator, Erizo*/
this.Erizo = this.Erizo || {};

Erizo.sessionId = 103;

Erizo.Connection = (specInput) => {
  let that = {};
  const spec = specInput;
  Erizo.sessionId += 1;

  spec.sessionId = Erizo.sessionId;

    // Check which WebRTC Stack is installed.
  that.browser = Erizo.getBrowser();
  if (that.browser === 'fake') {
    L.Logger.warning('Publish/subscribe video/audio streams not supported in erizofc yet');
    that = Erizo.FcStack(spec);
  } else if (that.browser === 'mozilla') {
    L.Logger.debug('Firefox Stack');
    that = Erizo.FirefoxStack(spec);
  } else if (that.browser === 'bowser') {
    L.Logger.debug('Bowser Stack');
    that = Erizo.BowserStack(spec);
  } else if (that.browser === 'chrome-stable' || that.browser === 'electron') {
    L.Logger.debug('Chrome Stable Stack');
    that = Erizo.ChromeStableStack(spec);
  } else {
    L.Logger.error('No stack available for this browser');
    throw new Error('WebRTC stack not available');
  }
  if (!that.updateSpec) {
    that.updateSpec = (newSpec, callback) => {
      L.Logger.error('Update Configuration not implemented in this browser');
      if (callback) { callback('unimplemented'); }
    };
  }

  return that;
};

Erizo.getBrowser = () => {
  let browser = 'none';

  if (typeof module !== 'undefined' && module.exports) {
    browser = 'fake';
  } else if (window.navigator.userAgent.match('Firefox') !== null) {
        // Firefox
    browser = 'mozilla';
  } else if (window.navigator.userAgent.match('Bowser') !== null) {
    browser = 'bowser';
  } else if (window.navigator.userAgent.match('Chrome') !== null) {
    browser = 'chrome-stable';
    if (window.navigator.userAgent.match('Electron') !== null) {
      browser = 'electron';
    }
  } else if (window.navigator.userAgent.match('Safari') !== null) {
    browser = 'bowser';
  } else if (window.navigator.userAgent.match('AppleWebKit') !== null) {
    browser = 'bowser';
  }
  return browser;
};

Erizo.GetUserMedia = (config, callback, error) => {
  let promise;
  let screenConfig;
  navigator.getMedia = (navigator.getUserMedia ||
                       navigator.webkitGetUserMedia ||
                       navigator.mozGetUserMedia ||
                       navigator.msGetUserMedia);

  if (config.screen) {
    L.Logger.debug('Screen access requested');
    switch (Erizo.getBrowser()) {
      case 'electron' :
        L.Logger.debug('Screen sharing in Electron');
        screenConfig = {};
        screenConfig.video = config.video || {};
        screenConfig.video.mandatory = config.video.mandatory || {};
        screenConfig.video.mandatory.chromeMediaSource = 'screen';
        navigator.getMedia(screenConfig, callback, error);
        break;
      case 'mozilla':
        L.Logger.debug('Screen sharing in Firefox');
        screenConfig = {};
        if (config.video.mandatory !== undefined) {
          screenConfig.video = config.video;
          screenConfig.video.mediaSource = 'window' || 'screen';
        } else {
          screenConfig = {
            audio: config.audio,
            video: { mediaSource: 'window' || 'screen' },
          };
        }
        if (navigator.mediaDevices && navigator.mediaDevices.getUserMedia) {
          promise = navigator.mediaDevices.getUserMedia(screenConfig).then(callback);
                    // Google compressor complains about a func called catch
          promise.catch(error);
        } else {
          navigator.getMedia(screenConfig, callback, error);
        }
        break;

      case 'chrome-stable':
        L.Logger.debug('Screen sharing in Chrome');
        if (config.desktopStreamId) {
          screenConfig.video = config.video || {};
          screenConfig.video.mandatory.chromeMediaSource = 'desktop';
          screenConfig.video.mandatory.chromeMediaSourceId = config.desktopStreamId;
          navigator.getMedia(screenConfig, callback, error);
        } else {
                  // Default extensionId - this extension is only usable in our server,
                  // please make your own extension based on the code in
                  // erizo_controller/erizoClient/extras/chrome-extension
          let extensionId = 'okeephmleflklcdebijnponpabbmmgeo';
          if (config.extensionId) {
            L.Logger.debug(`extensionId supplied, using ${config.extensionId}`);
            extensionId = config.extensionId;
          }
          L.Logger.debug('Screen access on chrome stable, looking for extension');
          try {
            chrome.runtime.sendMessage(extensionId, { getStream: true },
                        (response) => {
                          if (response === undefined) {
                            L.Logger.error('Access to screen denied');
                            const theError = { code: 'Access to screen denied' };
                            error(theError);
                            return;
                          }
                          const theId = response.streamId;
                          if (config.video.mandatory !== undefined) {
                            screenConfig.video = config.video;
                            screenConfig.video.mandatory.chromeMediaSource = 'desktop';
                            screenConfig.video.mandatory.chromeMediaSourceId = theId;
                          } else {
                            screenConfig = { video: { mandatory: { chromeMediaSource: 'desktop',
                              chromeMediaSourceId: theId } } };
                          }
                          navigator.getMedia(screenConfig, callback, error);
                        });
          } catch (e) {
            L.Logger.debug('Screensharing plugin is not accessible ');
            const theError = { code: 'no_plugin_present' };
            error(theError);
          }
        }
        break;

      default:
        L.Logger.error('This browser does not support ScreenSharing');
    }
  } else if (typeof module !== 'undefined' && module.exports) {
    L.Logger.error('Video/audio streams not supported in erizofc yet');
  } else {
    if (config.video && Erizo.getBrowser() === 'mozilla') {
      const ffConfig = { video: {}, audio: config.audio, screen: config.screen };
      if (config.audio.mandatory !== undefined) {
        const audioCfg = config.audio.mandatory;
        if (audioCfg.sourceId) {
          ffConfig.audio.deviceId = audioCfg.sourceId;
        }
      }
      if (config.video.mandatory !== undefined) {
        const videoCfg = config.video.mandatory;
        ffConfig.video.width = { min: videoCfg.minWidth, max: videoCfg.maxWidth };
        ffConfig.video.height = { min: videoCfg.minHeight, max: videoCfg.maxHeight };
        if (videoCfg.sourceId) {
          ffConfig.video.deviceId = videoCfg.sourceId;
        }
      }
      if (config.video.optional !== undefined) {
        ffConfig.video.frameRate = config.video.optional[1].maxFrameRate;
      }
      if (navigator.mediaDevices && navigator.mediaDevices.getUserMedia) {
        promise = navigator.mediaDevices.getUserMedia(ffConfig).then(callback);
                    // Google compressor complains about a func called catch
        promise.catch(error);
        return;
      }
    }
    navigator.getMedia(config, callback, error);
  }
};
