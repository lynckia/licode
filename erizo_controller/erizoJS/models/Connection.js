/* global require, exports */


const events = require('events');
// eslint-disable-next-line import/no-unresolved
const addon = require('./../../../erizoAPI/build/Release/addon');
const logger = require('./../../common/logger').logger;
const SessionDescription = require('./SessionDescription');
const SemanticSdp = require('./../../common/semanticSdp/SemanticSdp');
const PerformanceStats = require('./../../common/PerformanceStats');
const sdpTransform = require('sdp-transform');
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

const CONNECTION_QUALITY_LEVEL_UPDATE_INTERVAL = 5000; // ms
const CONNECTION_QUALITY_LEVEL_INCREASE_UPDATE_INTERVAL = 30000; // ms

class Connection extends events.EventEmitter {
  constructor(erizoControllerId, id, threadPool, ioThreadPool, clientId,
    streamPriorityStrategy = false, options = {}) {
    super();
    log.info(`message: constructor, id: ${id},`, logger.objectToLog(options), logger.objectToLog(options.metadata));
    this.id = id;
    this.erizoControllerId = erizoControllerId;
    this.clientId = clientId;
    this.threadPool = threadPool;
    this.ioThreadPool = ioThreadPool;
    this.mediaConfiguration = 'default';
    //  {id: stream}
    this.mediaStreams = new Map();
    this.options = options;
    this.streamPriorityStrategy = streamPriorityStrategy;
    this.wrtc = this._createWrtc();
    this.initialized = false;
    this.qualityLevel = -1;
    this.trickleIce = options.trickleIce || false;
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
    this.isNegotiationLocked = false;
    this.queue = [];
    this.lastQualityLevelChanged = new Date() - CONNECTION_QUALITY_LEVEL_INCREASE_UPDATE_INTERVAL;
  }

  _logSdp(...message) {
    log.debug('negotiation:', ...message, ', id:', this.id, ', lockReason: ', this.lockReason, ',',
      logger.objectToLog(this.options), logger.objectToLog(this.options.metadata));
  }

  static _getMediaConfiguration(mediaConfiguration = 'default') {
    if (global.mediaConfig && global.mediaConfig.codecConfigurations) {
      if (global.mediaConfig.codecConfigurations[mediaConfiguration]) {
        return JSON.stringify(global.mediaConfig.codecConfigurations[mediaConfiguration]);
      } else if (global.mediaConfig.codecConfigurations.default) {
        return JSON.stringify(global.mediaConfig.codecConfigurations.default);
      }
      log.warn(
        'message: Bad media config file. You need to specify a default codecConfiguration,',
        logger.objectToLog(this.options));
      return JSON.stringify({});
    }
    log.warn(
      'message: Bad media config file. You need to specify a default codecConfiguration,',
      logger.objectToLog(this.options));
    return JSON.stringify({});
  }

  static _getBwDistributionConfig(strategyId) {
    if (strategyId &&
      global.bwDistributorConfig.strategyDefinitions &&
      global.bwDistributorConfig.strategyDefinitions[strategyId]) {
      const requestedStrategyDefinition =
       global.bwDistributorConfig.strategyDefinitions[strategyId];
      if (requestedStrategyDefinition.priorities) {
        const serialized = Helpers.serializeStreamPriorityStrategy(requestedStrategyDefinition);
        if (serialized) {
          const result = {
            type: 'StreamPriority',
            strategyId,
            strategy: serialized,
          };
          return JSON.stringify(result);
        }
      }
      log.warn(`message: Bad strategy definition. Using default distributor Config ${global.bwDistributorConfig.defaultType}`);
      return JSON.stringify({ type: global.bwDistributorConfig.defaultType });
    }
    log.info(`message: No strategy definiton. Using default distributor Config ${global.bwDistributorConfig.defaultType}`);
    return JSON.stringify({ type: global.bwDistributorConfig.defaultType });
  }

  _createWrtc() {
    const wrtc = new addon.WebRtcConnection(this.threadPool, this.ioThreadPool, this.id,
      global.config.erizo.stunserver,
      global.config.erizo.stunport,
      global.config.erizo.minport,
      global.config.erizo.maxport,
      this.trickleIce,
      Connection._getMediaConfiguration(this.mediaConfiguration),
      Connection._getBwDistributionConfig(this.streamPriorityStrategy),
      global.config.erizo.useConnectionQualityCheck,
      global.config.erizo.turnserver,
      global.config.erizo.turnport,
      global.config.erizo.turnusername,
      global.config.erizo.turnpass,
      global.config.erizo.networkinterface);

    if (this.options) {
      const metadata = this.options.metadata || {};
      wrtc.setMetadata(JSON.stringify(metadata));
    }
    return wrtc;
  }

  _createMediaStream(id, options = {}, isPublisher = true, offerFromErizo = false) {
    log.debug(`message: _createMediaStream, connectionId: ${this.id}, ` +
              `mediaStreamId: ${id}, isPublisher: ${isPublisher},`,
    logger.objectToLog(this.options), logger.objectToLog(this.options.metadata),
    logger.objectToLog(options), logger.objectToLog(options.metadata));
    const sessionVersion = offerFromErizo ? this.sessionVersion : -1;
    const mediaStream = new addon.MediaStream(this.threadPool,
      this.wrtc, id,
      options.label,
      Connection._getMediaConfiguration(this.mediaConfiguration),
      isPublisher,
      sessionVersion);
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
      log.debug('getting local sdp for answer', info, ',',
        logger.objectToLog(this.options), logger.objectToLog(this.options.metadata));
      return { type: 'answer', sdp: info };
    });
  }

  createOffer(requestId = undefined) {
    return this.getLocalSdp().then((info) => {
      PerformanceStats.mark(requestId, PerformanceStats.Marks.CONNECTION_OFFER_CREATED);
      log.debug('getting local sdp for offer', info, ',',
        logger.objectToLog(this.options), logger.objectToLog(this.options.metadata));
      return { type: 'offer', sdp: info };
    });
  }

  getLocalSdp() {
    if (!this.wrtc) {
      return Promise.resolve();
    }
    return this.wrtc.getLocalDescription().then((desc) => {
      if (!this.wrtc || !desc) {
        log.error('Cannot get local description,',
          logger.objectToLog(this.options), logger.objectToLog(this.options.metadata));
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
      const timeSinceLastQualityLevel = new Date() - this.lastQualityLevelChanged;
      const canIncreaseQualityLevel = newQualityLevel > this.qualityLevel &&
          timeSinceLastQualityLevel > CONNECTION_QUALITY_LEVEL_INCREASE_UPDATE_INTERVAL;
      const canDecreaseQualityLevel = newQualityLevel < this.qualityLevel;
      if (canIncreaseQualityLevel || canDecreaseQualityLevel) {
        this.qualityLevel = newQualityLevel;
        this.lastQualityLevelChanged = new Date();
        this._onStatusEvent({ type: 'quality_level', level: this.qualityLevel }, CONN_QUALITY_LEVEL);
      }
    }
  }

  sendOffer(requestId = undefined) {
    PerformanceStats.mark(requestId, PerformanceStats.Marks.CONNECTION_OFFER_ENQUEUED);
    return this._enqueueOrSendOffer(requestId);
  }

  _enqueueOrSendOffer(requestId = undefined) {
    if (this.isNegotiationLocked) {
      this._logSdp('Enqueueing sendOffer, id:', this.id);
      return this._enqueueNegotiation(this._enqueueOrSendOffer.bind(this, requestId));
    }
    this._logSdp('SendOffer');

    this._lockNegotiation('sendOffer');
    PerformanceStats.mark(requestId, PerformanceStats.Marks.CONNECTION_OFFER_DEQUEUED);
    return this._sendOffer(requestId);
  }

  _sendOffer(requestId = undefined) {
    if (!this.alreadyGathered && !this.trickleIce) {
      return Promise.resolve();
    }
    this._logSdp('_sendOffer');
    return this.createOffer(requestId).then((info) => {
      log.debug(`message: sendOffer sending event, type: ${info.type}, sessionVersion: ${this.sessionVersion},`,
        logger.objectToLog(this.options), logger.objectToLog(this.options.metadata));
      this._onStatusEvent(info, CONN_SDP);
      PerformanceStats.mark(requestId, PerformanceStats.Marks.CONNECTION_OFFER_SENT);
    });
  }

  sendAnswer() {
    if (!this.alreadyGathered && !this.trickleIce) {
      return Promise.resolve();
    }
    this._logSdp('sendAnswer');
    return this.createAnswer().then((info) => {
      log.debug(`message: sendAnswer sending event, type: ${info.type}, sessionVersion: ${this.sessionVersion},`,
        logger.objectToLog(this.options), logger.objectToLog(this.options.metadata));
      this._onStatusEvent(info, CONN_SDP);
    });
  }

  init(createOffer = this.options.createOffer) {
    if (this.initialized) {
      return false;
    }
    this.initialized = true;
    this.qualityLevelInterval = setInterval(this.updateConnectionQualityLevel.bind(this),
      CONNECTION_QUALITY_LEVEL_UPDATE_INTERVAL);
    log.debug(`message: Init Connection, connectionId: ${this.id},`,
      logger.objectToLog(this.options), logger.objectToLog(this.options.metadata));
    this.sessionVersion = 0;

    this.wrtc.init((newStatus, mess) => {
      log.info('message: WebRtcConnection status update, ',
        `id: ${this.id}, status: ${newStatus},`,
        logger.objectToLog(this.options), logger.objectToLog(this.options.metadata));
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
            `id: ${this.id},`,
          logger.objectToLog(this.options), logger.objectToLog(this.options.metadata));
          this._onStatusEvent({ type: 'failed', sdp: mess }, newStatus);
          break;

        case CONN_READY:
          log.debug(`message: connection ready, id: ${this.id} status: ${newStatus},`,
            logger.objectToLog(this.options), logger.objectToLog(this.options.metadata));
          this._readyResolveFunction();
          this._onStatusEvent({ type: 'ready' }, newStatus);
          break;
        default:
          log.error(`message: unknown webrtc status ${newStatus},`,
            logger.objectToLog(this.options), logger.objectToLog(this.options.metadata));
      }
    });
    if (createOffer) {
      log.debug('message: create offer requested, id:', this.id, ',',
        logger.objectToLog(this.options), logger.objectToLog(this.options.metadata));
      const audioEnabled = createOffer.audio;
      const videoEnabled = createOffer.video;
      const bundle = createOffer.bundle;
      this.createOfferPromise = this.wrtc.createOffer(videoEnabled, audioEnabled, bundle);
    }
    this._initializeResolveFunction();
    return true;
  }

  addMediaStream(id, options, isPublisher, offerFromErizo) {
    let promise = Promise.resolve();
    log.info(`message: addMediaStream, connectionId: ${this.id}, mediaStreamId: ${id},`,
      logger.objectToLog(this.options), logger.objectToLog(this.options.metadata),
      logger.objectToLog(options), logger.objectToLog(options.metadata));
    if (this.mediaStreams.get(id) === undefined) {
      const mediaStream = this._createMediaStream(id, options, isPublisher, offerFromErizo);
      promise = this.wrtc.addMediaStream(mediaStream);
      this.mediaStreams.set(id, mediaStream);
    }
    return promise;
  }

  removeMediaStream(id, sendOffer = true, requestId = undefined) {
    const promise = Promise.resolve();
    if (this.mediaStreams.get(id) !== undefined) {
      const removePromise = this.wrtc.removeMediaStream(id);
      const closePromise = this.mediaStreams.get(id).close();
      removePromise.then(() => PerformanceStats.mark(requestId,
        PerformanceStats.Marks.CONNECTION_STREAM_REMOVED));
      closePromise.then(() => PerformanceStats.mark(requestId,
        PerformanceStats.Marks.CONNECTION_STREAM_CLOSED));
      this.mediaStreams.delete(id);
      return Promise.all([removePromise, closePromise]).then(() => {
        if (sendOffer) {
          return this.sendOffer(requestId);
        }
        return Promise.resolve();
      });
    }
    log.error(`message: Trying to remove mediaStream not found, clientId: ${this.clientId}, streamId: ${id}`,
      logger.objectToLog(this.options), logger.objectToLog(this.options.metadata));
    return promise;
  }

  setRemoteDescription(sdp, receivedSessionVersion = -1) {
    const sdpInfo = SemanticSdp.SDPInfo.processString(sdp);
    let oldIceCredentials = ['', ''];
    if (this.remoteDescription) {
      oldIceCredentials = this.remoteDescription.getICECredentials();
    }
    this.remoteDescription = new SessionDescription(sdpInfo, this.mediaConfiguration);
    this._logSdp('setRemoteDescription');
    const iceCredentials = this.remoteDescription.getICECredentials();
    if (oldIceCredentials[0] !== '' && oldIceCredentials[0] !== iceCredentials[0]) {
      this.alreadyGathered = false;
      this.onGathered = new Promise((resolve, reject) => {
        this._gatheredResolveFunction = resolve;
        this._gatheredRejectFunction = reject;
      });
      log.info(`message: ICE restart detected, clientId: ${this.clientId}`,
        logger.objectToLog(this.options), logger.objectToLog(this.options.metadata));
      this._logSdp('restartIce');
      this.wrtc.maybeRestartIce(iceCredentials[0], iceCredentials[1]);
    }
    return this.wrtc.setRemoteDescription(this.remoteDescription.connectionDescription,
      receivedSessionVersion);
  }

  addRemoteCandidate(sdpCandidate) {
    const candidatesInfo = sdpTransform.parse(sdpCandidate.candidate);
    if (candidatesInfo.sdpMid === 'end' || candidatesInfo.candidates === undefined) {
      return;
    }
    candidatesInfo.candidates.forEach((candidate) => {
      if (candidate.transport.toLowerCase() !== 'udp') {
        return;
      }
      this.wrtc.addRemoteCandidate(sdpCandidate.sdpMid, sdpCandidate.sdpMLineIndex,
        candidate.foundation, candidate.component, candidate.priority, candidate.transport,
        candidate.ip, candidate.port, candidate.type, candidate.raddr, candidate.rport,
        sdpCandidate.candidate);
    });
  }

  onSignalingMessage(msg) {
    this._logSdp('onSignalingMessage, type:', msg.type);
    if (msg.type === 'offer') {
      this._lockNegotiation('processOffer');
      return this._onSignalingMessage(msg).then(() => {
        this._unlockNegotiation();
        this._dequeueSignalingMessage();
      });
    }
    if (msg.type === 'answer') {
      if (!this.isNegotiationLocked) {
        log.warn('message: Received answer and negotiation was not locked, connectionId: ', this.id, ',',
          logger.objectToLog(this.options), logger.objectToLog(this.options.metadata));
      }
      const promise = this._onSignalingMessage(msg);
      this._unlockNegotiation();
      this._dequeueSignalingMessage();
      return promise;
    }

    if (this.isNegotiationLocked) {
      return this._enqueueNegotiation(this.onSignalingMessage.bind(this, msg));
    }

    return this._onSignalingMessage(msg);
  }

  _lockNegotiation(reason) {
    this.lockReason = reason;
    this._logSdp('_lockNegotiation');
    this.isNegotiationLocked = true;
  }

  _unlockNegotiation() {
    this.isNegotiationLocked = false;
    this._logSdp('_unlockNegotiation');
  }

  _enqueueNegotiation(negotiationCall) {
    this._logSdp('_enqueueNegotiation');
    return new Promise((success) => {
      this.queue.push(() => {
        negotiationCall().then(() => {
          success();
        }).catch(() => {
          success();
        });
        this._dequeueSignalingMessage();
      });
    });
  }

  _dequeueSignalingMessage() {
    if (this.isNegotiationLocked) {
      return;
    }
    this._logSdp('_dequeueNegotiation');
    if (this.queue.length > 0) {
      const func = this.queue.shift();
      func();
    }
  }

  _onSignalingMessage(msg) {
    this._logSdp('_onSignalingMessage, type:', msg.type);
    if (msg.type === 'offer') {
      return this.setRemoteDescription(msg.sdp, msg.receivedSessionVersion)
        .then(() => {
          const onEvent = this.trickleIce ? this.onInitialized : this.onGathered;
          return onEvent;
        })
        .then(() => this.sendAnswer())
        .catch(() => {
          log.error('message: Error processing offer/answer in connection, connectionId:', this.id, ',',
            logger.objectToLog(this.options), logger.objectToLog(this.options.metadata));
        });
    } else if (msg.type === 'offer-noanswer') {
      return this.setRemoteDescription(msg.sdp, msg.receivedSessionVersion).catch(() => {
        log.error('message: Error processing offer/noanswer in connection, connectionId:', this.id, ',',
          logger.objectToLog(this.options), logger.objectToLog(this.options.metadata));
      });
    } else if (msg.type === 'answer') {
      return this.setRemoteDescription(msg.sdp, msg.receivedSessionVersion).catch(() => {
        log.error('message: Error processing answer in connection, connectionId:', this.id, ',',
          logger.objectToLog(this.options), logger.objectToLog(this.options.metadata));
      });
    } else if (msg.type === 'candidate') {
      this.addRemoteCandidate(msg.candidate);
      return Promise.resolve();
    } else if (msg.type === 'updatestream') {
      if (msg.sdp) {
        return this.setRemoteDescription(msg.sdp, msg.receivedSessionVersion).catch(() => {
          log.error('message: Error processing updatestream in connection, connectionId:', this.id, ',',
            logger.objectToLog(this.options), logger.objectToLog(this.options.metadata));
        });
      }
    } else if (msg.type === 'offer-dropped') {
      log.debug('message: Offer dropped, sending again', this.id, ',',
        logger.objectToLog(this.options), logger.objectToLog(this.options.metadata));
      return this.sendOffer();
    } else if (msg.type === 'answer-dropped') {
      log.error('message: Answer dropped, sending again', this.id, ',',
        logger.objectToLog(this.options), logger.objectToLog(this.options.metadata));
    }
    return Promise.resolve();
  }

  setStreamPriorityStrategy(strategyId) {
    this.streamPriorityStrategy = strategyId;
    this.wrtc.setBwDistributionConfig(
      Connection._getBwDistributionConfig(this.streamPriorityStrategy));
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

  getDurationDistribution() {
    if (!this.wrtc) {
      return [];
    }
    return this.wrtc.getDurationDistribution();
  }

  getDelayDistribution() {
    if (!this.wrtc) {
      return [];
    }
    return this.wrtc.getDelayDistribution();
  }

  resetStats() {
    if (!this.wrtc) {
      return;
    }
    this.wrtc.resetStats();
  }

  close() {
    log.info(`message: Closing connection, id: ${this.id},`,
      logger.objectToLog(this.options), logger.objectToLog(this.options.metadata));
    log.info(`message: WebRtcConnection status update, id: ${this.id}, status: ${CONN_FINISHED},`,
      logger.objectToLog(this.options), logger.objectToLog(this.options.metadata));
    clearInterval(this.qualityLevelInterval);
    const promises = [];
    this.mediaStreams.forEach((mediaStream, id) => {
      log.debug(`message: Closing mediaStream, connectionId : ${this.id}, ` +
        `mediaStreamId: ${id},`,
      logger.objectToLog(this.options), logger.objectToLog(this.options.metadata));
      promises.push(mediaStream.close());
    });
    Promise.all(promises).then(() => {
      log.debug(`message: Closing WRTC, id: ${this.id},`,
        logger.objectToLog(this.options), logger.objectToLog(this.options.metadata));
      this.wrtc.close().then(() => {
        log.debug(`message: WRTC closed, id: ${this.id},`,
          logger.objectToLog(this.options), logger.objectToLog(this.options.metadata));
        delete this.wrtc;
      });
      this.mediaStreams.clear();
    });
  }
}
exports.Connection = Connection;
