const ConnectionHelpers = require('./NativeConnectionHelpers');
const ErizoNativeConnection = require('./nativeClient');
const logger = require('./logger').logger;

const log = logger.getLogger('NativeConnectionManager');

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
    log.info('CreateOffer');
  };

  that.addStream = () => {
    log.info('addStream');
  };

  that.removeStream = (stream) => {
    log.info('removeStream');
  };

  that.processSignalingMessage = (msg) => {
    log.info('processSignaling', msg.type);
    that.peerConnection.processSignallingMessage(msg);
  };

  that.sendSignalingMessage = () => {
    log.info('Sending signaling Message');
  };

  that.peerConnection.onaddstream = (stream) => {
    if (that.onaddstream) {
      that.onaddstream(stream);
    }
  };

  return that;
};

exports.ConnectionManager = () => {
  const that = {};
  
  that.getOrBuildErizoConnection = (specInput, erizoId = undefined, enableSinglePC = false) => {
    log.debug(`getOrBuildErizoConnection, erizoId: ${erizoId}`);
    const configuration = Object.assign({}, specInput);
    configuration.sessionId = sessionId;
    sessionId += 1;
    return NativeStack(specInput); // jshint ignore:line
  };

  that.closeConnection = (connection) => {
    log.debug('Close connection');
    connection.close();
  };

  return that;
};



