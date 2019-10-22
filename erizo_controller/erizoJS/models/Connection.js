/* global require, exports */


const events = require('events');
// eslint-disable-next-line import/no-unresolved
const addon = require('./../../../erizoAPI/build/Release/addon');
const logger = require('./../../common/logger').logger;
const SessionDescription = require('./SessionDescription');
const SemanticSdp = require('./../../common/semanticSdp/SemanticSdp');
const Setup = require('./../../common/semanticSdp/Setup');
const Helpers = require('./Helpers');

const log = logger.getLogger('Connection');

const CONN_INITIAL = 101;
// CONN_STARTED        = 102,
const CONN_GATHERED = 103;
const CONN_READY = 104;
const CONN_FINISHED = 105;
const CONN_QUALITY_LEVEL = 150;
const CONN_CANDIDATE = 201;
const CONN_SDP = 202;
const CONN_SDP_PROCESSED = 203;
const CONN_FAILED = 500;
const WARN_BAD_CONNECTION = 502;

const RESEND_LAST_ANSWER_RETRY_TIMEOUT = 50;
const RESEND_LAST_ANSWER_MAX_RETRIES = 10;

const CONNECTION_QUALITY_LEVEL_UPDATE_INTERVAL = 5000; // ms

class Connection extends events.EventEmitter {
  constructor(erizoControllerId, id, threadPool, ioThreadPool, clientId, options = {}) {
    super();
    log.info(`message: constructor, id: ${id}`);
    this.id = id;
    this.erizoControllerId = erizoControllerId;
    this.clientId = clientId;
    this.threadPool = threadPool;
    this.ioThreadPool = ioThreadPool;
    this.mediaConfiguration = 'default';
    //  {id: stream}
    this.mediaStreams = new Map();
    this.wrtc = this._createWrtc();
    this.initialized = false;
    this.qualityLevel = -1;
    this.options = options;
    this.trickleIce = options.trickleIce || false;
    this.metadata = this.options.metadata || {};
    this.onGathered = new Promise((resolve, reject) => {
      this._gatheredResolveFunction = resolve;
      this._gatheredRejectFunction = reject;
    });
    this.onInitialized = new Promise((resolve, reject) => {
      this._initializeResolveFunction = resolve;
      this._initializeRejectFunction = reject;
    });
    this.onStarted = new Promise((resolve, reject) => {
      this._startResolveFunction = resolve;
      this._startRejectFunction = reject;
    });
    this.onReady = new Promise((resolve, reject) => {
      this._readyResolveFunction = resolve;
      this._readyRejectFunction = reject;
    });
  }

  static _getMediaConfiguration(mediaConfiguration = 'default') {
    if (global.mediaConfig && global.mediaConfig.codecConfigurations) {
      if (global.mediaConfig.codecConfigurations[mediaConfiguration]) {
        return JSON.stringify(global.mediaConfig.codecConfigurations[mediaConfiguration]);
      } else if (global.mediaConfig.codecConfigurations.default) {
        return JSON.stringify(global.mediaConfig.codecConfigurations.default);
      }
      log.warn(
        'message: Bad media config file. You need to specify a default codecConfiguration.');
      return JSON.stringify({});
    }
    log.warn(
      'message: Bad media config file. You need to specify a default codecConfiguration.');
    return JSON.stringify({});
  }

  _createWrtc() {
    const wrtc = new addon.WebRtcConnection(this.threadPool, this.ioThreadPool, this.id,
      global.config.erizo.stunserver,
      global.config.erizo.stunport,
      global.config.erizo.minport,
      global.config.erizo.maxport,
      this.trickleIce,
      Connection._getMediaConfiguration(this.mediaConfiguration),
      global.config.erizo.useNicer,
      global.config.erizo.useConnectionQualityCheck,
      global.config.erizo.turnserver,
      global.config.erizo.turnport,
      global.config.erizo.turnusername,
      global.config.erizo.turnpass,
      global.config.erizo.networkinterface);

    if (this.metadata) {
      wrtc.setMetadata(JSON.stringify(this.metadata));
    }
    return wrtc;
  }

  _createMediaStream(id, options = {}, isPublisher = true) {
    log.debug(`message: _createMediaStream, connectionId: ${this.id}, ` +
              `mediaStreamId: ${id}, isPublisher: ${isPublisher}`);
    const mediaStream = new addon.MediaStream(this.threadPool, this.wrtc, id,
      options.label, Connection._getMediaConfiguration(this.mediaConfiguration), isPublisher);
    mediaStream.id = id;
    mediaStream.label = options.label;
    if (options.metadata) {
      mediaStream.metadata = options.metadata;
      mediaStream.setMetadata(JSON.stringify(options.metadata));
    }
    mediaStream.onMediaStreamEvent((type, message) => {
      this._onMediaStreamEvent(type, message, mediaStream.id);
    });
    return mediaStream;
  }

  _onMediaStreamEvent(type, message, mediaStreamId) {
    const streamEvent = {
      type,
      mediaStreamId,
      message,
    };
    this.emit('media_stream_event', streamEvent);
  }

  _onStatusEvent(info, evt) {
    this.emit('status_event', this.erizoControllerId, this.clientId, this.id, info, evt);
  }

  createAnswer() {
    return this.getLocalSdp().then((info) => {
      log.debug('getting local sdp for answer', info);
      return { type: 'answer', sdp: info };
    });
  }

  createOffer() {
    return this.getLocalSdp().then((info) => {
      log.debug('getting local sdp for offer', info);
      return { type: 'offer', sdp: info };
    });
  }

  getLocalSdp() {
    return this.wrtc.getLocalDescription().then((desc) => {
      if (!desc) {
        log.error('Cannot get local description');
        return '';
      }
      this.wrtc.localDescription = new SessionDescription(desc);
      const sdp = this.wrtc.localDescription.getSdp(this.sessionVersion);
      this.sessionVersion += 1;
      let message = sdp.toString();
      message = message.replace(this.options.privateRegexp, this.options.publicIP);
      return message;
    });
  }

  updateConnectionQualityLevel() {
    if (this.wrtc) {
      const newQualityLevel = this.wrtc.getConnectionQualityLevel();
      if (newQualityLevel !== this.qualityLevel) {
        this.qualityLevel = newQualityLevel;
        this._onStatusEvent({ type: 'quality_level', level: this.qualityLevel }, CONN_QUALITY_LEVEL);
      }
    }
  }

  sendOffer() {
    if (!this.alreadyGathered && !this.trickleIce) {
      return;
    }
    this.createOffer().then((info) => {
      log.debug(`message: sendOffer sending event, type: ${info.type}, sessionVersion: ${this.sessionVersion}`);
      this._onStatusEvent(info, CONN_SDP);
    });
  }

  sendAnswer(evt = CONN_SDP_PROCESSED, forceOffer = false) {
    if (!this.alreadyGathered && !this.trickleIce) {
      return;
    }
    const promise =
      this.options.createOffer || forceOffer ? this.createOffer() : this.createAnswer();
    promise.then((info) => {
      log.debug(`message: sendAnswer sending event, type: ${info.type}, sessionVersion: ${this.sessionVersion}`);
      this._onStatusEvent(info, evt);
    });
  }

  _resendLastAnswer(evt, streamId, label, forceOffer = false, removeStream = false) {
    if (!this.wrtc || !this.wrtc.localDescription) {
      log.debug('message: _resendLastAnswer, this.wrtc or this.wrtc.localDescription are not present');
      return Promise.reject('fail');
    }
    const isOffer = this.options.createOffer || forceOffer;
    return this.wrtc.getLocalDescription().then((localDescription) => {
      if (!localDescription) {
        log.error('message: _resendLastAnswer, Cannot get local description');
        return Promise.reject('fail');
      }
      this.wrtc.localDescription = new SessionDescription(localDescription);
      const sdp = this.wrtc.localDescription.getSdp(this.sessionVersion);
      const stream = sdp.getStream(label);
      if (stream && removeStream) {
        log.info(`resendLastAnswer: StreamId ${streamId} is stream and removeStream, label ${label}, sessionVersion ${this.sessionVersion}`);
        return Promise.reject('retry');
      }
      this.sessionVersion += 1;

      if (isOffer) {
        sdp.medias.forEach((media) => {
          if (media.getSetup() !== Setup.ACTPASS) {
            media.setSetup(Setup.ACTPASS);
          }
        });
      }

      let message = sdp.toString();
      message = message.replace(this.options.privateRegexp, this.options.publicIP);

      const info = { type: isOffer ? 'offer' : 'answer', sdp: message };
      log.debug(`message: _resendLastAnswer sending event, type: ${info.type}, streamId: ${streamId}`);
      this._onStatusEvent(info, evt);
      return Promise.resolve();
    });
  }

  init(createOffer = this.options.createOffer) {
    if (this.initialized) {
      return false;
    }
    this.initialized = true;
    this.qualityLevelInterval = setInterval(this.updateConnectionQualityLevel.bind(this),
      CONNECTION_QUALITY_LEVEL_UPDATE_INTERVAL);
    log.debug(`message: Init Connection, connectionId: ${this.id} `,
      logger.objectToLog(this.options));
    this.sessionVersion = 0;

    this.wrtc.init((newStatus, mess) => {
      log.info('message: WebRtcConnection status update, ' +
        `id: ${this.id}, status: ${newStatus}`,
        logger.objectToLog(this.metadata));
      switch (newStatus) {
        case CONN_INITIAL:
          this._startResolveFunction();
          break;

        case CONN_SDP_PROCESSED:
        case CONN_SDP:
          break;

        case CONN_GATHERED:
          this.alreadyGathered = true;
          this._gatheredResolveFunction();
          break;

        case CONN_CANDIDATE:
          // eslint-disable-next-line no-param-reassign
          mess = mess.replace(this.options.privateRegexp, this.options.publicIP);
          this._onStatusEvent({ type: 'candidate', candidate: mess }, newStatus);
          break;

        case CONN_FAILED:
          log.warn(`message: failed the ICE process, code: ${WARN_BAD_CONNECTION},` +
            `id: ${this.id}`);
          this._onStatusEvent({ type: 'failed', sdp: mess }, newStatus);
          break;

        case CONN_READY:
          log.debug(`message: connection ready, id: ${this.id} status: ${newStatus}`);
          this._readyResolveFunction();
          this._onStatusEvent({ type: 'ready' }, newStatus);
          break;
        default:
          log.error(`message: unknown webrtc status ${newStatus}`);
      }
    });
    if (createOffer) {
      log.debug('message: create offer requested, id:', this.id);
      const audioEnabled = createOffer.audio;
      const videoEnabled = createOffer.video;
      const bundle = createOffer.bundle;
      this.createOfferPromise = this.wrtc.createOffer(videoEnabled, audioEnabled, bundle);
    }
    this._initializeResolveFunction();
    return true;
  }

  addMediaStream(id, options, isPublisher) {
    let promise = Promise.resolve();
    log.info(`message: addMediaStream, connectionId: ${this.id}, mediaStreamId: ${id}`);
    if (this.mediaStreams.get(id) === undefined) {
      const mediaStream = this._createMediaStream(id, options, isPublisher);
      promise = this.wrtc.addMediaStream(mediaStream);
      this.mediaStreams.set(id, mediaStream);
    }
    return promise;
  }

  removeMediaStream(id, sendOffer = true) {
    const promise = Promise.resolve();
    if (this.mediaStreams.get(id) !== undefined) {
      const label = this.mediaStreams.get(id).label;
      const removePromise = this.wrtc.removeMediaStream(id);
      const closePromise = this.mediaStreams.get(id).close();
      this.mediaStreams.delete(id);
      const retryPromise = Helpers.retryWithPromise(
        this._resendLastAnswer.bind(this, CONN_SDP, id, label, sendOffer, true),
        RESEND_LAST_ANSWER_RETRY_TIMEOUT, RESEND_LAST_ANSWER_MAX_RETRIES);
      return Promise.all([removePromise, closePromise, retryPromise]);
    }
    log.error(`message: Trying to remove mediaStream not found, id: ${id}`);
    return promise;
  }

  setRemoteDescription(sdp) {
    this.remoteDescription = new SessionDescription(sdp, this.mediaConfiguration);
    return this.wrtc.setRemoteDescription(this.remoteDescription.connectionDescription);
  }

  processOffer(sdp) {
    const sdpInfo = SemanticSdp.SDPInfo.processString(sdp);
    return this.setRemoteDescription(sdpInfo);
  }

  processAnswer(sdp) {
    const sdpInfo = SemanticSdp.SDPInfo.processString(sdp);
    return this.setRemoteDescription(sdpInfo);
  }

  addRemoteCandidate(candidate) {
    this.wrtc.addRemoteCandidate(candidate.sdpMid, candidate.sdpMLineIndex, candidate.candidate);
  }

  onSignalingMessage(msg) {
    if (msg.type === 'offer') {
      let onEvent;
      if (this.trickleIce) {
        onEvent = this.onInitialized;
      } else {
        onEvent = this.onGathered;
      }
      return this.processOffer(msg.sdp)
          .then(() => onEvent)
          .then(() => {
            this.sendAnswer();
          }).catch(() => {
            log.error('message: Error processing offer/answer in connection, connectionId:', this.id);
          });
    } else if (msg.type === 'offer-noanswer') {
      return this.processOffer(msg.sdp).catch(() => {
        log.error('message: Error processing offer/noanswer in connection, connectionId:', this.id);
      });
    } else if (msg.type === 'answer') {
      return this.processAnswer(msg.sdp).catch(() => {
        log.error('message: Error processing answer in connection, connectionId:', this.id);
      });
    } else if (msg.type === 'candidate') {
      this.addRemoteCandidate(msg.candidate);
      return Promise.resolve();
    } else if (msg.type === 'updatestream') {
      if (msg.sdp) {
        return this.processOffer(msg.sdp).catch(() => {
          log.error('message: Error processing updatestream in connection, connectionId:', this.id);
        });
      }
    }
    return Promise.resolve();
  }

  getMediaStream(id) {
    return this.mediaStreams.get(id);
  }

  getNumMediaStreams() {
    return this.mediaStreams.size;
  }

  getStats(callback) {
    if (!this.wrtc) {
      callback('{}');
      return true;
    }
    return this.wrtc.getStats(callback);
  }

  close() {
    log.info(`message: Closing connection ${this.id}`);
    log.info(`message: WebRtcConnection status update, id: ${this.id}, status: ${CONN_FINISHED}, ` +
            `${logger.objectToLog(this.metadata)}`);
    clearInterval(this.qualityLevelInterval);
    const promises = [];
    this.mediaStreams.forEach((mediaStream, id) => {
      log.debug(`message: Closing mediaStream, connectionId : ${this.id}, ` +
        `mediaStreamId: ${id}`);
      promises.push(mediaStream.close());
    });
    Promise.all(promises).then(() => {
      this.wrtc.close();
      this.mediaStreams.clear();
      delete this.wrtc;
    });
  }

}
exports.Connection = Connection;
