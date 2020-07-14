/* global require, exports */

/* eslint-disable no-param-reassign */

const NodeClass = require('./Node').Node;
const Subscriber = require('./Subscriber').Subscriber;
// eslint-disable-next-line import/no-unresolved
const addon = require('./../../../erizoAPI/build/Release/addon');
const logger = require('./../../common/logger').logger;
const Helpers = require('./Helpers');

// Logger
const log = logger.getLogger('Publisher');

const MIN_SLIDESHOW_PERIOD = 2000;
const MAX_SLIDESHOW_PERIOD = 10000;
const FALLBACK_SLIDESHOW_PERIOD = 3000;
const PLIS_TO_RECOVER = 3;
const WARN_NOT_FOUND = 404;

class Source extends NodeClass {
  constructor(clientId, streamId, threadPool, options = {}) {
    super(clientId, streamId, options);
    this.threadPool = threadPool;
    // {clientId1: Subscriber, clientId2: Subscriber}
    this.subscribers = {};
    this.externalOutputs = {};
    this.muteAudio = false;
    this.muteVideo = false;
    this.muxer = new addon.OneToManyProcessor();
  }

  get numSubscribers() {
    return Object.keys(this.subscribers).length;
  }

  forEachSubscriber(action) {
    const subscriberIds = Object.keys(this.subscribers);
    for (let i = 0; i < subscriberIds.length; i += 1) {
      action(subscriberIds[i], this.subscribers[subscriberIds[i]]);
    }
  }


  addSubscriber(clientId, connection, options) {
    log.info(`message: Adding subscriber, clientId: ${clientId}, streamId ${this.streamId},`,
      logger.objectToLog(this.options), logger.objectToLog(this.options.metadata));
    const subscriber = new Subscriber(clientId, this.streamId, connection, this, options);

    this.subscribers[clientId] = subscriber;
    this.muxer.addSubscriber(subscriber.mediaStream, subscriber.mediaStream.id);
    subscriber.mediaStream.minVideoBW = this.minVideoBW;

    subscriber._onSchemeSlideShowModeChangeListener =
      this._onSchemeSlideShowModeChange.bind(this, clientId);
    subscriber.on('scheme-slideshow-change', subscriber._onSchemeSlideShowModeChangeListener);

    log.debug(`message: Setting scheme from publisher to subscriber, clientId: ${clientId}, scheme: ${this.scheme},`,
      logger.objectToLog(this.options), logger.objectToLog(this.options.metadata));

    subscriber.mediaStream.scheme = this.scheme;
    const muteVideo = (options.muteStream && options.muteStream.video) || false;
    const muteAudio = (options.muteStream && options.muteStream.audio) || false;
    this.muteSubscriberStream(clientId, muteVideo, muteAudio);

    if (options.video) {
      this.setVideoConstraints(clientId,
        options.video.width, options.video.height, options.video.frameRate);
    }
    return subscriber;
  }

  removeSubscriber(clientId) {
    const subscriber = this.subscribers[clientId];
    if (subscriber === undefined) {
      log.warn(`message: subscriber to remove not found clientId: ${clientId}, ` +
        `streamId: ${this.streamId},`,
      logger.objectToLog(this.options), logger.objectToLog(this.options.metadata));
      return;
    }

    subscriber.removeListener('scheme-slideshow-change',
      subscriber._onSchemeSlideShowModeChangeListener);

    this.muxer.removeSubscriber(subscriber.mediaStream.id);
    delete this.subscribers[clientId];
    this.maybeStopSlideShow();
  }

  getSubscriber(clientId) {
    return this.subscribers[clientId];
  }

  hasSubscriber(clientId) {
    return this.subscribers[clientId] !== undefined;
  }

  addExternalOutput(url, options) {
    const eoId = `${url}_${this.streamId}`;
    log.info(`message: Adding ExternalOutput, id: ${eoId}, url: ${url},`,
      logger.objectToLog(this.options), logger.objectToLog(this.options.metadata));
    const externalOutput = new addon.ExternalOutput(this.threadPool, url,
      Helpers.getMediaConfiguration(options.mediaConfiguration));
    externalOutput.id = eoId;
    externalOutput.init();
    this.muxer.addExternalOutput(externalOutput, url);
    this.externalOutputs[url] = externalOutput;
  }

  removeExternalOutput(url) {
    log.info(`message: Removing ExternalOutput, url: ${url},`,
      logger.objectToLog(this.options), logger.objectToLog(this.options.metadata));
    return new Promise((resolve) => {
      this.muxer.removeSubscriber(url);
      this.externalOutputs[url].close(() => {
        log.info('message: ExternalOutput closed,',
          logger.objectToLog(this.options), logger.objectToLog(this.options.metadata));
        delete this.externalOutputs[url];
        resolve();
      });
    });
  }

  removeExternalOutputs() {
    const promises = [];
    Object.keys(this.externalOutputs).forEach((key) => {
      log.info(`message: Removing externalOutput, id ${key},`,
        logger.objectToLog(this.options), logger.objectToLog(this.options.metadata));
      promises.push(this.removeExternalOutput(key));
    });
    return Promise.all(promises);
  }

  hasExternalOutput(url) {
    return this.externalOutputs[url] !== undefined;
  }

  getExternalOutput(url) {
    return this.externalOutputs[url];
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

  _onSchemeSlideShowModeChange(message, clientId) {
    this.setSlideShow(message.enabled, clientId);
  }

  onStreamMessage(msg) {
    if (msg.type === 'updatestream') {
      if (msg.config) {
        if (msg.config.minVideoBW) {
          log.debug('message: updating minVideoBW for publisher,' +
                    `id: ${this.streamId}, ` +
                    `minVideoBW: ${msg.config.minVideoBW},`,
          logger.objectToLog(this.options), logger.objectToLog(this.options.metadata));
          this.minVideoBW = msg.config.minVideoBW;
          this.forEachSubscriber((clientId, subscriber) => {
            subscriber.minVideoBW = msg.config.minVideoBW * 1000; // bps
            subscriber.lowerThres = Math.floor(subscriber.minVideoBW * (1 - 0.2));
            subscriber.upperThres = Math.ceil(subscriber.minVideoBW * (1 + 0.1));
          });
        }
        if (msg.config.muteStream !== undefined) {
          this.muteStream(msg.config.muteStream);
        }
        if (msg.config.maxVideoBW) {
          this.mediaStream.setMaxVideoBW(msg.config.maxVideoBW);
        }
      }
    } else if (msg.type === 'control') {
      this.processControlMessage(undefined, msg.action);
    }
  }

  processControlMessage(clientId, action) {
    const publisherSide = clientId === undefined || action.publisherSide;
    switch (action.name) {
      case 'controlhandlers':
        if (action.enable) {
          this.enableHandlers(publisherSide ? undefined : clientId, action.handlers);
        } else {
          this.disableHandlers(publisherSide ? undefined : clientId, action.handlers);
        }
        break;
      default:
        log.error(`message: Unknwon processControlleMessage, action.name: ${action.name},`,
          logger.objectToLog(this.options), logger.objectToLog(this.options.metadata));
    }
  }

  requestVideoKeyFrame() {
    if (this.mediaStream) {
      this.mediaStream.generatePLIPacket();
    }
  }

  maybeStopSlideShow() {
    if (this.connection && this.mediaStream) {
      let shouldStopSlideShow = true;
      this.forEachSubscriber((id, subscriber) => {
        if (subscriber.mediaStream.slideShowMode === true ||
          subscriber.mediaStream.slideShowModeFallback === true) {
          shouldStopSlideShow = false;
        }
      });

      if (!shouldStopSlideShow) {
        return;
      }
      log.debug('message: clearing Pli interval as no more ' +
                'slideshows subscribers are present,',
      logger.objectToLog(this.options), logger.objectToLog(this.options.metadata));
      if (this.ei && this.mediaStream.periodicPlis) {
        clearInterval(this.mediaStream.periodicPlis);
        this.mediaStream.periodicPlis = undefined;
      } else {
        this.mediaStream.setPeriodicKeyframeRequests(false);
      }
    }
  }

  static _updateMediaStreamSubscriberSlideshow(subscriber, slideShowMode, isFallback) {
    if (isFallback) {
      subscriber.mediaStream.slideShowModeFallback = slideShowMode;
    } else {
      subscriber.mediaStream.slideShowMode = slideShowMode;
    }
    const globalSlideShowStatus = subscriber.mediaStream.slideShowModeFallback ||
      subscriber.mediaStream.slideShowMode;

    subscriber.mediaStream.setSlideShowMode(globalSlideShowStatus);
    return globalSlideShowStatus;
  }

  setSlideShow(slideShowMode, clientId, isFallback = false) {
    if (!this.mediaStream) {
      return;
    }
    const subscriber = this.getSubscriber(clientId);
    if (!subscriber) {
      log.warn('message: subscriber not found for updating slideshow, ' +
        `code: ${WARN_NOT_FOUND}, id: ${clientId}_${this.streamId},`,
      logger.objectToLog(this.options), logger.objectToLog(this.options.metadata));
      return;
    }

    log.info(`message: setting SlideShow, id: ${subscriber.clientId}, ` +
      `slideShowMode: ${slideShowMode} isFallback: ${isFallback},`,
    logger.objectToLog(this.options), logger.objectToLog(this.options.metadata));
    let period = slideShowMode === true ? MIN_SLIDESHOW_PERIOD : slideShowMode;
    if (isFallback) {
      period = slideShowMode === true ? FALLBACK_SLIDESHOW_PERIOD : slideShowMode;
    }
    if (Number.isSafeInteger(period)) {
      period = period < MIN_SLIDESHOW_PERIOD ? MIN_SLIDESHOW_PERIOD : period;
      period = period > MAX_SLIDESHOW_PERIOD ? MAX_SLIDESHOW_PERIOD : period;
      Source._updateMediaStreamSubscriberSlideshow(subscriber, true, isFallback);
      if (this.ei) {
        if (this.mediaStream.periodicPlis) {
          clearInterval(this.mediaStream.periodicPlis);
          this.mediaStream.periodicPlis = undefined;
        }
        this.mediaStream.periodicPlis = setInterval(() => {
          this.ei.generatePLIPacket();
        }, period);
      } else {
        this.mediaStream.setPeriodicKeyframeRequests(true, period);
      }
    } else {
      const result = Source._updateMediaStreamSubscriberSlideshow(subscriber, false, isFallback);
      if (!result) {
        for (let pliIndex = 0; pliIndex < PLIS_TO_RECOVER; pliIndex += 1) {
          if (this.ei) {
            this.ei.generatePLIPacket();
          }
        }
      }
      this.maybeStopSlideShow();
    }
  }

  muteStream(muteStreamInfo, clientId) {
    if (muteStreamInfo.video === undefined) {
      muteStreamInfo.video = false;
    }
    if (muteStreamInfo.audio === undefined) {
      muteStreamInfo.audio = false;
    }
    if (clientId && this.hasSubscriber(clientId)) {
      this.muteSubscriberStream(clientId, muteStreamInfo.video, muteStreamInfo.audio);
    } else {
      this.muteVideo = muteStreamInfo.video;
      this.muteAudio = muteStreamInfo.audio;
      this.forEachSubscriber((id, subscriber) => {
        this.muteSubscriberStream(id, subscriber.muteVideo, subscriber.muteAudio);
      });
    }
  }

  setQualityLayer(qualityLayer, clientId) {
    const subscriber = this.getSubscriber(clientId);
    if (!subscriber) {
      return;
    }
    log.info('message: setQualityLayer, spatialLayer: ', qualityLayer.spatialLayer,
      ', temporalLayer: ', qualityLayer.temporalLayer, ',',
      logger.objectToLog(subscriber.options), logger.objectToLog(subscriber.options.metadata));
    subscriber.mediaStream.setQualityLayer(qualityLayer.spatialLayer, qualityLayer.temporalLayer);
  }

  enableSlideShowBelowSpatialLayer(qualityLayer, clientId) {
    const subscriber = this.getSubscriber(clientId);
    if (!subscriber) {
      return;
    }
    log.info('message: setMinSpatialLayer, enabled: ', qualityLayer.enabled,
      ', spatialLayer: ', qualityLayer.spatialLayer, ',',
      logger.objectToLog(this.options), logger.objectToLog(this.options.metadata));
    subscriber.mediaStream.enableSlideShowBelowSpatialLayer(qualityLayer.enabled,
      qualityLayer.spatialLayer);
  }

  muteSubscriberStream(clientId, muteVideo, muteAudio) {
    const subscriber = this.getSubscriber(clientId);
    subscriber.muteVideo = muteVideo;
    subscriber.muteAudio = muteAudio;
    log.info('message: Mute Subscriber Stream, video: ', this.muteVideo || muteVideo,
      ', audio: ', this.muteAudio || muteAudio, ',',
      logger.objectToLog(subscriber.options), logger.objectToLog(subscriber.options.metadata));
    subscriber.mediaStream.muteStream(this.muteVideo || muteVideo,
      this.muteAudio || muteAudio);
  }

  setVideoConstraints(video, clientId) {
    const subscriber = this.getSubscriber(clientId);
    if (!subscriber) {
      return;
    }
    const width = video.width;
    const height = video.height;
    const frameRate = video.frameRate;
    const maxWidth = (width && width.max !== undefined) ? width.max : -1;
    const maxHeight = (height && height.max !== undefined) ? height.max : -1;
    const maxFrameRate = (frameRate && frameRate.max !== undefined) ? frameRate.max : -1;
    subscriber.mediaStream.setVideoConstraints(maxWidth, maxHeight, maxFrameRate);
  }

  enableHandlers(clientId, handlers) {
    let mediaStream = this.mediaStream;
    if (!handlers || !mediaStream) {
      return;
    }
    if (clientId) {
      mediaStream = this.getSubscriber(clientId).mediaStream;
    }
    if (mediaStream) {
      handlers.forEach((handler) => {
        mediaStream.enableHandler(handler);
      });
    }
  }

  disableHandlers(clientId, handlers) {
    let mediaStream = this.mediaStream;
    if (!handlers || !mediaStream) {
      return;
    }
    if (clientId) {
      mediaStream = this.getSubscriber(clientId).mediaStream;
    }
    if (mediaStream) {
      handlers.forEach((handler) => {
        mediaStream.disableHandler(handler);
      });
    }
  }

  // eslint-disable-next-line class-methods-use-this
  close() {
    return Promise.resolve();
  }
}

class Publisher extends Source {
  constructor(clientId, streamId, connection, options) {
    super(clientId, streamId, connection.threadPool, options);
    this.mediaConfiguration = options.mediaConfiguration;
    this.options = options;
    this.connection = connection;

    this.connection.mediaConfiguration = options.mediaConfiguration;
    this.promise = this.connection.addMediaStream(streamId, options, true, false);
    this.mediaStream = this.connection.getMediaStream(streamId);

    this.minVideoBW = options.minVideoBW;
    this.scheme = options.scheme;

    if (options.maxVideoBW) {
      this.mediaStream.setMaxVideoBW(options.maxVideoBW);
    }

    this.mediaStream.setAudioReceiver(this.muxer);
    this.mediaStream.setVideoReceiver(this.muxer);
    this.muxer.setPublisher(this.mediaStream);
    const muteVideo = (options.muteStream && options.muteStream.video) || false;
    const muteAudio = (options.muteStream && options.muteStream.audio) || false;
    this.muteStream({ video: muteVideo, audio: muteAudio });
  }

  close() {
    const removeMediaStreamPromise = this.connection.removeMediaStream(this.mediaStream.id);
    if (this.mediaStream.monitorInterval) {
      clearInterval(this.mediaStream.monitorInterval);
    }
    if (this.mediaStream.periodicPlis !== undefined) {
      log.debug(`message: clearing periodic PLIs for publisher, id: ${this.streamId},`,
        logger.objectToLog(this.options), logger.objectToLog(this.options.metadata));
      clearInterval(this.mediaStream.periodicPlis);
      this.mediaStream.periodicPlis = undefined;
    }
    return removeMediaStreamPromise;
  }
}

class ExternalInput extends Source {
  constructor(url, streamId, label, threadPool) {
    super(url, streamId, threadPool);
    const eiId = `${streamId}_${url}`;

    log.warn(`message: Adding ExternalInput, id: ${eiId}, url: ${url}`);

    const ei = new addon.ExternalInput(url);

    this.ei = ei;
    ei.id = streamId;

    this.subscribers = {};
    this.externalOutputs = {};
    this.mediaStream = {};
    this.connection = ei;
    this.label = label;

    ei.setAudioReceiver(this.muxer);
    ei.setVideoReceiver(this.muxer);
    this.muxer.setExternalPublisher(ei);
  }

  init() {
    return this.ei.init();
  }
  // eslint-disable-next-line class-methods-use-this
  close() {
    return Promise.resolve();
  }
}

exports.Publisher = Publisher;
exports.ExternalInput = ExternalInput;
