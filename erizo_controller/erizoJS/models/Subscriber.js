/* global require, exports */


const NodeClass = require('./Node').Node;
const logger = require('./../../common/logger').logger;
const SemanticSdp = require('./../../common/semanticSdp/SemanticSdp');

// Logger
const log = logger.getLogger('Subscriber');

class Subscriber extends NodeClass {
  constructor(clientId, streamId, connection, publisher, options) {
    super(clientId, streamId, options);
    this.connection = connection;
    this.connection.mediaConfiguration = options.mediaConfiguration;
    this.promise = this.connection.addMediaStream(this.erizoStreamId, options, false);
    this._connectionListener = this._emitStatusEvent.bind(this);
    this._mediaStreamListener = this._onMediaStreamEvent.bind(this);
    connection.on('status_event', this._connectionListener);
    connection.on('media_stream_event', this._mediaStreamListener);
    this.mediaStream = connection.getMediaStream(this.erizoStreamId);
    this.publisher = publisher;
    this.ready = false;
    this.connectionReady = connection.ready;
  }

  _emitStatusEvent(evt, status, streamId) {
    const isGlobalStatus = streamId === undefined || streamId === '';
    const isNotMe = !isGlobalStatus && (`${streamId}`) !== (`${this.erizoStreamId}`);
    if (isNotMe) {
      log.debug('onStatusEvent dropped in publisher', streamId, this.erizoStreamId);
      return;
    }
    if (evt.type === 'ready') {
      if (this.connectionReady) {
        return;
      }
      this.connectionReady = true;
      if (!(this.ready && this.connectionReady)) {
        log.debug('ready event dropped in publisher', this.ready, this.connectionReady);
        return;
      }
    }

    if (evt.type === 'answer' || evt.type === 'offer') {
      if (!this.ready && this.connectionReady) {
        const readyEvent = { type: 'ready' };
        this._onConnectionStatusEvent(readyEvent);
        this.emit('status_event', readyEvent);
      }
      this.ready = true;
    }
    this._onConnectionStatusEvent(evt);
    this.emit('status_event', evt, status);
  }

  copySdpInfoFromPublisher() {
    if (this.publisher && this.publisher.connection && this.publisher.connection.wrtc &&
      this.publisher.connection.wrtc.localDescription && this.connection && this.connection.wrtc) {
      const publisherSdp = this.publisher.connection.wrtc.localDescription.connectionDescription;
      this.connection.wrtc.copySdpToLocalDescription(publisherSdp);
    }
  }

  _onConnectionStatusEvent(connectionEvent) {
    if (connectionEvent.type === 'ready') {
      if (this.clientId && this.options.browser === 'bowser') {
        this.publisher.requestVideoKeyFrame();
      }
      if (this.options.slideShowMode === true ||
          Number.isSafeInteger(this.options.slideShowMode)) {
        this.publisher.setSlideShow(this.options.slideShowMode, this.clientId);
      }
    }
  }

  _onMediaStreamEvent(mediaStreamEvent) {
    if (mediaStreamEvent.type === 'slideshow_fallback_update') {
      this.publisher.setSlideShow(mediaStreamEvent.message !==
        'false', this.clientId, true);
    }
  }

  disableDefaultHandlers() {
    const disabledHandlers = global.config.erizo.disabledHandlers;
    if (!disabledHandlers || !this.mediaStream) {
      return;
    }
    disabledHandlers.forEach((handler) => {
      this.mediaStream.disableHandler(handler);
    });
  }

  onSignalingMessage(msg, streamIds) {
    const connection = this.connection;

    if (msg.type === 'offer') {
      connection.processOffer(msg.sdp, [this.erizoStreamId]);
      if (msg.config && msg.config.maxVideoBW) {
        this.mediaStream.setMaxVideoBW(msg.config.maxVideoBW);
      }
      this.disableDefaultHandlers();
    } else if (msg.type === 'answer') {
      connection.processAnswer(msg.sdp, [this.erizoStreamId]);
      if (msg.config && msg.config.maxVideoBW) {
        this.mediaStream.setMaxVideoBW(msg.config.maxVideoBW);
      }
      this.disableDefaultHandlers();
    } else if (msg.type === 'candidate') {
      connection.addRemoteCandidate(msg.candidate);
    } else if (msg.type === 'updatestream') {
      if (msg.sdp) {
        const sdp = SemanticSdp.SDPInfo.processString(msg.sdp);
        connection.setRemoteDescription(sdp, [this.erizoStreamId]);
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
        if (msg.config.slideShowBelowLayer !== undefined) {
          this.publisher.enableSlideShowBelowSpatialLayer(
            msg.config.slideShowBelowLayer, this.clientId);
        }
        if (msg.config.video !== undefined) {
          this.publisher.setVideoConstraints(msg.config.video, this.clientId);
        }
        if (msg.config.maxVideoBW) {
          this.mediaStream.setMaxVideoBW(msg.config.maxVideoBW);
        }
      }
    } else if (msg.type === 'control') {
      this.publisher.processControlMessage(this.clientId, msg.action);
    }
  }

  close() {
    log.debug(`msg: Closing subscriber, streamId:${this.streamId}`);
    this.publisher = undefined;
    let promise = Promise.resolve();
    if (this.connection) {
      promise = this.connection.removeMediaStream(this.mediaStream.id);
      this.connection.removeListener('status_event', this._connectionListener);
      this.connection.removeListener('media_stream_event', this._mediaStreamListener);
    }
    if (this.mediaStream && this.mediaStream.monitorInterval) {
      clearInterval(this.mediaStream.monitorInterval);
    }
    return promise;
  }
}

exports.Subscriber = Subscriber;
