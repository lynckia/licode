const ConnectionHelpers = require('./NativeConnectionHelpers');
const ErizoNativeConnection = require('./nativeClient');
const logger = require('./logger').logger;

const log = logger.getLogger('NativeConnectionManager');

let sessionId = 0;

class NativeStack {
  constructor(config) {
    const configuration = Object.assign({}, config);
    log.info('Creating a NativeStack', configuration);


    this.pcConfig = {
      iceServers: [],
    };

    if (config.iceServers !== undefined) {
      this.pcConfig.iceServers = configuration.iceServers;
    }

    configuration.audio = configuration.audio || false;
    configuration.video = configuration.video || false;

    this.peerConnection = ErizoNativeConnection.ErizoNativeConnection(configuration);
    this.peerConnection.onaddstream = (stream) => {
      if (this.onaddstream) {
        this.onaddstream(stream);
      }
    };

    this.desc = {};
    this.callback = undefined;
  }

  close() {
    if (this.peerConnection) {
      this.peerConnection.close();
    } else {
      log.error('Trying to close with no underlying PC!');
    }
  }

  stop() {
    this.close();
  }

  createOffer() {
    log.info('CreateOffer');
  }

  addStream() {
    log.info('addStream');
  }

  removeStream(stream) {
    log.info('removeStream');
  }

  processSignalingMessage(msg) {
    log.info('processSignaling', msg.type);
    this.peerConnection.processSignallingMessage(msg);
  }

  sendSignalingMessage() {
    log.info('Sending signaling Message');
  }
}

class ErizoConnectionManager {
  constructor() {}

  getOrBuildErizoConnection(specInput, erizoId = undefined, enableSinglePC = false){
    log.debug(`getOrBuildErizoConnection, erizoId: ${erizoId}`);
    const configuration = Object.assign({}, specInput);
    configuration.sessionId = sessionId;
    sessionId += 1;
    return new NativeStack(specInput); // jshint ignore:line
  }

  closeConnection(connection) {
    log.debug('Close connection');
    connection.close();
  }
}

exports.ErizoConnectionManager = ErizoConnectionManager;
