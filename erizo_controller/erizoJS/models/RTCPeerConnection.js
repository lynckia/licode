/* global require, exports, setImmediate */
const EventEmitter = require('events');
const util = require('util');

const WebRtcConnection = require('./WebRtcConnection');
const logger = require('./../../common/logger').logger;

const log = logger.getLogger('RTCPeerConnection');

function isEmpty(sdp) {
  return !sdp || sdp === '';
}

// https://html.spec.whatwg.org/multipage/webappapis.html#queue-a-task
const queueTask = util.promisify(setImmediate);

// https://w3c.github.io/webrtc-pc/
class RTCPeerConnection extends EventEmitter {
  constructor(configuration) {
    super();
    this.internalConnection = new WebRtcConnection(configuration);

    this.internalConnection.on('media_stream_event', (evt) => {
      this.emit('media_stream_event', evt);
    });

    this.internalConnection.on('status_event', (...args) => {
      this.emit('status_event', ...args);
    });

    // Some properties defined https://w3c.github.io/webrtc-pc/#constructor
    this.isClosed = false;
    this.negotiationNeeded = false;
    this.operations = [];
    this.updateNegotiationNeededFlagOnEmptyChain = false;
    this.lastCreatedOffer = '';
    this.lastCreatedAnswer = '';
    this.signalingState = 'stable';
    this.pendingLocalDescription = null;
    this.currentLocalDescription = null;
    this.pendingRemoteDescription = null;
    this.currentRemoteDescription = null;

    // Other properties
    this.clientId = configuration.clientId;
  }

  init() {
    this.internalConnection.init();
  }

  get localDescription() {
    const description = this.pendingLocalDescription ?
      this.pendingLocalDescription : this.currentLocalDescription;
    return description;
  }

  get remoteDescription() {
    const description = this.pendingRemoteDescription ?
      this.pendingRemoteDescription : this.currentRemoteDescription;
    return description;
  }

  get id() {
    return this.internalConnection.id;
  }

  get onGathered() {
    return this.internalConnection.onGathered;
  }

  get onInitialized() {
    return this.internalConnection.onInitialized;
  }

  get onStarted() {
    return this.internalConnection.onStarted;
  }

  get onReady() {
    return this.internalConnection.onReady;
  }

  get mediaConfiguration() {
    return this.internalConnection.mediaConfiguration;
  }

  set mediaConfiguration(newConfiguration) {
    this.internalConnection.mediaConfiguration = newConfiguration;
  }

  get qualityLevel() {
    return this.internalConnection.qualityLevel;
  }

  get metadata() {
    return this.internalConnection.metadata;
  }

  // Steps from https://w3c.github.io/webrtc-pc/#dfn-creating-an-offer
  async chainedCreateOffer() {
    // Step 1
    if (this.signalingState !== 'stable' && this.signalingState !== 'have-local-offer') {
      throw new Error('InvalidStateError');
    }

    // Begin of https://w3c.github.io/webrtc-pc/#dfn-in-parallel-steps-to-create-an-offer
    // Steps 1 to 3
    // Not implemented

    // Step 4
    await queueTask();

    // Begin of https://w3c.github.io/webrtc-pc/#dfn-final-steps-to-create-an-offer
    // Step 1
    if (this.isClosed) {
      return {};
    }

    // Steps 2 to 4 (Partially implemented)
    const offer = await this.internalConnection.createOffer();

    // Step 5
    this.lastCreatedOffer = offer.sdp;

    // Step 6
    return offer;
  }

  // Steps from https://w3c.github.io/webrtc-pc/#dom-rtcpeerconnection-createoffer
  createOffer() {
    // Step 2
    if (this.isClosed) {
      return Promise.reject(new Error('InvalidStateError'));
    }

    // Step 3
    return this.addToOperationChain(this.chainedCreateOffer.bind(this));
  }

  // Steps from https://w3c.github.io/webrtc-pc/#dfn-creating-an-answer
  async chainedCreateAnswer() {
    // Step 1
    if (this.signalingState !== 'have-remote-offer') {
      throw new Error('InvalidStateError');
    }

    // Begin of https://w3c.github.io/webrtc-pc/#dfn-in-parallel-steps-to-create-an-answer
    // Steps 1 to 3
    // Not implemented

    // Step 4
    await queueTask();

    // Begin of https://w3c.github.io/webrtc-pc/#dfn-final-steps-to-create-an-answer
    // Step 1
    if (this.isClosed) {
      return {};
    }

    // Steps 2 to 4 (Partially implemented)
    const answer = await this.internalConnection.createAnswer();

    // Step 5
    this.lastCreatedAnswer = answer.sdp;

    // Step 6
    return answer;
  }

  // Steps from https://w3c.github.io/webrtc-pc/#dom-rtcpeerconnection-createanswer
  createAnswer() {
    // Step 2
    if (this.isClosed) {
      return Promise.reject(new Error('InvalidStateError'));
    }

    // Step 3
    return this.addToOperationChain(this.chainedCreateAnswer.bind(this));
  }

  // Step 4 from https://w3c.github.io/webrtc-pc/#dom-peerconnection-setlocaldescription
  async chainedSetLocalDescription(description = {}) {
    let sdp = description.sdp;

    // Step 4.1
    let type = description.type;
    if (!type) {
      if (['stable', 'have-local-offer'].indexOf(this.signalingState) !== -1) {
        type = 'offer';
      } else {
        type = 'answer';
      }
    }

    // Step 4.2
    if (type === 'offer' && !isEmpty(sdp) && sdp !== this.lastCreatedOffer) {
      throw new Error('InvalidModificationError');
    }

    // Step 4.3
    if (type === 'answer' && !isEmpty(sdp) && sdp !== this.lastCreatedAnswer) {
      throw new Error('InvalidModificationError');
    }

    // Step 4.4
    if (isEmpty(sdp) && type === 'offer') {
      sdp = this.lastCreatedOffer;
      if (isEmpty(sdp) || this.negotiationNeeded) {
        const offer = await this.chainedCreateOffer();
        return this.setLocalSessionDescription(offer);
      }
    }

    // Step 4.5
    if (isEmpty(sdp) && type === 'answer') {
      sdp = this.lastCreatedAnswer;
      if (isEmpty(sdp) || this.negotiationNeeded) {
        const answer = await this.internalConnection.createAnswer();
        return this.setLocalSessionDescription({ type, sdp: answer.sdp });
      }
    }

    // Step 4.6
    return this.setLocalSessionDescription({ type, sdp });
  }

  // Steps from https://w3c.github.io/webrtc-pc/#dom-peerconnection-setlocaldescription
  setLocalDescription(description) {
    return this.addToOperationChain(this.chainedSetLocalDescription.bind(this, description));
  }

  // Step 3 from https://w3c.github.io/webrtc-pc/#dom-peerconnection-setremotedescription
  async chainedSetRemoteDescription(description) {
    // Step 3.1
    // https://tools.ietf.org/html/rfc8829#section-5.6
    if (description.type === 'offer' &&
        ['stable', 'have-remote-offer'].indexOf(this.signalingState) === -1) {
      throw new Error('Rollback needed and not implemented');
    }

    // Step 3.1
    // https://tools.ietf.org/html/rfc8829#section-5.6
    if (description.type === 'answer' &&
        ['have-local-offer', 'have-remote-pranswer'].indexOf(this.signalingState) === -1) {
      throw new Error('Rollback needed and not implemented');
    }

    // Step 3.2
    return this.setRemoteSessionDescription(description);
  }

  // Steps from https://w3c.github.io/webrtc-pc/#dom-peerconnection-setremotedescription
  setRemoteDescription(description) {
    return this.addToOperationChain(this.chainedSetRemoteDescription.bind(this, description));
  }

  // Steps from https://w3c.github.io/webrtc-pc/#set-local-description
  async setLocalSessionDescription(description) {
    return this.setSessionDescription(description, false);
  }

  // Steps from https://w3c.github.io/webrtc-pc/#set-remote-description
  async setRemoteSessionDescription(description) {
    return this.setSessionDescription(description, true);
  }

  // Steps from https://w3c.github.io/webrtc-pc/#set-description
  async setSessionDescription(description, remote) {
    const initialSignalingState = this.signalingState;
    // Step 2
    if (description.type === 'rollback' && this.signalingState === 'stable') {
      throw new Error('InvalidStateError');
    }

    // Rollback mechanism is not implemented
    if (description.type === 'rollback') {
      throw new Error('NotImplemented');
    }

    // Step 3
    // Not implemented

    // Step 4
    await queueTask();
    try {
      if (remote) {
        // Steps from https://tools.ietf.org/html/draft-ietf-rtcweb-jsep-25#section-5.6
        if (description.type === 'offer' && ['stable', 'have-remote-offer'].indexOf(this.signalingState) === -1) {
          throw new Error(`OperationError, state: ${this.signalingState}, descriptionType: ${description.type}`);
        }
        if (description.type === 'answer' && ['have-remote-pranswer', 'have-local-offer'].indexOf(this.signalingState) === -1) {
          throw new Error(`OperationError, state: ${this.signalingState}, descriptionType: ${description.type}`);
        }
        await this.internalConnection.setRemoteDescription(description);
      } else {
        // Steps from https://tools.ietf.org/html/draft-ietf-rtcweb-jsep-25#section-5.5
        if (description.type === 'offer' && ['stable', 'have-local-offer'].indexOf(this.signalingState) === -1) {
          throw new Error(`OperationError, state: ${this.signalingState}, descriptionType: ${description.type}`);
        }
        if (description.type === 'answer' && ['have-local-pranswer', 'have-remote-offer'].indexOf(this.signalingState) === -1) {
          throw new Error(`OperationError, state: ${this.signalingState}, descriptionType: ${description.type}`);
        }
        await this.internalConnection.setLocalDescription(description);
      }
    } catch (e) {
      const stack = e && e.stack && e.stack.replace(/\n/g, '\\n').replace(/\s/g, '').replace(/:/g, '.');
      const message = e && e.message;
      log.error(`message: operation error in RTCPeerConnection, clientId: ${this.clientId}, connectionId: ${this.id}, error: ${message}, stack: ${stack}`);
      // Step 4.4.1
      if (this.isClosed) {
        return;
      }

      // Steps 4.4.2 to 4.4.6
      // Not implemented

      // Step 4.4.7
      throw new Error('OperationError');
    }

    // Step 5.1
    if (this.isClosed) {
      return;
    }

    // Steps 5.2 and 5.3
    // Not implemented


    if (!remote) {
      // Step 5.4.1
      if (description.type === 'offer') {
        this.pendingLocalDescription = description.sdp;
        this.signalingState = 'have-local-offer';
      }

      // Step 5.4.2
      if (description.type === 'answer') {
        this.currentLocalDescription = description.sdp;
        this.currentRemoteDescription = this.pendingRemoteDescription;
        this.pendingLocalDescription = null;
        this.pendingRemoteDescription = null;
        this.lastCreatedOffer = '';
        this.lastCreatedAnswer = '';
        this.signalingState = 'stable';
      }

      // Step 5.4.3
      // Not implemented
    } else {
      // Remote Description
      // Step 5.5.1
      if (description.type === 'offer') {
        this.pendingRemoteDescription = description;
        this.signalingState = 'have-remote-offer';
      }

      // Step 5.5.2
      if (description.type === 'answer') {
        this.currentRemoteDescription = description;
        this.currentLocalDescription = this.pendingLocalDescription;
        this.pendingRemoteDescription = null;
        this.pendingLocalDescription = null;
        this.lastCreatedAnswer = '';
        this.lastCreatedOffer = '';
        this.signalingState = 'stable';
      }

      // Step 5.5.3
      // Not implemented
    }

    // Steps 5.6 to 5.11
    // Not implemented

    // Step 5.12
    if (this.signalingState === 'stable') {
      this.negotiationNeeded = false;
      await this.updateNegotiationNeededFlag();
    }

    // Step 5.13
    if (this.signalingState !== initialSignalingState) {
      this.emit('signalingstatechange');
    }

    // Steps 5.14 to 5.18
    // Not implemented
  }

  async addIceCandidate(candidate) {
    // Step 3
    if (isEmpty(candidate.candidate) && !candidate.sdpMid && !candidate.sdpMLineIndex) {
      throw new Error('TypeError');
    }

    return this.addToOperationChain(this.chainedAddIceCandidate.bind(this, candidate));
  }

  // Steps from https://w3c.github.io/webrtc-pc/#dom-peerconnection-addicecandidate
  async chainedAddIceCandidate(candidate) {
    // Step 4.1
    if (!this.remoteDescription) {
      throw new Error('InvalidStateError');
    }

    // Steps 4.2 to 4.5
    // Not implemented

    // Step 4.7.1
    // Not implemented

    // Step 4.7.2
    await queueTask();

    // Step 4.7.2.1
    if (this.isClosed) {
      return;
    }

    // Step 4.7.2.2 and 4.7.2.3
    if (this.pendingRemoteDescription ||
        this.currentRemoteDescription) {
      await this.internalConnection.addIceCandidate(candidate.candidate);
    }
  }

  // Steps from https://w3c.github.io/webrtc-pc/#dfn-update-the-negotiation-needed-flag
  async updateNegotiationNeededFlag() {
    // Step 1
    if (this.operations.length !== 0) {
      this.updateNegotiationNeededFlagOnEmptyChain = true;
      return;
    }

    // Step 2
    await queueTask();

    // Step 2.1
    if (this.isClosed) {
      return;
    }

    // Step 2.2
    if (this.operations.length !== 0) {
      this.updateNegotiationNeededFlagOnEmptyChain = true;
      return;
    }

    // Step 2.3
    if (this.signalingState !== 'stable') {
      return;
    }

    // Step 2.4
    const negotiationIsNeeded = await this.checkIfNegotiationIsNeeded();

    if (!negotiationIsNeeded) {
      this.negotiationNeeded = false;
      return;
    }

    // Step 2.5
    if (this.negotiationNeeded) {
      return;
    }


    // Step 2.6
    this.negotiationNeeded = true;

    // Step 2.7
    this.emit('negotiationneeded');
  }

  // Steps from https://w3c.github.io/webrtc-pc/#dfn-check-if-negotiation-is-needed
  async checkIfNegotiationIsNeeded() {
    // Steps 1
    if (this.internalConnection.isNegotiationNeeded()) {
      return true;
    }

    // Steps 2 to 5
    // Not implemented

    // Step 6
    return false;
  }

  // Steps from https://w3c.github.io/webrtc-pc/#dfn-close-the-connection
  async closeAlgorithm(/* disappear */) {
    // Step 1
    if (this.isClosed) {
      return;
    }

    // Step 2
    this.isClosed = true;

    // Step 3
    this.signalingState = 'closed';

    // Step 4 to 11 (only partially implemented)
    await this.internalConnection.close();
  }

  // Steps from https://w3c.github.io/webrtc-pc/#dom-rtcpeerconnection-close
  async close() {
    // Step 2
    await this.closeAlgorithm(false);
  }

  saveOperation(operation, resolve, reject) {
    let value;
    let resolved;
    operation().then((result) => {
      resolved = true;
      value = result;
    }).catch((result) => {
      resolved = false;
      value = result;
    }).finally(() => {
      // Step 7.1
      if (this.isClosed) {
        // This is not part of the standard but we need to
        // reject the promises anyway.
        reject(new Error('InvalidStateError'));
        return;
      }
      if (resolved) {
        // Step 7.2
        resolve(value);
      } else {
        // Step 7.3
        reject(value);
      }
    });
  }

  // Steps from https://w3c.github.io/webrtc-pc/#dfn-chain
  addToOperationChain(operation) {
    // Step 2
    if (this.isClosed) {
      return Promise.reject(new Error('InvalidStateError'));
    }

    // Step 4
    const p = new Promise((resolve, reject) => {
      // Step 5
      this.operations.push(this.saveOperation.bind(this, operation, resolve, reject));

      // Step 6
      if (this.operations.length === 1) {
        this.operations[0]();
      }
    });

    const finalSteps = () => {
      // Step 7.4.1
      if (this.isClosed) {
        return;
      }

      // Step 7.4.2
      this.operations.shift();

      // Step 7.4.3
      if (this.operations.length > 0) {
        this.operations[0]();
      }

      // Step 7.4.4
      if (!this.updateNegotiationNeededFlagOnEmptyChain) {
        return;
      }

      this.updateNegotiationNeededFlagOnEmptyChain = false;
      this.updateNegotiationNeededFlag();
    };

    p.then(finalSteps, finalSteps);

    return p;
  }

  async addStream(id, options, isPublisher) {
    if (this.isClosed) {
      throw new Error('InvalidStateError');
    }
    if (await this.internalConnection.addStream(id, options, isPublisher)) {
      this.updateNegotiationNeededFlag();
    }
  }

  async removeStream(id, requestId) {
    if (this.isClosed) {
      throw new Error('InvalidStateError');
    }
    if (await this.internalConnection.removeStream(id, requestId)) {
      this.updateNegotiationNeededFlag();
    }
  }

  getStream(id) {
    return this.internalConnection.getStream(id);
  }

  getStreams() {
    return this.internalConnection.getStreams();
  }

  getNumStreams() {
    return this.internalConnection.getNumStreams();
  }

  copySdpInfoFromConnection(connection) {
    this.internalConnection.copySdpInfoFromConnection(connection.internalConnection);
  }

  getStats(callback) {
    return this.internalConnection.getStats(callback);
  }

  resetStats() {
    this.internalConnection.resetStats();
  }

  setStreamPriorityStrategy(streamPriorityStrategy) {
    this.internalConnection.setStreamPriorityStrategy(streamPriorityStrategy);
  }

  getDurationDistribution() {
    return this.internalConnection.getDurationDistribution();
  }

  getDelayDistribution() {
    return this.internalConnection.getDelayDistribution();
  }
}

module.exports = RTCPeerConnection;
