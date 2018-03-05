/*global require, exports*/
'use strict';
var addon = require('./../../../erizoAPI/build/Release/addon');
var logger = require('./../../common/logger').logger;
var Helpers = require('./Helpers');

// Logger
var log = logger.getLogger('Publisher');

class Node {
  constructor(clientId, streamId) {
    this.clientId = clientId;
    this.streamId = streamId;
    this.erizoStreamId = Helpers.getErizoStreamId(clientId, streamId);
  }
}

class Subscriber extends Node {
   constructor(clientId, streamId, connection, options) {
     super(clientId, streamId);
     this.connection = connection;
     this.connection.mediaConfiguration = options.mediaConfiguration;
     this.connection.addMediaStream(this.erizoStreamId);
     this.mediaStream = connection.getMediaStream(this.erizoStreamId);
   }
  close() {
    this.connection.removeMediaStream(this.mediaStream.id);
  }
}

class Source extends Node {
  constructor(clientId, streamId, threadPool) {
    super(clientId, streamId);
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
    const subscriber = new Subscriber(clientId, this.streamId, connection, options); 
    
    this.subscribers[clientId] = subscriber;
    this.muxer.addSubscriber(subscriber.mediaStream, subscriber.mediaStream.id);
    subscriber.mediaStream.minVideoBW = this.minVideoBW;

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
  }

  removeSubscriber(clientId) {
    let subscriber = this.subscribers[clientId];
    if (subscriber === undefined) {
      log.warn(`message: subscriber to remove not found clientId: ${clientId}, ` +
        `streamId: ${this.streamId}`);
      return;
    }
    
    this.muxer.removeSubscriber(subscriber.mediaStream.id);
    delete this.subscribers[clientId];
  }

  getSubscriber(clientId) {
    return this.subscribers[clientId];
  }

  hasSubscriber(clientId) {
    return this.subscribers[clientId] !== undefined;
  }

  addExternalOutput(url, options) {
    var eoId = url + '_' + this.streamId;
    log.info('message: Adding ExternalOutput, id: ' + eoId + ', url: ' + url);
    var externalOutput = new addon.ExternalOutput(this.threadPool, url,
      Helpers.getMediaConfiguration(options.mediaConfiguration));
    externalOutput.id = eoId;
    externalOutput.init();
    this.muxer.addExternalOutput(externalOutput, url);
    this.externalOutputs[url] = externalOutput;
  }

  removeExternalOutput(url) {
    log.info(`message: Removing ExternalOutput, url: ${url}`);
    return new Promise(function(resolve) {
      this.muxer.removeSubscriber(url);
      this.externalOutputs[url].close(function() {
        log.info('message: ExternalOutput closed');
        delete this.externalOutputs[url];
        resolve();
      });
    }.bind(this));
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

  muteStream(muteVideo, muteAudio) {
    this.muteVideo = muteVideo;
    this.muteAudio = muteAudio;
    for (var subId in this.subscribers) {
      var sub = this.getSubscriber(subId);
      this.muteSubscriberStream(subId, sub.muteVideo, sub.muteAudio);
    }
  }

  setQualityLayer(clientId, spatialLayer, temporalLayer) {
    var subscriber = this.getSubscriber(clientId);
    log.info('message: setQualityLayer, spatialLayer: ', spatialLayer,
                                     ', temporalLayer: ', temporalLayer);
    subscriber.mediaStream.setQualityLayer(spatialLayer, temporalLayer);
  }

  muteSubscriberStream(clientId, muteVideo, muteAudio) {
    var subscriber = this.getSubscriber(clientId);
    subscriber.muteVideo = muteVideo;
    subscriber.muteAudio = muteAudio;
    log.info('message: Mute Stream, video: ', this.muteVideo || muteVideo,
                                 ', audio: ', this.muteAudio || muteAudio);
    subscriber.mediaStream.muteStream(this.muteVideo || muteVideo,
                          this.muteAudio || muteAudio);
  }

  setVideoConstraints(clientId, width, height, frameRate) {
    var subscriber = this.getSubscriber(clientId);
    var maxWidth = (width && width.max !== undefined) ? width.max : -1;
    var maxHeight = (height && height.max !== undefined) ? height.max : -1;
    var maxFrameRate = (frameRate && frameRate.max !== undefined) ? frameRate.max : -1;
    subscriber.mediaStream.setVideoConstraints(maxWidth, maxHeight, maxFrameRate);
  }

  enableHandlers(clientId, handlers) {
    let mediaStream = this.mediaStream;
    if (clientId) {
      mediaStream = this.getSubscriber(clientId).mediaStream;
    }
    if (mediaStream) {
      for (var index in handlers) {
        mediaStream.enableHandler(handlers[index]);
      }
    }
  }

  disableHandlers(clientId, handlers) {
    let mediaStream = this.mediaStream;
    if (clientId) {
      mediaStream = this.getSubscriber(clientId).mediaStream;
    }
    if (mediaStream) {
      for (var index in handlers) {
        mediaStream.disableHandler(handlers[index]);
      }
    }
  }
}

class Publisher extends Source {
  constructor(clientId, streamId, connection, options) {
    super(clientId, streamId, connection.threadPool);
    this.mediaConfiguration = options.mediaConfiguration;
    this.connection = connection;
    this.connection.mediaConfiguration = options.mediaConfiguration;
    
    this.connection.addMediaStream(streamId);
    this.mediaStream = this.connection.getMediaStream(streamId);
    

    this.minVideoBW = options.minVideoBW;
    this.scheme = options.scheme;

    this.mediaStream.setAudioReceiver(this.muxer);
    this.mediaStream.setVideoReceiver(this.muxer);
    this.muxer.setPublisher(this.mediaStream);
    const muteVideo = (options.muteStream && options.muteStream.video) || false;
    const muteAudio = (options.muteStream && options.muteStream.audio) || false;
    this.muteStream(muteVideo, muteAudio);
  }

  close() {
    this.connection.removeMediaStream(this.mediaStream.id);
  }
}

class ExternalInput extends Source {
  constructor(url, streamId, threadPool) {
    super(url, streamId, threadPool);
    var eiId = streamId + '_' + url;

    log.info('message: Adding ExternalInput, id: ' + eiId);

    var ei = new addon.ExternalInput(url);

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
