const ErizoNativeConnection = require('./nativeClient');
const logger = require('./logger').logger;

const log = logger.getLogger('NativeStack');

let sessionId = 0;

const NativeStack = (config) => {
  const that = {};
  const configuration = Object.assign({}, config);
  log.info('Creating a NativeStack', configuration);

  that.pcConfig = {
    iceServers: [],
  };

  if (config.iceServers !== undefined) {
    that.pcConfig.iceServers = configuration.iceServers;
  }

  configuration.audio = configuration.audio || false;
  configuration.video = configuration.video || false;

  that.peerConnection = ErizoNativeConnection.ErizoNativeConnection(configuration);
  that.desc = {};
  that.callback = undefined;

  that.close = () => {
    log.info('Close NATIVE');
    if (that.peerConnection) {
      that.peerConnection.close();
    } else {
      log.error('Trying to close with no underlying PC!');
    }
  };

  that.stop = () => {
    that.close();
  };

  that.createOffer = () => {
    log.info('NATIVESTACK: CreateOffer');
  };

  that.addStream = () => {
    log.info('NATIVESTACK: addStream');
  };

  that.processSignalingMessage = (msg) => {
    log.info('NATIVESTACK: processSignaling', msg.type);
    that.peerConnection.processSignallingMessage(msg);
  };

  that.sendSignalingMessage = () => {
    log.info('NATIVESTACK: Sending signaling Message');
  };

  that.peerConnection.onaddstream = (stream) => {
    if (that.onaddstream) {
      that.onaddstream(stream);
    }
  };

  return that;
};
exports.buildConnection = (config) => {
  log.info('Creating Connection');
  const configuration = Object.assign({}, config);
  configuration.sessionId = sessionId;
  sessionId += 1;
  return NativeStack(configuration); // jshint ignore:line
};

exports.GetUserMedia = (opt, callback) => {
  log.info('Fake getUserMedia to use with files', opt);
  // if (that.peerConnection && opt.video.file){
  //     that.peerConnection.prepareVideo(opt.video.file);
  // }
  callback('');
};

exports.getBrowser = () => 'fake';
