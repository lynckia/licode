/* global require, exports, setImmediate */

const EventEmitter = require('events');

// eslint-disable-next-line
const erizo = require(`./../../../erizoAPI/build/Release/${global.config.erizo.addon}`);
const logger = require('./../../common/logger').logger;
const SessionDescription = require('./SessionDescription');
const SemanticSdp = require('./../../common/semanticSdp/SemanticSdp');
const PerformanceStats = require('../../common/PerformanceStats');
const Helpers = require('./Helpers');

const sdpTransform = require('sdp-transform');

const log = logger.getLogger('WebRtcConnection');

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

class WebRtcConnection extends EventEmitter {
  constructor(configuration) {
    super();
    this.threadPool = configuration.threadPool;
    this.ioThreadPool = configuration.ioThreadPool;
    this.trickleIce = configuration.trickleIce || false;
    this.mediaConfiguration = configuration.mediaConfiguration || 'default';
    this.id = configuration.id;
    this.erizoControllerId = configuration.erizoControllerId;
    this.clientId = configuration.clientId;
    this.encryptTransport = configuration.encryptTransport;
    this.streamPriorityStrategy = configuration.streamPriorityStrategy;
    this.connectionTargetBw = configuration.connectionTargetBw;
    //  {id: stream}
    this.mediaStreams = new Map();
    this.options = configuration.options;
    this.initialized = false;
    this.qualityLevel = -1;
    this.willReceivePublishers = configuration.isRemote;

    this.lastQualityLevelChanged = new Date() - CONNECTION_QUALITY_LEVEL_INCREASE_UPDATE_INTERVAL;

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

    this.wrtc = this._createWrtc();
  }

  onWebRtcConnectionEvent(newStatus, mess) {
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
  }

  init() {
    if (this.initialized) {
      return false;
    }
    this.initialized = true;
    this.qualityLevelInterval = setInterval(this._updateConnectionQualityLevel.bind(this),
      CONNECTION_QUALITY_LEVEL_UPDATE_INTERVAL);
    log.debug(`message: Init Connection, connectionId: ${this.id},`,
      logger.objectToLog(this.options), logger.objectToLog(this.options.metadata));
    this.sessionVersion = 0;

    this.wrtc.init(this.onWebRtcConnectionEvent.bind(this));
    this._initializeResolveFunction();
    return true;
  }

  async addStream(id, options, isPublisher) {
    if (!this.wrtc) {
      return false;
    }
    log.info(`message: addMediaStream, connectionId: ${this.id}, mediaStreamId: ${id},`,
      logger.objectToLog(this.options), logger.objectToLog(this.options.metadata),
      logger.objectToLog(options), logger.objectToLog(options.metadata));
    if (this.mediaStreams.get(id) === undefined) {
      const mediaStream = this._createMediaStream(id, options, isPublisher);
      this.mediaStreams.set(id, mediaStream);
      mediaStream.isPublisher = isPublisher;
      await this.wrtc.addMediaStream(mediaStream);
      let updateNegotiationNeededFlag = false;
      if (!isPublisher) {
        this.internalNegotiationNeeded = true;
        updateNegotiationNeededFlag = true;
      }
      return updateNegotiationNeededFlag;
    }
    log.error(`message: Trying to add mediaStream that already existed, clientId: ${this.clientId}, streamId: ${id}`,
      logger.objectToLog(this.options), logger.objectToLog(this.options.metadata));
    return false;
  }

  async removeStream(id, requestId) {
    if (!this.wrtc) {
      return false;
    }
    if (this.mediaStreams.get(id) !== undefined) {
      const mediaStream = this.mediaStreams.get(id);
      if (requestId) {
        PerformanceStats.mark(requestId, PerformanceStats.Marks.REMOVING_NATIVE_STREAM);
      }
      const removePromise = this.wrtc.removeMediaStream(id);
      const closePromise = mediaStream.close();
      if (requestId) {
        removePromise.then(() =>
          PerformanceStats.mark(requestId, PerformanceStats.Marks.NATIVE_STREAM_REMOVED));
        closePromise.then(() =>
          PerformanceStats.mark(requestId, PerformanceStats.Marks.NATIVE_STREAM_CLOSED));
      }
      this.mediaStreams.delete(id);
      await Promise.all([removePromise, closePromise]);
      let updateNegotiationNeededFlag = false;
      if (!mediaStream.isPublisher) {
        this.internalNegotiationNeeded = true;
        updateNegotiationNeededFlag = true;
      }
      return updateNegotiationNeededFlag;
    }
    log.error(`message: Trying to remove mediaStream not found, clientId: ${this.clientId}, streamId: ${id}`,
      logger.objectToLog(this.options), logger.objectToLog(this.options.metadata));
    return false;
  }

  async close() {
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
    await Promise.all(promises);
    log.debug(`message: Closing WRTC, id: ${this.id},`,
      logger.objectToLog(this.options), logger.objectToLog(this.options.metadata));
    await this.wrtc.close();
    log.debug(`message: WRTC closed, id: ${this.id},`,
      logger.objectToLog(this.options), logger.objectToLog(this.options.metadata));
    delete this.wrtc;
    this.mediaStreams.clear();
  }

  async createAnswer() {
    if (!this.wrtc) {
      log.error('message: Cannot get local description on answer,',
        logger.objectToLog(this.options), logger.objectToLog(this.options.metadata));
      return { type: 'answer', sdp: '' };
    }
    if (!this.trickleIce) {
      await this.onGathered;
    }

    const desc = await this.wrtc.getLocalDescription();
    if (!desc) {
      log.error('message: Cannot get local description on answer,',
        logger.objectToLog(this.options), logger.objectToLog(this.options.metadata));
      return { type: 'answer', sdp: '' };
    }

    this.wrtc.localDescription = new SessionDescription(desc, undefined);
    const sdp = this.wrtc.localDescription.getSdp(this.sessionVersion);
    this.sessionVersion += 1;
    let message = sdp.toString();
    message = message.replace(this.options.privateRegexp, this.options.publicIP);
    return { type: 'answer', sdp: message };
  }

  async createOffer() {
    if (!this.wrtc) {
      return { type: 'offer', sdp: '' };
    }
    await this.wrtc.createOffer(true, true, true);
    if (!this.trickleIce) {
      await this.onGathered;
    }
    const desc = await this.wrtc.getLocalDescription();
    if (!this.wrtc || !desc) {
      log.error('Cannot get local description,',
        logger.objectToLog(this.options), logger.objectToLog(this.options.metadata));
      return { type: 'offer', sdp: '' };
    }

    this.wrtc.localDescription = new SessionDescription(desc, undefined);
    const sdp = this.wrtc.localDescription.getSdp(this.sessionVersion);
    this.sessionVersion += 1;
    let message = sdp.toString();
    message = message.replace(this.options.privateRegexp, this.options.publicIP);
    return { type: 'offer', sdp: message };
  }

  setLocalDescription(description) {
    // Noop
    if (!this.wrtc) {
      log.error('message: Cannot set local description,',
        logger.objectToLog(this.options), logger.objectToLog(this.options.metadata));
    }
    return description;
  }

  async setRemoteDescription(description) {
    const sdpInfo = SemanticSdp.SDPInfo.processString(description.sdp);
    let oldIceCredentials = ['', ''];
    if (this.wrtc.remoteDescription) {
      oldIceCredentials = this.wrtc.remoteDescription.getICECredentials();
    }
    this.wrtc.remoteDescription = new SessionDescription(sdpInfo, this.mediaConfiguration);
    this._logSdp('setRemoteDescription');
    const iceCredentials = this.wrtc.remoteDescription.getICECredentials();
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
    await this.wrtc.setRemoteDescription(this.wrtc.remoteDescription.connectionDescription);
  }

  async addIceCandidate(sdpCandidate) {
    const candidatesInfo = sdpTransform.parse(sdpCandidate.candidate);
    if (candidatesInfo.sdpMid === 'end' || candidatesInfo.candidates === undefined) {
      return;
    }
    const results = [];
    // eslint-disable-next-line no-restricted-syntax
    for (const candidate of candidatesInfo.candidates) {
      if (candidate.transport.toLowerCase() === 'udp') {
        results.push(this.wrtc.addRemoteCandidate(sdpCandidate.sdpMid, sdpCandidate.sdpMLineIndex,
          candidate.foundation, candidate.component, candidate.priority, candidate.transport,
          candidate.ip, candidate.port, candidate.type, candidate.raddr, candidate.rport,
          sdpCandidate.candidate));
      }
    }
    await Promise.all(results);
  }

  isNegotiationNeeded() {
    const needed = this.internalNegotiationNeeded;
    this.internalNegotiationNeeded = false;
    return needed;
  }

  getStream(id) {
    return this.mediaStreams.get(id);
  }

  getStreams() {
    return this.mediaStreams;
  }

  getNumStreams() {
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

  setConnectionTargetBw(connectionTargetBw) {
    this.connectionTargetBw = connectionTargetBw;
    this.wrtc.setConnectionTargetBw(connectionTargetBw);
  }

  setStreamPriorityStrategy(strategyId) {
    this.streamPriorityStrategy = strategyId;
    const strategy = WebRtcConnection._getBwDistributionConfig(this.streamPriorityStrategy);
    this.setConnectionTargetBw(strategy.connectionTargetBw);
    this.wrtc.setBwDistributionConfig(JSON.stringify(strategy));
  }

  copySdpInfoFromConnection(sourceConnection = {}) {
    if (sourceConnection &&
        sourceConnection.wrtc &&
        sourceConnection.wrtc.localDescription &&
        this.wrtc) {
      const publisherSdp = sourceConnection.wrtc.localDescription.connectionDescription;
      this.wrtc.copySdpToLocalDescription(publisherSdp);
    }
  }

  _logSdp(...message) {
    log.debug('negotiation:', ...message, ', id:', this.id, ', lockReason: ', this.lockReason, ',',
      logger.objectToLog(this.options), logger.objectToLog(this.options.metadata));
  }

  _updateConnectionQualityLevel() {
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

  _createWrtc() {
    const wrtc = new erizo.WebRtcConnection(this.threadPool, this.ioThreadPool, this.id,
      global.config.erizo.stunserver,
      global.config.erizo.stunport,
      global.config.erizo.minport,
      global.config.erizo.maxport,
      this.trickleIce,
      WebRtcConnection._getMediaConfiguration(this.mediaConfiguration, this.willReceivePublishers),
      JSON.stringify(WebRtcConnection._getBwDistributionConfig(this.streamPriorityStrategy)),
      global.config.erizo.useConnectionQualityCheck,
      this.encryptTransport,
      global.config.erizo.turnserver,
      global.config.erizo.turnport,
      global.config.erizo.turnusername,
      global.config.erizo.turnpass,
      global.config.erizo.networkinterface);

    if (this.options) {
      const metadata = this.options.metadata || {};
      wrtc.setMetadata(JSON.stringify(metadata));
    }
    wrtc.setConnectionTargetBw(this.connectionTargetBw);
    return wrtc;
  }

  _createMediaStream(id, options = {}, isPublisher = true) {
    log.debug(`message: _createMediaStream, connectionId: ${this.id}, ` +
    `mediaStreamId: ${id}, isPublisher: ${isPublisher},`,
    logger.objectToLog(this.options), logger.objectToLog(this.options.metadata),
    logger.objectToLog(options), logger.objectToLog(options.metadata));

    const handlerProfile = options.handlerProfile ? options.handlerProfile : 0;
    const handlers = global.config.erizo.handlerProfiles
      ? global.config.erizo.handlerProfiles[handlerProfile].slice() : [];
    for (let i = 0; i < handlers.length; i += 1) {
      handlers[i] = JSON.stringify(handlers[i]);
    }
    const mediaStream = new erizo.MediaStream(this.threadPool,
      this.wrtc, id, options.label, isPublisher,
      options.audio, options.video, handlers, options.priority);
    mediaStream.id = id;
    mediaStream.label = options.label;
    mediaStream.isPublisher = isPublisher;
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

  static _removeExtensionMappingFromArray(extMappings, extension) {
    const index = extMappings.indexOf(extension);
    if (index > -1) {
      extMappings.splice(index, index + 1);
    }
  }

  static _getMediaConfiguration(mediaConfiguration = 'default', willReceivePublishers = false) {
    if (global.mediaConfig && global.mediaConfig.codecConfigurations) {
      let config = {};
      if (global.mediaConfig.codecConfigurations[mediaConfiguration]) {
        config = JSON.parse(JSON.stringify(
          global.mediaConfig.codecConfigurations[mediaConfiguration]));
      } else if (global.mediaConfig.codecConfigurations.default) {
        config = JSON.parse(JSON.stringify(global.mediaConfig.codecConfigurations.default));
      } else {
        log.warn(
          'message: Bad media config file. You need to specify a default codecConfiguration,',
          logger.objectToLog(this.options));
      }
      if (!willReceivePublishers && config.extMappings) {
        WebRtcConnection._removeExtensionMappingFromArray(config.extMappings,
          'urn:ietf:params:rtp-hdrext:sdes:mid');
        WebRtcConnection._removeExtensionMappingFromArray(config.extMappings,
          'urn:ietf:params:rtp-hdrext:sdes:rtp-stream-id');
      }
      return JSON.stringify(config);
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
        const connectionTargetBw =
          global.bwDistributorConfig.strategyDefinitions[strategyId].connectionTargetBw ?
            global.bwDistributorConfig.strategyDefinitions[strategyId].connectionTargetBw : 0;
        if (serialized) {
          const result = {
            type: 'StreamPriority',
            strategyId,
            strategy: serialized,
            connectionTargetBw,
          };

          return result;
        }
      }
      log.warn(`message: Bad strategy definition. Using default distributor Config ${global.bwDistributorConfig.defaultType}`);
      return { type: global.bwDistributorConfig.defaultType };
    }
    log.info(`message: No strategy definiton. Using default distributor Config ${global.bwDistributorConfig.defaultType}`);
    return { type: global.bwDistributorConfig.defaultType };
  }
}

module.exports = WebRtcConnection;
