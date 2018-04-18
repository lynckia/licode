/*global require, exports*/
'use strict';
const NodeClass = require('./Node').Node;
const Subscriber = require('./Subscriber').Subscriber;
const addon = require('./../../../erizoAPI/build/Release/addon');
const logger = require('./../../common/logger').logger;
const Helpers = require('./Helpers');
var SemanticSdp = require('./../../common/semanticSdp/SemanticSdp');

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

  addSubscriber(clientId, connection, options) {
    log.info(`message: Adding subscriber, clientId: ${clientId}, ` +
             `${logger.objectToLog(options)}` +
              `, ${logger.objectToLog(options.metadata)}`);
    const subscriber = new Subscriber(clientId, this.streamId, connection, this, options);

    this.subscribers[clientId] = subscriber;
    this.muxer.addSubscriber(subscriber.mediaStream, subscriber.mediaStream.id);
    subscriber.mediaStream.minVideoBW = this.minVideoBW;

    subscriber._onSchemeSlideShowModeChangeListener =
      this._onSchemeSlideShowModeChange.bind(this, clientId);
    subscriber.on('scheme-slideshow-change', subscriber._onSchemeSlideShowModeChangeListener);

    log.debug(`message: Setting scheme from publisher to subscriber, ` +
              `clientId: ${clientId}, scheme: ${this.scheme}, `+
               ` ${logger.objectToLog(options.metadata)}`);

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
    let subscriber = this.subscribers[clientId];
    if (subscriber === undefined) {
      log.warn(`message: subscriber to remove not found clientId: ${clientId}, ` +
        `streamId: ${this.streamId}`);
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
    const eoId = url + '_' + this.streamId;
    log.info('message: Adding ExternalOutput, id: ' + eoId + ', url: ' + url);
    const externalOutput = new addon.ExternalOutput(this.threadPool, url,
      Helpers.getMediaConfiguration(options.mediaConfiguration));
    externalOutput.id = eoId;
    externalOutput.init();
    this.muxer.addExternalOutput(externalOutput, url);
    this.externalOutputs[url] = externalOutput;
  }

  removeExternalOutput(url) {
    log.info(`message: Removing ExternalOutput, url: ${url}`);
    return new Promise((resolve) => {
      this.muxer.removeSubscriber(url);
      this.externalOutputs[url].close(() => {
        log.info('message: ExternalOutput closed');
        delete this.externalOutputs[url];
        resolve();
      });
    });
  }

  removeExternalOutputs() {
    const promises = [];
    for (let externalOutputKey in this.externalOutputs) {
        log.info('message: Removing externalOutput, id ' + externalOutputKey);
        promises.push(this.removeExternalOutput(externalOutputKey));
    }
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
    if (!this.mediaStream) {
      return;
    }
    for (const index in disabledHandlers) {
      this.mediaStream.disableHandler(disabledHandlers[index]);
    }
  }

  _onSchemeSlideShowModeChange(message, clientId) {
    this.setSlideShow(message.enabled, clientId);
  }

  onSignalingMessage(msg) {
    const connection = this.connection;
    if (!connection) {
      return;
    }
    if (msg.type === 'offer') {
      const sdp = SemanticSdp.SDPInfo.processString(msg.sdp);
      connection.setRemoteDescription(sdp, this.streamId);
      if (msg.config && msg.config.maxVideoBW) {
        this.mediaStream.setMaxVideoBW(msg.config.maxVideoBW);
      }
      this.disableDefaultHandlers();
    } else if (msg.type === 'candidate') {
      connection.addRemoteCandidate(msg.candidate);
    } else if (msg.type === 'updatestream') {
      if (msg.sdp) {
        const sdp = SemanticSdp.SDPInfo.processString(msg.sdp);
        connection.setRemoteDescription(sdp, this.streamId);
        if (this.mediaStream) {
          this.mediaStream.setMaxVideoBW();
        }
      }
      if (msg.config) {
        if (msg.config.minVideoBW) {
          log.debug('message: updating minVideoBW for publisher, ' +
                    'id: ' + this.streamId + ', ' +
                    'minVideoBW: ' + msg.config.minVideoBW);
          this.minVideoBW = msg.config.minVideoBW;
          for (const clientId in this.subscribers) {
            const subscriber = this.getSubscriber(clientId);
            subscriber.minVideoBW = msg.config.minVideoBW * 1000; // bps
            subscriber.lowerThres = Math.floor(subscriber.minVideoBW * (1 - 0.2));
            subscriber.upperThres = Math.ceil(subscriber.minVideoBW * (1 + 0.1));
          }
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
    switch(action.name) {
      case 'controlhandlers':
        if (action.enable) {
          this.enableHandlers(publisherSide ? undefined : clientId, action.handlers);
        } else {
          this.disableHandlers(publisherSide ? undefined : clientId, action.handlers);
        }
        break;
    }
  }

  requestVideoKeyFrame() {
    if (this.mediaStream) {
      this.mediaStream.generatePLIPacket();
    }
  }

  maybeStopSlideShow() {
    if (this.connection && this.mediaStream && this.mediaStream.periodicPlis !== undefined) {
      for (const i in this.subscribers) {
        if (this.getSubscriber(i).mediaStream.slideShowMode === true ||
          this.getSubscriber(i).mediaStream.slideShowModeFallback === true) {
          return;
        }
      }
      log.debug('message: clearing Pli interval as no more ' +
                'slideshows subscribers are present');
      clearInterval(this.mediaStream.periodicPlis);
      this.mediaStream.periodicPlis = undefined;
    }
  }

  _updateMediaStreamSubscriberSlideshow(subscriber, slideShowMode, isFallback) {
    if (isFallback) {
      subscriber.mediaStream.slideShowModeFallback = slideShowMode;
    } else {
      subscriber.mediaStream.slideShowMode = slideShowMode;
    }
    let globalSlideShowStatus = subscriber.mediaStream.slideShowModeFallback ||
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
                 'code: ' + WARN_NOT_FOUND + ', id: ' + clientId + '_' + this.streamId);
        return;
    }

    log.warn('message: setting SlideShow, id: ' + subscriber.clientId +
              ', slideShowMode: ' + slideShowMode + ' isFallback: ' + isFallback);
    let period = slideShowMode === true ? MIN_SLIDESHOW_PERIOD : slideShowMode;
    if (isFallback) {
      period = slideShowMode === true? FALLBACK_SLIDESHOW_PERIOD : slideShowMode;
    } 
    if (Number.isSafeInteger(period)) {
      period = period < MIN_SLIDESHOW_PERIOD ? MIN_SLIDESHOW_PERIOD : period;
      period = period > MAX_SLIDESHOW_PERIOD ? MAX_SLIDESHOW_PERIOD : period;
      this._updateMediaStreamSubscriberSlideshow(subscriber, true, isFallback);
      if (this.mediaStream.periodicPlis) {
        clearInterval(this.mediaStream.periodicPlis);
        this.mediaStream.periodicPlis = undefined;
      }
      this.mediaStream.periodicPlis = setInterval(() => {
        this.mediaStream.generatePLIPacket();
      }, period);
    } else {
      let result = this._updateMediaStreamSubscriberSlideshow(subscriber, false, isFallback);
      if (!result) {
        for (let pliIndex = 0; pliIndex < PLIS_TO_RECOVER; pliIndex++) {
          this.mediaStream.generatePLIPacket();
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
      for (const subId in this.subscribers) {
        const sub = this.getSubscriber(subId);
        this.muteVideo = muteStreamInfo.video;
        this.muteAudio = muteStreamInfo.audio;
        this.muteSubscriberStream(subId, sub.muteVideo, sub.muteAudio);
      }
    }
  }

  setQualityLayer(qualityLayer, clientId) {
    const subscriber = this.getSubscriber(clientId);
    if (!subscriber) {
      return;
    }
    log.info('message: setQualityLayer, spatialLayer: ', qualityLayer.spatialLayer,
                                     ', temporalLayer: ', qualityLayer.temporalLayer);
    subscriber.mediaStream.setQualityLayer(qualityLayer.spatialLayer, qualityLayer.temporalLayer);
  }

  setMinSpatialLayer(qualityLayer, clientId) {
    const subscriber = this.getSubscriber(clientId);
    if (!subscriber) {
      return;
    }
    log.info('message: setMinSpatialLayer, spatialLayer: ', qualityLayer.spatialLayer);
    subscriber.mediaStream.setMinSpatialLayer(qualityLayer.spatialLayer);
  }

  muteSubscriberStream(clientId, muteVideo, muteAudio) {
    const subscriber = this.getSubscriber(clientId);
    subscriber.muteVideo = muteVideo;
    subscriber.muteAudio = muteAudio;
    log.info('message: Mute Stream, video: ', this.muteVideo || muteVideo,
                                 ', audio: ', this.muteAudio || muteAudio);
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
    if (!mediaStream) {
      return;
    }
    if (clientId) {
      mediaStream = this.getSubscriber(clientId).mediaStream;
    }
    if (mediaStream) {
      for (const index in handlers) {
        mediaStream.enableHandler(handlers[index]);
      }
    }
  }

  disableHandlers(clientId, handlers) {
    let mediaStream = this.mediaStream;
    if (!mediaStream) {
      return;
    }
    if (clientId) {
      mediaStream = this.getSubscriber(clientId).mediaStream;
    }
    if (mediaStream) {
      for (const index in handlers) {
        mediaStream.disableHandler(handlers[index]);
      }
    }
  }

  close() {
  }
}

class Publisher extends Source {
  constructor(clientId, streamId, connection, options) {
    super(clientId, streamId, connection.threadPool, options);
    this.mediaConfiguration = options.mediaConfiguration;
    this.options = options;
    this.connection = connection;

    this.connection.mediaConfiguration = options.mediaConfiguration;
    this.connection.addMediaStream(streamId, options, true);
    this._connectionListener = this._emitStatusEvent.bind(this);
    connection.on('status_event', this._connectionListener);
    this.mediaStream = this.connection.getMediaStream(streamId);

    this.minVideoBW = options.minVideoBW;
    this.scheme = options.scheme;
    this.ready = false;
    this.connectionReady = connection.ready;

    this.mediaStream.setAudioReceiver(this.muxer);
    this.mediaStream.setVideoReceiver(this.muxer);
    this.muxer.setPublisher(this.mediaStream);
    const muteVideo = (options.muteStream && options.muteStream.video) || false;
    const muteAudio = (options.muteStream && options.muteStream.audio) || false;
    this.muteStream({video: muteVideo, audio: muteAudio});
  }

  _emitStatusEvent(evt, status, streamId) {
    log.debug('onStatusEvent in publisher', evt.type, this.streamId, streamId);
    const isGlobalStatus = streamId === undefined || streamId === '';
    const isNotMe = !isGlobalStatus && (streamId + '') !== (this.streamId + '');
    if (isNotMe) {
      log.debug('onStatusEvent dropped in publisher', streamId, this.streamId);
      return;
    }
    if (evt.type === 'ready') {
      if (this.connectionReady) {
        return;
      }
      this.connectionReady = true;
      if (!(this.ready && this.connectionReady)) {
        log.debug('ready event dropped in publisher', this.ready, this.connectionReady);
        return;
      }
    }

    if (evt.type === 'answer' || evt.type === 'offer') {
      if (!this.ready && this.connectionReady) {
        this.emit('status_event', {type: 'ready'});
      }
      this.ready = true;
    }
    this.emit('status_event', evt, status);
  }

  close() {
    this.connection.removeMediaStream(this.mediaStream.id);
    this.connection.removeListener('status_event', this._connectionListener);
    if (this.mediaStream.monitorInterval) {
      clearInterval(this.mediaStream.monitorInterval);
    }
    if (this.mediaStream.periodicPlis !== undefined) {
        log.debug('message: clearing periodic PLIs for publisher, id: ' + this.streamId);
        clearInterval(this.mediaStream.periodicPlis);
        this.mediaStream.periodicPlis = undefined;
    }
  }
}

class ExternalInput extends Source {
  constructor(url, streamId, threadPool) {
    super(url, streamId, threadPool);
    const eiId = streamId + '_' + url;

    log.info('message: Adding ExternalInput, id: ' + eiId);

    const ei = new addon.ExternalInput(url);

    this.ei = ei;
    ei.id = streamId;

    this.subscribers = {};
    this.externalOutputs = {};
    this.mediaStream = {};
    this.connection = ei;

    ei.setAudioReceiver(this.muxer);
    ei.setVideoReceiver(this.muxer);
    this.muxer.setExternalPublisher(ei);
  }

  init() {
    return this.ei.init();
  }

  close() {
  }
}

exports.Publisher = Publisher;
exports.ExternalInput = ExternalInput;
