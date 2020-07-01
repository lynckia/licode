/* global require, exports */


const NodeClass = require('./Node').Node;
const logger = require('./../../common/logger').logger;

// Logger
const log = logger.getLogger('Subscriber');

class Subscriber extends NodeClass {
  constructor(clientId, streamId, connection, publisher, options) {
    super(clientId, streamId, options);
    this.connection = connection;
    this.connection.mediaConfiguration = options.mediaConfiguration;
    this.promise = this.connection.addMediaStream(this.erizoStreamId, options, false,
      options.offerFromErizo);
    this._mediaStreamListener = this._onMediaStreamEvent.bind(this);
    connection.on('media_stream_event', this._mediaStreamListener);
    connection.onReady.then(() => {
      if (this.clientId && this.options.browser === 'bowser') {
        this.publisher.requestVideoKeyFrame();
      }
      if (this.options.slideShowMode === true ||
          Number.isSafeInteger(this.options.slideShowMode)) {
        this.publisher.setSlideShow(this.options.slideShowMode, this.clientId);
      }
    });
    this.mediaStream = connection.getMediaStream(this.erizoStreamId);
    this.publisher = publisher;
  }

  copySdpInfoFromPublisher() {
    if (this.publisher && this.publisher.connection && this.publisher.connection.wrtc &&
      this.publisher.connection.wrtc.localDescription && this.connection && this.connection.wrtc) {
      const publisherSdp = this.publisher.connection.wrtc.localDescription.connectionDescription;
      this.connection.wrtc.copySdpToLocalDescription(publisherSdp);
    }
  }

  _onMediaStreamEvent(mediaStreamEvent) {
    if (mediaStreamEvent.mediaStreamId !== this.erizoStreamId) {
      return;
    }
    if (mediaStreamEvent.type === 'slideshow_fallback_update') {
      this.publisher.setSlideShow(mediaStreamEvent.message !==
        'false', this.clientId, true);
    } else if (mediaStreamEvent.type === 'ready') {
      this.disableDefaultHandlers();
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

  onStreamMessage(msg) {
    if (msg.type === 'updatestream') {
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

  close(sendOffer = true) {
    log.debug(`message: Closing subscriber, streamId:${this.streamId}, `,
      logger.objectToLog(this.options), logger.objectToLog(this.options.metadata));
    this.publisher = undefined;
    let promise = Promise.resolve();
    if (this.connection) {
      promise = this.connection.removeMediaStream(this.mediaStream.id, sendOffer);
      this.connection.removeListener('media_stream_event', this._mediaStreamListener);
    }
    if (this.mediaStream && this.mediaStream.monitorInterval) {
      clearInterval(this.mediaStream.monitorInterval);
    }
    return promise;
  }
}

exports.Subscriber = Subscriber;
