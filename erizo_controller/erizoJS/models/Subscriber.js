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
    this.promise = this.connection.addStream(this.erizoStreamId, options, false);
    this.onReady = new Promise((resolve, reject) => {
      this._readyResolveFunction = resolve;
      this._readyRejectFunction = reject;
    });
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
    this.mediaStream = connection.getStream(this.erizoStreamId);
    this.publisher = publisher;
    this.setMaxVideoBW();
  }

  copySdpInfoFromPublisher() {
    this.connection.copySdpInfoFromConnection(this.publisher.connection);
  }

  updatePublisherMaxVideoBW() {
    this.setMaxVideoBW();
  }

  setMaxVideoBW(maxVideoBW) {
    let updatedMaxVideoBW;
    if (maxVideoBW) {
      this.maxVideoBW = maxVideoBW;
    }
    if (this.maxVideoBW) {
      updatedMaxVideoBW = this.publisher.getMaxVideoBW() ?
        Math.min(this.publisher.getMaxVideoBW(), this.maxVideoBW) : this.maxVideoBW;
      this.maxVideoBW = updatedMaxVideoBW;
    } else {
      updatedMaxVideoBW = this.publisher.getMaxVideoBW();
    }
    log.debug(`Setting maxVideoBW in subscriber, requested: ${maxVideoBW}, publisher ${this.publisher.getMaxVideoBW()}, result:${updatedMaxVideoBW}`);
    this.mediaStream.setMaxVideoBW(updatedMaxVideoBW);
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
      this._readyResolveFunction();
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
          this.setMaxVideoBW(msg.config.maxVideoBW);
        }
        if (msg.config.priorityLevel !== undefined) {
          this.mediaStream.setPriority(msg.config.priorityLevel);
        }
      }
    } else if (msg.type === 'control') {
      this.publisher.processControlMessage(this.clientId, msg.action);
    }
  }

  getDurationDistribution() {
    if (!this.mediaStream) {
      return [];
    }
    return this.mediaStream.getDurationDistribution();
  }

  getDelayDistribution() {
    if (!this.mediaStream) {
      return [];
    }
    return this.mediaStream.getDelayDistribution();
  }

  resetStats() {
    if (!this.mediaStream) {
      return;
    }
    this.mediaStream.resetStats();
  }

  close(requestId = undefined) {
    log.debug(`message: Closing subscriber, clientId: ${this.clientId}, streamId: ${this.streamId}, `,
      logger.objectToLog(this.options), logger.objectToLog(this.options.metadata));
    this.publisher = undefined;
    let promise = Promise.resolve();
    if (this.connection) {
      log.debug(`message: Removing Media Stream, clientId: ${this.clientId}, streamId: ${this.streamId}`);
      promise = this.connection.removeStream(this.mediaStream.id, requestId);
      this.connection.removeListener('media_stream_event', this._mediaStreamListener);
    }
    if (this.mediaStream && this.mediaStream.monitorInterval) {
      clearInterval(this.mediaStream.monitorInterval);
    }
    return promise;
  }
}

exports.Subscriber = Subscriber;
