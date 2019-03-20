/* global RTCSessionDescription, RTCIceCandidate, RTCPeerConnection */
// eslint-disable-next-line
import SemanticSdp from '../../../common/semanticSdp/SemanticSdp';
import Setup from '../../../common/semanticSdp/Setup';

import SdpHelpers from '../utils/SdpHelpers';
import Logger from '../utils/Logger';
import FunctionQueue from '../utils/FunctionQueue';

import StateMachine from '../../lib/state-machine';

const BaseStack = (specInput) => {
  const that = {};
  const specBase = specInput;
  const negotiationQueue = new FunctionQueue();
  const firstLocalDescriptionQueue = new FunctionQueue();
  let firstLocalDescriptionSet = false;
  let localDesc;
  let remoteDesc;
  let localSdp;
  let remoteSdp;
  let latestSessionVersion = -1;

  Logger.info('Starting Base stack', specBase);

  that.pcConfig = {
    iceServers: [],
    sdpSemantics: 'plan-b',  // WARN: Chrome 72+ will by default use unified-plan
  };

  that.con = {};
  if (specBase.iceServers !== undefined) {
    that.pcConfig.iceServers = specBase.iceServers;
  }
  if (specBase.forceTurn === true) {
    that.pcConfig.iceTransportPolicy = 'relay';
  }
  that.audio = specBase.audio;
  that.video = specBase.video;
  if (that.audio === undefined) {
    that.audio = true;
  }
  if (that.video === undefined) {
    that.video = true;
  }
  specBase.remoteCandidates = [];
  specBase.localCandidates = [];
  specBase.remoteDescriptionSet = false;

  that.mediaConstraints = {
    offerToReceiveVideo: (that.video !== undefined && that.video !== false),
    offerToReceiveAudio: (that.audio !== undefined && that.audio !== false),
  };

  const errorCallback = (where, errorcb, message) => {
    Logger.error('message:', message, 'in baseStack at', where);
    if (errorcb !== undefined) {
      errorcb('error');
    }
    that.peerConnection.fail();
  };

  // Aux functions

  const onIceCandidate = (event) => {
    let candidateObject = {};
    const candidate = event.candidate;
    if (!candidate) {
      Logger.info('Gathered all candidates. Sending END candidate');
      candidateObject = {
        sdpMLineIndex: -1,
        sdpMid: 'end',
        candidate: 'end',
      };
    } else {
      candidateObject = {
        sdpMLineIndex: candidate.sdpMLineIndex,
        sdpMid: candidate.sdpMid,
        candidate: candidate.candidate,
      };
      if (!candidateObject.candidate.match(/a=/)) {
        candidateObject.candidate = `a=${candidateObject.candidate}`;
      }
    }

    if (specBase.remoteDescriptionSet) {
      specBase.callback({ type: 'candidate', candidate: candidateObject });
    } else {
      specBase.localCandidates.push(candidateObject);
      Logger.info('Storing candidate: ', specBase.localCandidates.length, candidateObject);
    }
  };

  const setLocalDescForOffer = (isSubscribe, streamId, sessionDescription) => {
    localDesc = sessionDescription;
    if (!isSubscribe) {
      localDesc.sdp = that.enableSimulcast(localDesc.sdp);
    }
    localSdp = SemanticSdp.SDPInfo.processString(localDesc.sdp);
    SdpHelpers.setMaxBW(localSdp, specBase);
    localDesc.sdp = localSdp.toString();
    that.localSdp = localSdp;

    specBase.callback({
      type: localDesc.type,
      sdp: localDesc.sdp,
      config: { maxVideoBW: specBase.maxVideoBW },
    }, streamId);
  };

  const setLocalDescForAnswer = (streamIds, sessionDescription) => {
    localDesc = sessionDescription;
    localDesc.type = 'answer';
    localSdp = SemanticSdp.SDPInfo.processString(localDesc.sdp);
    SdpHelpers.setMaxBW(localSdp, specBase);
    localDesc.sdp = localSdp.toString();
    that.localSdp = localSdp;
    specBase.callback({
      type: localDesc.type,
      sdp: localDesc.sdp,
      config: { maxVideoBW: specBase.maxVideoBW },
    }, streamIds);
    Logger.info('Setting local description', localDesc);
    Logger.debug('processOffer - Local Description', localDesc.type, localDesc.sdp);
    return that.peerConnection.setLocalDescription(localDesc);
  };

  const configureLocalSdpAsOffer = () => {
    localDesc.type = 'offer';
    localSdp = SemanticSdp.SDPInfo.processString(localDesc.sdp);
    SdpHelpers.setMaxBW(localSdp, specBase);

    localSdp.medias.forEach((media) => {
      if (media.getSetup() !== Setup.ACTPASS) {
        media.setSetup(Setup.ACTPASS);
      }
    });
    localDesc.sdp = localSdp.toString();
    that.localSdp = localSdp;
  };

  const _processOffer = negotiationQueue.protectFunction((message, streamIds) => {
    const msg = message;
    remoteSdp = SemanticSdp.SDPInfo.processString(msg.sdp);

    const sessionVersion = remoteSdp && remoteSdp.origin && remoteSdp.origin.sessionVersion;
    if (latestSessionVersion >= sessionVersion) {
      Logger.warning(`message: processOffer discarding old sdp sessionVersion: ${sessionVersion}, latestSessionVersion: ${latestSessionVersion}`);
      // We send an answer back to finish this negotiation
      specBase.callback({
        type: 'answer',
        sdp: localDesc.sdp,
        config: { maxVideoBW: specBase.maxVideoBW },
      }, streamIds);
      return;
    }
    negotiationQueue.startEnqueuing();
    latestSessionVersion = sessionVersion;

    SdpHelpers.setMaxBW(remoteSdp, specBase);
    msg.sdp = remoteSdp.toString();
    that.remoteSdp = remoteSdp;
    that.peerConnectionStateMachine.processOffer(msg, streamIds);
  });

  const processOffer = firstLocalDescriptionQueue.protectFunction((message, streamIds) => {
    if (!firstLocalDescriptionSet) {
      firstLocalDescriptionQueue.startEnqueuing();
    }
    _processOffer(message, streamIds);
  });

  const processAnswer = negotiationQueue.protectFunction((message) => {
    const msg = message;

    remoteSdp = SemanticSdp.SDPInfo.processString(msg.sdp);
    const sessionVersion = remoteSdp && remoteSdp.origin && remoteSdp.origin.sessionVersion;
    if (latestSessionVersion >= sessionVersion) {
      Logger.warning(`processAnswer discarding old sdp, sessionVersion: ${sessionVersion}, latestSessionVersion: ${latestSessionVersion}`);
      return;
    }
    negotiationQueue.startEnqueuing();
    latestSessionVersion = sessionVersion;
    Logger.info('Set remote and local description');

    SdpHelpers.setMaxBW(remoteSdp, specBase);
    that.setStartVideoBW(remoteSdp);
    that.setHardMinVideoBW(remoteSdp);

    msg.sdp = remoteSdp.toString();

    configureLocalSdpAsOffer();
    Logger.debug('processAnswer - Remote Description', msg.type, msg.sdp);
    Logger.debug('processAnswer - Local Description', msg.type, localDesc.sdp);
    that.remoteSdp = remoteSdp;

    remoteDesc = msg;
    that.peerConnectionStateMachine.processAnswer(msg);
  });

  const processNewCandidate = (message) => {
    const msg = message;
    try {
      let obj;
      if (typeof (msg.candidate) === 'object') {
        obj = msg.candidate;
      } else {
        obj = JSON.parse(msg.candidate);
      }
      if (obj.candidate === 'end') {
        // ignore the end candidate for chrome
        return;
      }
      obj.candidate = obj.candidate.replace(/a=/g, '');
      obj.sdpMLineIndex = parseInt(obj.sdpMLineIndex, 10);
      const candidate = new RTCIceCandidate(obj);
      if (specBase.remoteDescriptionSet) {
        that.peerConnectionStateMachine.addIceCandidate(candidate);
      } else {
        specBase.remoteCandidates.push(candidate);
      }
    } catch (e) {
      Logger.error('Error parsing candidate', msg.candidate);
    }
  };

  const activeStates = ['ready', 'offer-created', 'answer-processed', 'offer-processed', 'spec-updated'];
  // FSM
  const PeerConnectionStateMachine = StateMachine.factory({
    init: 'initial',
    transitions: [
      { name: 'start', from: 'initial', to: 'ready' },
      { name: 'create-offer', from: activeStates, to: 'offer-created' },
      { name: 'add-ice-candidate', from: activeStates, to: function nextState() { return this.state; } },
      { name: 'process-answer', from: activeStates, to: 'answer-processed' },
      { name: 'process-offer', from: activeStates, to: 'offer-processed' },
      { name: 'update-spec', from: activeStates, to: 'spec-updated' },
      { name: 'add-stream', from: activeStates, to: function nextState() { return this.state; } },
      { name: 'remove-stream', from: activeStates, to: function nextState() { return this.state; } },
      { name: 'close', from: activeStates, to: 'closed' },
      { name: 'fail', from: '*', to: 'failed' },
    ],
    methods: {
      getPeerConnection: function getPeerConnection() {
        return this.peerConnection;
      },
      onBeforeStart: function init(lifecycle, pcConfig, con) {
        Logger.info(`FSM onBeforeInit, from ${lifecycle.from}, to: ${lifecycle.to}`);
        this.peerConnection = new RTCPeerConnection(pcConfig, con);
      },
      onBeforeClose: function close(lifecycle) {
        Logger.info(`FSM onBeforeClose, from ${lifecycle.from}, to: ${lifecycle.to}`);
        this.peerConnection.close();
      },
      onBeforeAddIceCandidate(lifecycle, candidate) {
        return new Promise((resolve, reject) => {
          Logger.info(`FSM onBeforeAddIceCandidate, from ${lifecycle.from}, to: ${lifecycle.to}`);
          this.peerConnectionStateMachine.addIceCandidate(candidate, resolve, reject);
        });
      },
      onBeforeAddStream: function onBeforeAddStream(lifecycle, stream) {
        Logger.info(`FSM onBeforeAddStream, from ${lifecycle.from}, to: ${lifecycle.to}`);
        this.peerConnection.addStream(stream);
      },
      onBeforeRemoveStream: function onBeforeRemoveStream(lifecycle, stream) {
        Logger.info(`FSM onBeforeRemoveStream, from ${lifecycle.from}, to: ${lifecycle.to}`);
        this.peerConnection.removeStream(stream);
      },
      onBeforeCreateOffer: function onBeforeCreateOffer(lifecycle, isSubscribe, streamId) {
        return new Promise((resolve, reject) => {
          negotiationQueue.startEnqueuing();
          Logger.debug('FSM onBeforeCreateOffer', that.mediaConstraints, streamId);
          let rejected = false;
          this.peerConnection.createOffer(that.mediaConstraints)
            .then(setLocalDescForOffer.bind(null, isSubscribe, streamId))
            .catch(() => {
              errorCallback('Create Offer', undefined);
              rejected = true;
              reject();
            })
            .then(() => {
              if (!rejected) {
                resolve();
              }
              setTimeout(() => {
                negotiationQueue.stopEnqueuing();
                negotiationQueue.nextInQueue();
              }, 0);
            });
        });
      },
      onBeforeProcessOffer:
      function onBeforeProcessOffer(lifecycle, msg, streamIds) {
        return new Promise((resolve, reject) => {
          let rejected = false;
          that.peerConnection.setRemoteDescription(msg)
            .then(() => {
              specBase.remoteDescriptionSet = true;
            }).then(() => that.peerConnection.createAnswer(that.mediaConstraints))
            .catch(() => {
              errorCallback('createAnswer', undefined);
              rejected = true;
              reject();
            })
            .then(setLocalDescForAnswer.bind(this, streamIds))
            .catch(() => {
              errorCallback('process Offer', undefined);
              rejected = true;
              reject();
            })
            .then(() => {
              firstLocalDescriptionSet = true;
              firstLocalDescriptionQueue.stopEnqueuing();
              firstLocalDescriptionQueue.dequeueAll();
              setTimeout(() => {
                negotiationQueue.stopEnqueuing();
                negotiationQueue.nextInQueue();
              }, 0);
              if (!rejected) {
                resolve();
              }
            });
        });
      },
      onBeforeProcessAnswer:
      function onBeforeProcessAnswer(lifecycle, msg) {
        Logger.info(`FSM onBeforeProcessAnswer, from ${lifecycle.from}, to: ${lifecycle.to}`);
        return new Promise((resolve, reject) => {
          let rejected = false;
          this.peerConnection.setLocalDescription(localDesc)
            .then(() => this.peerConnection.setRemoteDescription(new RTCSessionDescription(msg)))
            .then(() => {
              specBase.remoteDescriptionSet = true;
              Logger.info('FSM Candidates to be added: ', specBase.remoteCandidates.length,
                specBase.remoteCandidates);
              while (specBase.remoteCandidates.length > 0) {
                // IMPORTANT: preserve ordering of candidates
                this.peerConnection.addIceCandidate(specBase.remoteCandidates.shift());
              }
              Logger.info('FSM Local candidates to send:', specBase.localCandidates.length);
              while (specBase.localCandidates.length > 0) {
                // IMPORTANT: preserve ordering of candidates
                specBase.callback({ type: 'candidate', candidate: specBase.localCandidates.shift() });
              }
            })
            .catch(() => {
              errorCallback('processAnswer', undefined);
              rejected = true;
              reject();
            })
            .then(() => {
              firstLocalDescriptionSet = true;
              firstLocalDescriptionQueue.stopEnqueuing();
              firstLocalDescriptionQueue.dequeueAll();
              setTimeout(() => {
                negotiationQueue.stopEnqueuing();
                negotiationQueue.nextInQueue();
              }, 0);
              if (!rejected) {
                resolve();
              }
            });
        });
      },
      onBeforeUpdateSpec:
      function onBeforeUpdateSpec(lifecycle, streamId) {
        return new Promise((resolve, reject) => {
          this.peerConnection.setLocalDescription(localDesc)
            .then(() => {
              remoteSdp = SemanticSdp.SDPInfo.processString(remoteDesc.sdp);
              SdpHelpers.setMaxBW(remoteSdp, specBase);
              remoteDesc.sdp = remoteSdp.toString();
              that.remoteSdp = remoteSdp;
              return this.peerConnection
                .setRemoteDescription(new RTCSessionDescription(remoteDesc));
            }).then(() => {
              specBase.remoteDescriptionSet = true;
              specBase.callback({ type: 'updatestream', sdp: localDesc.sdp }, streamId);
              resolve();
            }).catch(() => {
              errorCallback('updateSpec', undefined);
              reject();
            });
        });
      },
      onAnswerProcessed: function onAnswerProcessed(lifecycle) {
        Logger.info(`FSM onAnswerProcessed set, from ${lifecycle.from}, to: ${lifecycle.to}`);
      },
      onOfferCreated: function onOfferCreated(lifecycle) {
        Logger.info(`FSM onOfferCreated set, from ${lifecycle.from}, to: ${lifecycle.to}`);
      },
      onOfferProcessed: function onOfferProcessed(lifecycle) {
        Logger.info(`FSM onOfferProcessed set, from ${lifecycle.from}, to: ${lifecycle.to}`);
      },
      onSpecUpdated: function onSpecUpdated(lifecycle) {
        Logger.info(`FSM onSpecUpdated, from ${lifecycle.from}, to: ${lifecycle.to}`);
      },
      onReady: function onReady(lifecycle) {
        Logger.info(`FSM onReady, from ${lifecycle.from}, to: ${lifecycle.to}`);
      },
      onClosed: function onClosed(lifecycle) {
        Logger.info(`FSM onClose, from ${lifecycle.from}, to: ${lifecycle.to}`);
      },
      onInvalidTransition: function onInvalidTransition(transition, from, to) {
        Logger.error('Invalid transition', transition, from, to);
        errorCallback('invalidTransition', undefined);
      },
      onPendingTransition: function onPendingTransition(transition, from, to) {
        Logger.error('Pending transition', transition, from, to);
        errorCallback('pendingTransition', undefined);
      },
      onError: function onError(lifecycle) {
        Logger.error(`I errored from: ${lifecycle.from}, to: ${lifecycle.to}`);
        errorCallback('fsmError', undefined);
      },
    },
  });

  that.createPeerConnection = (pcConfig, con) => {
    that.peerConnectionStateMachine = new PeerConnectionStateMachine();
    that.peerConnectionStateMachine.start(pcConfig, con);
    that.peerConnection = that.peerConnectionStateMachine.getPeerConnection();
  };


  that.createPeerConnection(that.pcConfig, that.con);
  // Peerconnection events

  that.peerConnection.onicecandidate = onIceCandidate;

  // public functions

  that.setStartVideoBW = (sdpInput) => {
    Logger.error('startVideoBW not implemented for this browser');
    return sdpInput;
  };

  that.setHardMinVideoBW = (sdpInput) => {
    Logger.error('hardMinVideoBw not implemented for this browser');
    return sdpInput;
  };

  that.enableSimulcast = (sdpInput) => {
    Logger.error('Simulcast not implemented');
    return sdpInput;
  };

  that.close = () => {
    that.state = 'closed';
    that.peerConnectionStateMachine.close();
  };

  that.setSimulcast = (enable) => {
    that.simulcast = enable;
  };

  that.setVideo = (video) => {
    that.video = video;
  };

  that.setAudio = (audio) => {
    that.audio = audio;
  };

  that.updateSpec = (configInput, streamId, callback = () => {}) => {
    const config = configInput;
    const shouldApplyMaxVideoBWToSdp = specBase.p2p && config.maxVideoBW;
    const shouldSendMaxVideoBWInOptions = !specBase.p2p && config.maxVideoBW;
    if (shouldApplyMaxVideoBWToSdp || config.maxAudioBW) {
      if (config.maxVideoBW) {
        Logger.debug('Maxvideo Requested:', config.maxVideoBW,
                                'limit:', specBase.limitMaxVideoBW);
        if (config.maxVideoBW > specBase.limitMaxVideoBW) {
          config.maxVideoBW = specBase.limitMaxVideoBW;
        }
        specBase.maxVideoBW = config.maxVideoBW;
        Logger.debug('Result', specBase.maxVideoBW);
      }
      if (config.maxAudioBW) {
        if (config.maxAudioBW > specBase.limitMaxAudioBW) {
          config.maxAudioBW = specBase.limitMaxAudioBW;
        }
        specBase.maxAudioBW = config.maxAudioBW;
      }

      localSdp = SemanticSdp.SDPInfo.processString(localDesc.sdp);
      SdpHelpers.setMaxBW(localSdp, specBase);
      localDesc.sdp = localSdp.toString();
      that.localSdp = localSdp;

      if (config.Sdp || config.maxAudioBW) {
        Logger.debug('Updating with SDP renegotiation', specBase.maxVideoBW, specBase.maxAudioBW);
        that.peerConnectionStateMachine.updateSpec(streamId, callback);
      } else {
        Logger.debug('Updating without SDP renegotiation, ' +
                     'newVideoBW:', specBase.maxVideoBW,
                     'newAudioBW:', specBase.maxAudioBW);
        specBase.callback({ type: 'updatestream', sdp: localDesc.sdp }, streamId);
      }
    }
    if (shouldSendMaxVideoBWInOptions ||
        config.minVideoBW ||
        (config.slideShowMode !== undefined) ||
        (config.muteStream !== undefined) ||
        (config.qualityLayer !== undefined) ||
        (config.slideShowBelowLayer !== undefined) ||
        (config.video !== undefined)) {
      Logger.debug('MaxVideoBW Changed to ', config.maxVideoBW);
      Logger.debug('MinVideo Changed to ', config.minVideoBW);
      Logger.debug('SlideShowMode Changed to ', config.slideShowMode);
      Logger.debug('muteStream changed to ', config.muteStream);
      Logger.debug('Video Constraints', config.video);
      Logger.debug('Will activate slideshow when below layer', config.slideShowBelowLayer);
      specBase.callback({ type: 'updatestream', config }, streamId);
    }
  };

  const _createOfferOnPeerConnection = negotiationQueue.protectFunction((isSubscribe = false, streamId = '') => {
    that.peerConnectionStateMachine.createOffer(isSubscribe, streamId);
  });

  // We need to protect it against calling multiple times to createOffer.
  // Otherwise it could change the ICE credentials before calling setLocalDescription
  // the first time in Chrome.
  that.createOffer = firstLocalDescriptionQueue.protectFunction((isSubscribe = false, forceOfferToReceive = false, streamId = '') => {
    if (!firstLocalDescriptionSet) {
      firstLocalDescriptionQueue.startEnqueuing();
    }

    if (!isSubscribe && !forceOfferToReceive) {
      that.mediaConstraints = {
        offerToReceiveVideo: false,
        offerToReceiveAudio: false,
      };
    }
    _createOfferOnPeerConnection(isSubscribe, streamId);
  });

  that.addStream = (stream) => {
    that.peerConnectionStateMachine.addStream(stream);
  };

  that.removeStream = (stream) => {
    that.peerConnectionStateMachine.removeStream(stream);
  };

  that.processSignalingMessage = (msgInput, streamIds) => {
    Logger.info(`processSignalingMessage type ${msgInput.type}`);
    if (msgInput.type === 'offer') {
      processOffer(msgInput, streamIds);
    } else if (msgInput.type === 'answer') {
      processAnswer(msgInput);
    } else if (msgInput.type === 'candidate') {
      processNewCandidate(msgInput);
    }
  };

  return that;
};

export default BaseStack;
