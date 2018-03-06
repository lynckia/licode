/*global require, exports*/
'use strict';
const NodeClass = require('./Node').Node;
const logger = require('./../../common/logger').logger;
var SemanticSdp = require('./../../common/semanticSdp/SemanticSdp');

// Logger
const log = logger.getLogger('Subscriber');

class Subscriber extends NodeClass {
  constructor(clientId, streamId, connection, publisher, options) {
    super(clientId, streamId, options);
    this.connection = connection;
    this.connection.mediaConfiguration = options.mediaConfiguration;
    this.connection.addMediaStream(this.erizoStreamId, options.metadata);
    this.mediaStream = connection.getMediaStream(this.erizoStreamId);
    this.publisher = publisher;
    this._listenToConnectionStuff();
  }

  _listenToConnectionStuff() {
    this._statusListener = this._onConnectionStatusEvent.bind(this);
    this.connection.on('status_event', this._statusListener);
  }

  _onConnectionStatusEvent(connectionEvent) {
    if (connectionEvent.type === 'ready') {
      if (this.clientId && this.options.browser === 'bowser') {
          this.publisher.generateVideoKeyFrame();
      }
      if (this.options.slideShowMode === true || 
          Number.isSafeInteger(this.options.slideShowMode)) {
        this.publisher.setSlideShow(this.options.slideShowMode, this.clientId);
      }
    }
  }

  disableDefaultHandlers() {
    const disabledHandlers = global.config.erizo.disabledHandlers;
    for (const index in disabledHandlers) {
      this.mediaStream.disableHandler(disabledHandlers[index]);
    }
  }

  onSignalingMessage(msg, publisher) {
    let connection = this.connection;

    if (msg.type === 'offer') {
      const sdp = SemanticSdp.SDPInfo.processString(msg.sdp);
      connection.setRemoteDescription(sdp);
      this.disableDefaultHandlers();
    } else if (msg.type === 'candidate') {
      connection.addRemoteCandidate(msg.candidate);
    } else if (msg.type === 'updatestream') {
      if (msg.sdp) {
        const sdp = SemanticSdp.SDPInfo.processString(msg.sdp);
        connection.setRemoteDescription(sdp);
      }
      if (msg.config) {
        if (msg.config.slideShowMode !== undefined) {
          this.publisher.setSlideShow(msg.config.slideShowMode, this.clientId);
        }
        if (msg.config.muteStream !== undefined) {
          this.publisher.muteStream(msg.config.muteStream, this.clientId);
        }
        if (msg.config.qualityLayer !== undefined) {
          this.publisher.setQualityLayer(msg.config.qualityLayer, this.clientId);
        }
        if (msg.config.video !== undefined) {
          this.publisher.setVideoConstraints(msg.config.video, this.clientId);
        }
      }
    } else if (msg.type === 'control') {
      publisher.processControlMessage(this.clientId, msg.action);
    }
  }

  close() {
    log.debug(`msg: Closing subscriber, streamId:${this.streamId}`);
    this.publisher = undefined;
    if (this.connection) {
      this.connection.removeListener('status_event', this._statusListener);
      this.connection.removeMediaStream(this.mediaStream.id);
    }
    if (this.mediaStream && this.mediaStream.monitorInterval) {
      clearInterval(this.mediaStream.monitorInterval);
    }
  }
}

exports.Subscriber = Subscriber;
