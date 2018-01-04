/*global require, exports*/
'use strict';
var addon = require('./../../../erizoAPI/build/Release/addon');
var logger = require('./../../common/logger').logger;
var Helpers = require('./Helpers');

// Logger
var log = logger.getLogger('Publisher');

function createWrtc(id, threadPool, ioThreadPool, mediaConfiguration) {
  var wrtc = new addon.WebRtcConnection(threadPool, ioThreadPool, id,
                                    global.config.erizo.stunserver,
                                    global.config.erizo.stunport,
                                    global.config.erizo.minport,
                                    global.config.erizo.maxport,
                                    false,
                                    Helpers.getMediaConfiguration(mediaConfiguration),
                                    global.config.erizo.useNicer,
                                    global.config.erizo.turnserver,
                                    global.config.erizo.turnport,
                                    global.config.erizo.turnusername,
                                    global.config.erizo.turnpass,
                                    global.config.erizo.networkinterface);
  return wrtc;
}

function createMediaStream (wrtc, id) {
  var mediaStream = new addon.MediaStream(wrtc, id);
  return mediaStream;
}

class Source {
  constructor(id, threadPool, ioThreadPool) {
    this.id = id;
    this.threadPool = threadPool;
    this.ioThreadPool = ioThreadPool;
    this.subscribers = {};
    this.externalOutputs = {};
    this.muteAudio = false;
    this.muteVideo = false;
    this.muxer = new addon.OneToManyProcessor();
  }

  get numSubscribers() {
    return Object.keys(this.subscribers).length;
  }

  addSubscriber(id, options) {
    var wrtcId = id + '_' + this.id;
    log.info('message: Adding subscriber, id: ' + wrtcId + ', ' +
             logger.objectToLog(options)+
              ', ' + logger.objectToLog(options.metadata));
    var wrtc = createWrtc(wrtcId, this.threadPool, this.ioThreadPool, options.mediaConfiguration);
    wrtc.wrtcId = wrtcId;
    wrtc.mediaConfiguration = options.mediaConfiguration;
    wrtc.mediaStream = createMediaStream(wrtc, id);
    wrtc.addMediaStream(wrtc.mediaStream);
    this.subscribers[id] = wrtc;
    this.muxer.addSubscriber(wrtc.mediaStream, id);
    wrtc.mediaStream.minVideoBW = this.minVideoBW;
    log.debug('message: Setting scheme from publisher to subscriber, ' +
              'id: ' + wrtcId + ', scheme: ' + this.scheme+
               ', ' + logger.objectToLog(options.metadata));
    wrtc.scheme = this.scheme;
    const muteVideo = (options.muteStream && options.muteStream.video) || false;
    const muteAudio = (options.muteStream && options.muteStream.audio) || false;
    this.muteSubscriberStream(id, muteVideo, muteAudio);
    if (options.video) {
      this.setVideoConstraints(id,
        options.video.width, options.video.height, options.video.frameRate);
    }
  }

  removeSubscriber(id) {
    this.muxer.removeSubscriber(id);
    delete this.subscribers[id];
  }

  getSubscriber(id) {
    return this.subscribers[id];
  }

  hasSubscriber(id) {
    return this.subscribers[id] !== undefined;
  }

  addExternalOutput(url, options) {
    var eoId = url + '_' + this.id;
    log.info('message: Adding ExternalOutput, id: ' + eoId);
    var externalOutput = new addon.ExternalOutput(url,
      Helpers.getMediaConfiguration(options.mediaConfiguration));
    externalOutput.wrtcId = eoId;
    externalOutput.init();
    this.muxer.addExternalOutput(externalOutput, url);
    this.externalOutputs[url] = externalOutput;
  }

  removeExternalOutput(url) {
    var self = this;
    this.muxer.removeSubscriber(url);
    this.externalOutputs[url].close(function() {
      log.info('message: ExternalOutput closed');
      delete self.externalOutputs[url];
    });
  }

  hasExternalOutput(url) {
    return this.externalOutputs[url] !== undefined;
  }

  getExternalOutput(url) {
    return this.externalOutputs[url];
  }

  muteStream(muteVideo, muteAudio) {
    this.muteVideo = muteVideo;
    this.muteAudio = muteAudio;
    for (var subId in this.subscribers) {
      var sub = this.getSubscriber(subId);
      this.muteSubscriberStream(subId, sub.muteVideo, sub.muteAudio);
    }
  }

  setQualityLayer(id, spatialLayer, temporalLayer) {
    var subscriber = this.getSubscriber(id);
    log.info('message: setQualityLayer, spatialLayer: ', spatialLayer,
                                     ', temporalLayer: ', temporalLayer);
    subscriber.mediaStream.setQualityLayer(spatialLayer, temporalLayer);
  }

  muteSubscriberStream(id, muteVideo, muteAudio) {
    var subscriber = this.getSubscriber(id);
    subscriber.muteVideo = muteVideo;
    subscriber.muteAudio = muteAudio;
    log.info('message: Mute Stream, video: ', this.muteVideo || muteVideo,
                                 ', audio: ', this.muteAudio || muteAudio);
    subscriber.mediaStream.muteStream(this.muteVideo || muteVideo,
                          this.muteAudio || muteAudio);
  }

  setVideoConstraints(id, width, height, frameRate) {
    var subscriber = this.getSubscriber(id);
    var maxWidth = (width && width.max !== undefined) ? width.max : -1;
    var maxHeight = (height && height.max !== undefined) ? height.max : -1;
    var maxFrameRate = (frameRate && frameRate.max !== undefined) ? frameRate.max : -1;
    subscriber.mediaStream.setVideoConstraints(maxWidth, maxHeight, maxFrameRate);
  }

  enableHandlers(id, handlers) {
    var wrtc = this.wrtc;
    if (id) {
      wrtc = this.getSubscriber(id);
    }
    if (wrtc) {
      for (var index in handlers) {
        wrtc.mediaStream.enableHandler(handlers[index]);
      }
    }
  }

  disableHandlers(id, handlers) {
    var wrtc = this.wrtc;
    if (id) {
      wrtc = this.getSubscriber(id);
    }
    if (wrtc) {
      for (var index in handlers) {
        wrtc.mediaStream.disableHandler(handlers[index]);
      }
    }
  }
}

class Publisher extends Source {
  constructor(id, threadPool, ioThreadPool, options) {
    super(id, threadPool, ioThreadPool);
    this.mediaConfiguration = options.mediaConfiguration;
    this.wrtc = createWrtc(this.id, this.threadPool, this.ioThreadPool, options.mediaConfiguration);
    this.wrtc.wrtcId = id;
    this.wrtc.mediaConfiguration = options.mediaConfiguration;

    this.wrtc.mediaStream = createMediaStream(this.wrtc, this.wrtc.wrtcId);
    this.wrtc.addMediaStream(this.wrtc.mediaStream);

    this.minVideoBW = options.minVideoBW;
    this.scheme = options.scheme;

    this.wrtc.mediaStream.setAudioReceiver(this.muxer);
    this.wrtc.mediaStream.setVideoReceiver(this.muxer);
    this.muxer.setPublisher(this.wrtc.mediaStream);
    const muteVideo = (options.muteStream && options.muteStream.video) || false;
    const muteAudio = (options.muteStream && options.muteStream.audio) || false;
    this.muteStream(muteVideo, muteAudio);
  }

  resetWrtc() {
    if (this.numSubscribers > 0) {
      return;
    }
    this.wrtc = createWrtc(this.id, this.threadPool, this.ioThreadPool, this.mediaConfiguration);
    this.wrtc.mediaStream = createMediaStream(this.wrtc, this.wrtc.wrtcId);

    this.wrtc.mediaStream.setAudioReceiver(this.muxer);
    this.wrtc.mediaStream.setVideoReceiver(this.muxer);
    this.muxer.setPublisher(this.wrtc.mediaStream);
  }
}

class ExternalInput extends Source {
  constructor(id, threadPool, ioThreadPool, url) {
    super(id, threadPool, ioThreadPool);
    var eiId = id + '_' + url;

    log.info('message: Adding ExternalInput, id: ' + eiId);

    var ei = new addon.ExternalInput(url);

    this.ei = ei;
    ei.wrtcId = eiId;

    this.subscribers = {};
    this.externalOutputs = {};

    ei.setAudioReceiver(this.muxer);
    ei.setVideoReceiver(this.muxer);
    this.muxer.setExternalPublisher(ei);
  }

  init() {
    return this.ei.init();
  }
}

exports.Publisher = Publisher;
exports.ExternalInput = ExternalInput;
