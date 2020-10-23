/* global navigator, window, chrome */
import Logger from './Logger';

const log = Logger.module('ConnectionHelpers');

const getBrowser = () => {
  let browser = 'none';

  if ((typeof module !== 'undefined' && module.exports)) {
    browser = 'fake';
  } else if (window.navigator.userAgent.match('Firefox') !== null) {
    // Firefox
    browser = 'mozilla';
  } else if (window.navigator.userAgent.match('Chrome') !== null) {
    browser = 'chrome-stable';
    if (window.navigator.userAgent.match('Electron') !== null) {
      browser = 'electron';
    }
  } else if (window.navigator.userAgent.match('Safari') !== null) {
    browser = 'safari';
  } else if (window.navigator.userAgent.match('AppleWebKit') !== null) {
    browser = 'safari';
  }
  return browser;
};

const GetUserMedia = (config, callback = () => {}, error = () => {}) => {
  let screenConfig;

  const getUserMedia = (userMediaConfig, cb, errorCb) => {
    navigator.mediaDevices.getUserMedia(userMediaConfig).then(cb).catch(errorCb);
  };

  const getDisplayMedia = (userMediaConfig, cb, errorCb) => {
    navigator.mediaDevices.getDisplayMedia(userMediaConfig).then(cb).catch(errorCb);
  };

  const configureScreensharing = () => {
    switch (getBrowser()) {
      case 'electron' :
        log.debug('message: Screen sharing in Electron');
        screenConfig = {};
        screenConfig.video = config.video || {};
        screenConfig.video.mandatory = config.video.mandatory || {};
        screenConfig.video.mandatory.chromeMediaSource = 'desktop';
        screenConfig.video.mandatory.chromeMediaSourceId = config.desktopStreamId;
        getUserMedia(screenConfig, callback, error);
        break;
      case 'mozilla':
        log.debug('message: Screen sharing in Firefox');
        screenConfig = {};
        if (config.video !== undefined) {
          screenConfig.video = config.video;
          if (!screenConfig.video.mediaSource) {
            screenConfig.video.mediaSource = 'window' || 'screen';
          }
        } else {
          screenConfig = {
            audio: config.audio,
            video: { mediaSource: 'window' || 'screen' },
          };
        }
        getUserMedia(screenConfig, callback, error);
        break;

      case 'chrome-stable':
        log.debug('message: Screen sharing in Chrome');
        screenConfig = {};
        if (config.desktopStreamId) {
          screenConfig.video = config.video || { mandatory: {} };
          screenConfig.video.mandatory = screenConfig.video.mandatory || {};
          screenConfig.video.mandatory.chromeMediaSource = 'desktop';
          screenConfig.video.mandatory.chromeMediaSourceId = config.desktopStreamId;
          getUserMedia(screenConfig, callback, error);
        } else {
          // Default extensionId - this extension is only usable in our server,
          // please make your own extension based on the code in
          // erizo_controller/erizoClient/extras/chrome-extension
          let extensionId = 'okeephmleflklcdebijnponpabbmmgeo';
          if (config.extensionId) {
            log.debug(`message: extensionId supplied, extensionId: ${config.extensionId}`);
            extensionId = config.extensionId;
          }
          log.debug('message: Screen access on chrome stable looking for extension');
          try {
            chrome.runtime.sendMessage(extensionId, { getStream: true },
              (response) => {
                if (response === undefined) {
                  log.error('message: Access to screen denied');
                  const theError = { code: 'Access to screen denied' };
                  error(theError);
                  return;
                }
                const theId = response.streamId;
                if (config.video.mandatory !== undefined) {
                  screenConfig.video = config.video || { mandatory: {} };
                  screenConfig.video.mandatory.chromeMediaSource = 'desktop';
                  screenConfig.video.mandatory.chromeMediaSourceId = theId;
                } else {
                  screenConfig = { video: { mandatory: { chromeMediaSource: 'desktop',
                    chromeMediaSourceId: theId } } };
                }
                getUserMedia(screenConfig, callback, error);
              });
          } catch (e) {
            log.debug('message: Screensharing plugin is not accessible');
            const theError = { code: 'no_plugin_present' };
            error(theError);
          }
        }
        break;
      default:
        log.error('message: This browser does not support ScreenSharing');
    }
  };

  if (config.screen) {
    if (config.desktopStreamId || config.extensionId) {
      log.debug('message: Screen access requested using GetUserMedia');
      configureScreensharing();
    } else {
      log.debug('message: Screen access requested using GetDisplayMedia');
      getDisplayMedia(config, callback, error);
    }
  } else if (typeof module !== 'undefined' && module.exports) {
    log.error('message: Video/audio streams not supported in erizofc yet');
  } else {
    log.debug(`message: Calling getUserMedia, config: ${JSON.stringify(config)}`);
    getUserMedia(config, callback, error);
  }
};


const ConnectionHelpers = { GetUserMedia, getBrowser };

export default ConnectionHelpers;
