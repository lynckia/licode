/*global require, exports*/
'use strict';

var config = require('config');
var ERIZO_API_ADDON_PATH = config.get('erizo.erizoAPIAddonPath');

var addon = require('../' + ERIZO_API_ADDON_PATH);
var logger = require('../common/logger').logger;

// Logger
var log = logger.getLogger('Publisher');

function createWrtc(id, threadPool, ioThreadPool) {
  var wrtc = new addon.WebRtcConnection(threadPool, ioThreadPool, id,
                                    GLOBAL.config.erizo.stunserver,
                                    GLOBAL.config.erizo.stunport,
                                    GLOBAL.config.erizo.minport,
                                    GLOBAL.config.erizo.maxport,
                                    false,
                                    JSON.stringify(GLOBAL.mediaConfig),
                                    GLOBAL.config.erizo.useNicer,
                                    GLOBAL.config.erizo.turnserver,
                                    GLOBAL.config.erizo.turnport,
                                    GLOBAL.config.erizo.turnusername,
                                    GLOBAL.config.erizo.turnpass,
                                    GLOBAL.config.erizo.networkinterface);

  return wrtc;
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
    var wrtc = createWrtc(wrtcId, this.threadPool, this.ioThreadPool);
    wrtc.wrtcId = wrtcId;
    this.subscribers[id] = wrtc;
    this.muxer.addSubscriber(wrtc, id);
    wrtc.minVideoBW = this.minVideoBW;
    log.debug('message: Setting scheme from publisher to subscriber, ' +
              'id: ' + wrtcId + ', scheme: ' + this.scheme+
               ', ' + logger.objectToLog(options.metadata));
    wrtc.scheme = this.scheme;
    this.muteSubscriberStream(id, false, false);
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

  addExternalOutput(url) {
    var eoId = url + '_' + this.id;
    log.info('message: Adding ExternalOutput, id: ' + eoId);
    var externalOutput = new addon.ExternalOutput(url);
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
    subscriber.setQualityLayer(spatialLayer, temporalLayer);
  }

  muteSubscriberStream(id, muteVideo, muteAudio) {
    var subscriber = this.getSubscriber(id);
    subscriber.muteVideo = muteVideo;
    subscriber.muteAudio = muteAudio;
    log.info('message: Mute Stream, video: ', this.muteVideo || muteVideo,
                                 ', audio: ', this.muteAudio || muteAudio);
    subscriber.muteStream(this.muteVideo || muteVideo,
                          this.muteAudio || muteAudio);
  }

  enableHandlers(id, handlers) {
    var wrtc = this.wrtc;
    if (id) {
      wrtc = this.getSubscriber(id);
    }
    if (wrtc) {
      for (var index in handlers) {
        wrtc.enableHandler(handlers[index]);
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
        wrtc.disableHandler(handlers[index]);
      }
    }
  }
}

class Publisher extends Source {
  constructor(id, threadPool, ioThreadPool, options) {
    super(id, threadPool, ioThreadPool);
    this.wrtc = createWrtc(this.id, this.threadPool, this.ioThreadPool);
    this.wrtc.wrtcId = id;

    this.minVideoBW = options.minVideoBW;
    this.scheme = options.scheme;

    this.wrtc.setAudioReceiver(this.muxer);
    this.wrtc.setVideoReceiver(this.muxer);
    this.muxer.setPublisher(this.wrtc);
  }

  resetWrtc() {
    if (this.numSubscribers > 0) {
      return;
    }
    this.wrtc = createWrtc(this.id, this.threadPool, this.ioThreadPool);
    this.wrtc.setAudioReceiver(this.muxer);
    this.wrtc.setVideoReceiver(this.muxer);
    this.muxer.setPublisher(this.wrtc);
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
