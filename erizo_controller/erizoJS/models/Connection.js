/*global require, exports*/
'use strict';
var addon = require('./../../../erizoAPI/build/Release/addon');
var logger = require('./../../common/logger').logger;
var log = logger.getLogger('Connection');

class Connection {

  constructor (id, threadPool, ioThreadPool) {
    log.info(`message: constructor, id: ${id}`);
    this.id = id;
    this.threadPool = threadPool;
    this.ioThreadPool = ioThreadPool;
    this.mediaConfiguration = 'default';
    //  {id: stream}
    this.mediaStreams = new Map();
    this.wrtc = this._createWrtc();
  }

  _getMediaConfiguration(mediaConfiguration = 'default') {
    if (global.mediaConfig && global.mediaConfig.codecConfigurations) {
      if (global.mediaConfig.codecConfigurations[mediaConfiguration]) {
        return JSON.stringify(global.mediaConfig.codecConfigurations[mediaConfiguration]);
      } else if (global.mediaConfig.codecConfigurations.default) {
        return JSON.stringify(global.mediaConfig.codecConfigurations.default);
      } else {
        log.warn(
          'message: Bad media config file. You need to specify a default codecConfiguration.'
         );
        return JSON.stringify({});
      }
    } else {
      log.warn(
        'message: Bad media config file. You need to specify a default codecConfiguration.'
      );
      return JSON.stringify({});
    }
  }

  _createWrtc() {
    var wrtc = new addon.WebRtcConnection(this.threadPool, this.ioThreadPool, this.id,
      global.config.erizo.stunserver,
      global.config.erizo.stunport,
      global.config.erizo.minport,
      global.config.erizo.maxport,
      false,
      this._getMediaConfiguration(this.mediaConfiguration),
      global.config.erizo.useNicer,
      global.config.erizo.turnserver,
      global.config.erizo.turnport,
      global.config.erizo.turnusername,
      global.config.erizo.turnpass,
      global.config.erizo.networkinterface);

    return wrtc;
  }

  _createMediaStream (id) {
    log.debug(`message: _createMediaStream, connectionId: ${this.id}, mediaStreamId: ${id}`);
    const mediaStream = new addon.MediaStream(this.threadPool, this.wrtc, id,
      this._getMediaConfiguration(this.mediaConfiguration));
    mediaStream.id = id;
    return mediaStream;
  }

  addMediaStream(id) {
    log.info(`message: addMediaStream, connectionId: ${this.id}, mediaStreamId: ${id}`);
    if (this.mediaStreams.get(id) === undefined) {
      const mediaStream = this._createMediaStream(id);
      this.wrtc.addMediaStream(mediaStream);
      this.mediaStreams.set(id, mediaStream);
    }
  }
  removeMediaStream(id) {
    if(this.mediaStreams.get(id) !== undefined) {
      this.wrtc.removeMediaStream(id);
      this.mediaStreams.get(id).close();
      this.mediaStreams.delete(id);
      log.debug(`removed mediaStreamId ${id}, remaining size ${this.mediaStreams.size}`);
    } else {
      log.error(`message: Trying to remove mediaStream not found, id: ${id}`);
    }
  }
  
  getMediaStream(id) {
    return this.mediaStreams.get(id);
  }

  close() {
    log.info(`message: Closing connection ${this.id}`);
    this.mediaStreams.forEach((mediaStream, id) => {
      log.debug(`message: Closing mediaStream, connectionId : ${this.id}, `+
        `mediaStreamId: ${id}`);
      mediaStream.close();
    });
    this.wrtc.close();
    delete this.mediaStreams;
    delete this.wrtc;
  }
  
}
exports.Connection = Connection;
