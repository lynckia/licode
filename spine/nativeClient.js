
const addon = require('./../erizoAPI/build/Release/addon'); // eslint-disable-line import/no-unresolved
const licodeConfig = require('./../licode_config');
const mediaConfig = require('./../rtp_media_config');
const logger = require('./logger').logger;

const log = logger.getLogger('NativeClient');

global.config = licodeConfig || {};
global.mediaConfig = mediaConfig || {};

const threadPool = new addon.ThreadPool(10);
threadPool.start();

const ioThreadPool = new addon.IOThreadPool(1);
if (global.config.erizo.useNicer) {
  ioThreadPool.start();
}

exports.ErizoNativeConnection = (config) => {
  const that = {};
  let wrtc;
  let syntheticInput;
  let externalInput;
  let oneToMany;
  let externalOutput;
  const configuration = Object.assign({}, config);

  that.connected = false;

  const CONN_INITIAL = 101;
  // CONN_STARTED = 102
  const CONN_GATHERED = 103;
  const CONN_READY = 104;
  // CONN_FINISHED = 105,
  const CONN_CANDIDATE = 201;
  const CONN_SDP = 202;
  const CONN_FAILED = 500;

  const generatePLIs = () => {
    externalOutput.interval = setInterval(() => {
      wrtc.generatePLIPacket();
    }, 1000);
  };

  log.info('NativeConnection constructor', configuration);
  const initWebRtcConnection = (initConnectionCallback) => {
    wrtc.init((newStatus, mess) => {
      log.info('webrtc Addon status ', newStatus);
      switch (newStatus) {
        case CONN_INITIAL:
          if (configuration.video && configuration.video.file && !externalInput) {
            that.prepareVideo(configuration.video.file);
          } else if (configuration.video && configuration.video.recording && !externalOutput) {
            that.prepareRecording(configuration.video.recording);
          } else if (configuration.video && configuration.video.synthetic && !syntheticInput) {
            that.prepareSynthetic(configuration.video.synthetic);
          } else {
            that.prepareNull();
          }
          initConnectionCallback('callback', { type: 'started' });
          break;

        case CONN_SDP:
        case CONN_GATHERED:
          setTimeout(() => {
            initConnectionCallback('callback', { type: 'offer', sdp: mess });
          }, 100);
          break;

        case CONN_CANDIDATE:
          initConnectionCallback('callback', { type: 'candidate', sdp: mess });
          break;

        case CONN_FAILED:
          log.warn('Connection failed the ICE process');
          that.connected = false;
          //   callback('callback', {type: 'failed', sdp: mess});
          break;

        case CONN_READY:
          log.info('Connection ready');
          that.connected = true;
          if (externalInput !== undefined) {
            log.info('Will start External Input');
            externalInput.init();
          }
          if (syntheticInput !== undefined) {
            log.info('Will start Synthetic Input');
            syntheticInput.init();
          }
          if (externalOutput !== undefined) {
            log.info('Will start External Output');
            externalOutput.init();
            generatePLIs();
          }
          break;

        default:
          log.error('Unknown status', newStatus);
          break;
      }
    });
  };


  wrtc = new addon.WebRtcConnection(threadPool, ioThreadPool, `spine_${configuration.sessionId}`,
  global.config.erizo.stunserver,
  global.config.erizo.stunport,
  global.config.erizo.minport,
  global.config.erizo.maxport,
  false,
  JSON.stringify(global.mediaConfig),
  global.config.erizo.useNicer,
  global.config.erizo.turnserver,
  global.config.erizo.turnport,
  global.config.erizo.turnusername,
  global.config.erizo.turnpass,
  global.config.erizo.networkinterface);

  that.createOffer = () => {
  };

  that.prepareNull = () => {
    log.info('Preparing null output');
    oneToMany = new addon.OneToManyProcessor();
    wrtc.setVideoReceiver(oneToMany);
    wrtc.setAudioReceiver(oneToMany);
    oneToMany.setPublisher(wrtc);
  };

  that.prepareVideo = (url) => {
    log.info('Preparing video', url);
    externalInput = new addon.ExternalInput(url);
    externalInput.setAudioReceiver(wrtc);
    externalInput.setVideoReceiver(wrtc);
  };

  that.prepareSynthetic = (spec) => {
    log.info('Preparing synthetic video', config);
    syntheticInput = new addon.SyntheticInput(threadPool,
      spec.audioBitrate,
      spec.minVideoBitrate,
      spec.maxVideoBitrate);
    syntheticInput.setAudioReceiver(wrtc);
    syntheticInput.setVideoReceiver(wrtc);
    syntheticInput.setFeedbackSource(wrtc);
  };

  that.prepareRecording = (url) => {
    log.info('Preparing Recording', url);
    externalOutput = new addon.ExternalOutput(url);
    wrtc.setVideoReceiver(externalOutput);
    wrtc.setAudioReceiver(externalOutput);
  };

  that.setRemoteDescription = () => {
    log.info('RemoteDescription');
  };

  that.processSignallingMessage = (signalingMsg) => {
    log.info('Receiving message', signalingMsg.type);
    if (signalingMsg.type === 'started') {
      initWebRtcConnection((mess, info) => {
        log.info('Message from wrtc', info.type);
        if (info.type === 'offer') {
          configuration.callback({ type: info.type, sdp: info.sdp });
        }
      }, {});

      const audioEnabled = true;
      const videoEnabled = true;
      const bundle = true;
      wrtc.createOffer(audioEnabled, videoEnabled, bundle);
    } else if (signalingMsg.type === 'answer') {
      setTimeout(() => {
        log.info('Passing delayed answer');
        wrtc.setRemoteSdp(signalingMsg.sdp);
        that.onaddstream({ stream: { active: true } });
      }, 10);
    }
  };

  that.getStats = () => {
    log.info('Requesting stats');
    return new Promise((resolve, reject) => {
      if (!wrtc) {
        reject('No connection present');
        return;
      }
      wrtc.getStats((statsString) => {
        resolve(JSON.parse(statsString));
      });
    });
  };

  that.close = () => {
    log.info('Closing nativeConnection');
    if (externalOutput) {
      externalOutput.close();
      if (externalOutput.interval) {
        clearInterval(externalOutput.interval);
      }
    }
    if (externalInput) {
      externalInput.close();
    }
    if (syntheticInput) {
      syntheticInput.close();
    }
    that.connected = false;
    wrtc.close();
    wrtc = undefined;
  };

  return that;
};
