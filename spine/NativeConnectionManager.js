const ConnectionHelpers = require('./NativeConnectionHelpers');
const ErizoNativeConnection = require('./nativeClient');
const { EventEmitter, ConnectionEvent } = require('./Events');
const logger = require('./logger').logger;

const log = logger.getLogger('NativeConnectionManager');

let sessionId = 0;

class NativeStack extends EventEmitter {
  constructor(config) {
    super();
    const configuration = Object.assign({}, config);
    configuration.label = configuration.label || `spine_${Math.floor(Math.random() * 1000)}`;
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
    this.peerConnection.onaddstream = (evt) => {
      this.emit(ConnectionEvent({ type: 'add-stream', stream: evt.stream }));
    };

    this.peerConnection.onremovestream = (evt) => {
      this.emit(ConnectionEvent({ type: 'remove-stream', stream: evt.stream }));
    };

    this.peerConnection.oniceconnectionstatechange = (state) => {
      this.emit(ConnectionEvent({ type: 'ice-state-change', state }));
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

  getOrBuildErizoConnection(specInput, erizoId = undefined, enableSinglePC = false) {
    log.debug(`getOrBuildErizoConnection, erizoId: ${erizoId}`);
    const configuration = Object.assign({}, specInput);
    configuration.sessionId = sessionId;
    sessionId += 1;
    return new NativeStack(specInput);
  }

  maybeCloseConnection(connection) {
    log.debug('Close connection');
    connection.close();
  }
}

exports.ErizoConnectionManager = ErizoConnectionManager;
