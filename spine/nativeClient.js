
const addon = require('./../erizoAPI/build/Release/addon'); // eslint-disable-line import/no-unresolved
const licodeConfig = require('./../licode_config');
const mediaConfig = require('./../rtp_media_config');
const logger = require('./logger').logger;
const SessionDescription = require('./../erizo_controller/erizoJS/models/SessionDescription');
const SemanticSdp = require('./../erizo_controller/common/semanticSdp/SemanticSdp');


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
  let mediaStream;
  let streamId;
  let syntheticInput;
  let externalInput;
  let oneToMany;
  let externalOutput;
  let firstOfferSent = false;
  const configuration = Object.assign({}, config);

  that.connected = false;

  const CONN_INITIAL = 101;
  // CONN_STARTED = 102
  const CONN_GATHERED = 103;
  const CONN_READY = 104;
  // CONN_FINISHED = 105,
  const CONN_CANDIDATE = 201;
  const CONN_SDP = 202;
  const CONN_SDP_PROCESSED  = 203;
  const CONN_FAILED = 500;

  const generatePLIs = () => {
    externalOutput.interval = setInterval(() => {
      mediaStream.generatePLIPacket();
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
        case CONN_SDP_PROCESSED:
          setTimeout(() => {
            wrtc.localDescription = new SessionDescription(wrtc.getLocalDescription());
            const sdp = wrtc.localDescription.getSdp();
            initConnectionCallback('callback', { type: 'offer', sdp: sdp.toString() });
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
  global.config.erizo.useConnectionQualityCheck,
  global.config.erizo.turnserver,
  global.config.erizo.turnport,
  global.config.erizo.turnusername,
  global.config.erizo.turnpass,
  global.config.erizo.networkinterface);

  streamId = `spine_${Math.floor(Math.random() * 1000)}`;

  mediaStream = new addon.MediaStream(threadPool, wrtc,
      streamId, config.label,
      JSON.stringify(global.mediaConfig));

  wrtc.addMediaStream(mediaStream);

  that.createOffer = () => {
  };

  that.prepareNull = () => {
    log.info('Preparing null output');
    oneToMany = new addon.OneToManyProcessor();
    mediaStream.setVideoReceiver(oneToMany);
    mediaStream.setAudioReceiver(oneToMany);
    oneToMany.setPublisher(mediaStream);
  };

  that.prepareVideo = (url) => {
    log.info('Preparing video', url);
    externalInput = new addon.ExternalInput(url);
    externalInput.setAudioReceiver(mediaStream);
    externalInput.setVideoReceiver(mediaStream);
  };

  that.prepareSynthetic = (spec) => {
    log.info('Preparing synthetic video', config);
    syntheticInput = new addon.SyntheticInput(threadPool,
      spec.audioBitrate,
      spec.minVideoBitrate,
      spec.maxVideoBitrate);
    syntheticInput.setAudioReceiver(mediaStream);
    syntheticInput.setVideoReceiver(mediaStream);
    syntheticInput.setFeedbackSource(mediaStream);
  };

  that.prepareRecording = (url) => {
    log.info('Preparing Recording', url);
    externalOutput = new addon.ExternalOutput(url, JSON.stringify(global.mediaConfig));
    mediaStream.setVideoReceiver(externalOutput);
    mediaStream.setAudioReceiver(externalOutput);
  };

  that.setRemoteDescription = () => {
    log.info('RemoteDescription');
  };

  that.processSignallingMessage = (signalingMsg) => {
    log.info('Receiving message', signalingMsg.type);
    if (signalingMsg.type === 'started') {
      initWebRtcConnection((mess, info) => {
        log.info('Message from wrtc', info.type);
        if (info.type === 'offer' && !firstOfferSent) {
          firstOfferSent = true;
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
        const sdp = SemanticSdp.SDPInfo.processString(signalingMsg.sdp);
        this.remoteDescription = new SessionDescription(sdp, 'default');
        wrtc.setRemoteDescription(this.remoteDescription.connectionDescription, streamId);
        sdp.streams.forEach((stream) => {
          let label = '';
          for (const track of stream.tracks.values()) {
            track.ssrcs.forEach(ssrc => {
              label = ssrc.mslabel;
            });
          }
          that.onaddstream({ stream: { active: true, id: label } });
        });
      }, 10);
    }
  };

  that.getStats = () => {
    log.info('Requesting stats');
    return new Promise((resolve, reject) => {
      if (!mediaStream) {
        reject('No stream present');
        return;
      }
      mediaStream.getStats((statsString) => {
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
    mediaStream.close();
    wrtc = undefined;
    mediaStream = undefined;
  };

  return that;
};
