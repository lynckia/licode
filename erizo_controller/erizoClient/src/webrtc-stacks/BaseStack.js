/* global RTCSessionDescription, RTCIceCandidate, RTCPeerConnection */

// eslint-disable-next-line
import SemanticSdp from '../../../common/semanticSdp/SemanticSdp';
import Setup from '../../../common/semanticSdp/Setup';
import Direction from '../../../common/semanticSdp/Direction';

import PeerConnectionFsm from './PeerConnectionFsm';

import SdpHelpers from '../utils/SdpHelpers';
import Logger from '../utils/Logger';
import FunctionQueue from '../utils/FunctionQueue';

const log = Logger.module('BaseStack');
const NEGOTIATION_TIMEOUT = 30000;

const BaseStack = (specInput) => {
  const that = {};
  const specBase = specInput;
  const negotiationQueue = new FunctionQueue(NEGOTIATION_TIMEOUT, () => {
    if (specBase.onEnqueueingTimeout) {
      specBase.onEnqueueingTimeout();
    }
  });
  that._queue = negotiationQueue;
  let localDesc;
  let remoteDesc;
  let localSdp;
  let remoteSdp;
  let latestSessionVersion = -1;

  const logs = [];
  const logSDP = (...message) => {
    logs.push(['Negotiation:', ...message].reduce((a, b) => `${a} ${b}`));
  };
  that.getNegotiationLogs = () => logs.reduce((a, b) => `${a}'\n'${b}`);

  log.debug(`message: Starting Base stack, spec: ${JSON.stringify(specBase)}`);

  that.pcConfig = {
    iceServers: [],
    sdpSemantics: 'plan-b', // WARN: Chrome 72+ will by default use unified-plan
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

  const onFsmError = (message) => {
    that.peerConnectionFsm.error(message);
  };

  that.peerConnection = new RTCPeerConnection(that.pcConfig, that.con);
  let negotiationneededCount = 0;
  that.peerConnection.onnegotiationneeded = () => { // one per media which is added
    let medias = that.audio ? 1 : 0;
    medias += that.video ? 1 : 0;
    if (negotiationneededCount % medias === 0) {
      logSDP('onnegotiationneeded - createOffer');
      const promise = that.peerConnectionFsm.createOffer(false);
      if (promise) {
        promise.catch(onFsmError.bind(this));
      }
    }
    negotiationneededCount += 1;
  };

  const configureLocalSdpAsAnswer = () => {
    localDesc.sdp = that.enableSimulcast(localDesc.sdp);
    localDesc.type = 'answer';
    localSdp = SemanticSdp.SDPInfo.processString(localDesc.sdp);
    SdpHelpers.setMaxBW(localSdp, specBase);

    const numberOfRemoteMedias = that.remoteSdp.getStreams().size;
    const numberOfLocalMedias = localSdp.getStreams().size;

    let direction = Direction.SENDRECV;
    if (numberOfRemoteMedias > 0 && numberOfLocalMedias > 0) {
      direction = Direction.SENDRECV;
    } else if (numberOfRemoteMedias > 0 && numberOfLocalMedias === 0) {
      direction = Direction.RECVONLY;
    } else if (numberOfRemoteMedias === 0 && numberOfLocalMedias > 0) {
      direction = Direction.SENDONLY;
    } else {
      direction = Direction.INACTIVE;
    }
    localSdp.getMedias().forEach((media) => {
      media.setDirection(direction);
    });

    localDesc.sdp = localSdp.toString();
    that.localSdp = localSdp;
  };

  const configureLocalSdpAsOffer = () => {
    localDesc.sdp = that.enableSimulcast(localDesc.sdp);
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

  const setLocalDescForOffer = (isSubscribe, sessionDescription) => {
    localDesc = sessionDescription;

    configureLocalSdpAsOffer();

    specBase.callback({
      type: localDesc.type,
      sdp: localDesc.sdp,
      receivedSessionVersion: latestSessionVersion,
      config: { maxVideoBW: specBase.maxVideoBW },
    });
  };

  const setLocalDescForAnswer = (sessionDescription) => {
    localDesc = sessionDescription;
    configureLocalSdpAsAnswer();
    specBase.callback({
      type: localDesc.type,
      sdp: localDesc.sdp,
      receivedSessionVersion: latestSessionVersion,
      config: { maxVideoBW: specBase.maxVideoBW },
    });
    log.debug(`message: Setting local description, localDesc: ${JSON.stringify(localDesc)}`);
    logSDP('processOffer - Local Description', localDesc.type);
    return that.peerConnection.setLocalDescription(localDesc).then(() => {
      that.setSimulcastLayersConfig();
    });
  };

  // Functions that are protected by a functionQueue
  that.enqueuedCalls = {
    negotiationQueue: {
      createOffer: negotiationQueue.protectFunction((isSubscribe = false,
        forceOfferToReceive = false) => {
        logSDP('queue - createOffer');
        negotiationQueue.startEnqueuing();
        if (!isSubscribe && !forceOfferToReceive) {
          that.mediaConstraints = {
            offerToReceiveVideo: false,
            offerToReceiveAudio: false,
          };
        }
        const promise = that.peerConnectionFsm.createOffer(isSubscribe);
        if (promise) {
          promise.catch(onFsmError.bind(this));
        } else {
          negotiationQueue.stopEnqueuing();
          negotiationQueue.nextInQueue();
        }
      }),

      processOffer: negotiationQueue.protectFunction((message) => {
        logSDP('queue - processOffer');
        negotiationQueue.startEnqueuing();
        const promise = that.peerConnectionFsm.processOffer(message);
        if (promise) {
          promise.catch(onFsmError.bind(this));
        } else {
          negotiationQueue.stopEnqueuing();
          negotiationQueue.nextInQueue();
        }
      }),

      negotiateMaxBW: negotiationQueue.protectFunction((configInput, callback) => {
        logSDP('queue - negotiateMaxBW');
        const promise = that.peerConnectionFsm.negotiateMaxBW(configInput, callback);
        if (promise) {
          promise.catch(onFsmError.bind(this));
        } else {
          negotiationQueue.nextInQueue();
        }
      }),

      processNewCandidate: negotiationQueue.protectFunction((message) => {
        logSDP('queue - processNewCandidate');
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
            negotiationQueue.nextInQueue();
            return;
          }
          obj.candidate = obj.candidate.replace(/a=/g, '');
          obj.sdpMLineIndex = parseInt(obj.sdpMLineIndex, 10);
          const candidate = new RTCIceCandidate(obj);
          if (specBase.remoteDescriptionSet) {
            negotiationQueue.startEnqueuing();
            that.peerConnectionFsm.addIceCandidate(candidate).catch(onFsmError.bind(this));
          } else {
            specBase.remoteCandidates.push(candidate);
          }
        } catch (e) {
          log.error(`message: Error parsing candidate, candidate: ${msg.candidate}, message: ${e.message}`);
        }
      }),

      addStream: negotiationQueue.protectFunction((stream) => {
        logSDP('queue - addStream');
        negotiationQueue.startEnqueuing();
        const promise = that.peerConnectionFsm.addStream(stream);
        if (promise) {
          promise.catch(onFsmError.bind(this));
        } else {
          negotiationQueue.stopEnqueuing();
          negotiationQueue.nextInQueue();
        }
      }),

      removeStream: negotiationQueue.protectFunction((stream) => {
        logSDP('queue - removeStream');
        negotiationQueue.startEnqueuing();
        const promise = that.peerConnectionFsm.removeStream(stream);
        if (promise) {
          promise.catch(onFsmError.bind(this));
        } else {
          negotiationQueue.stopEnqueuing();
          negotiationQueue.nextInQueue();
        }
      }),

      close: negotiationQueue.protectFunction(() => {
        logSDP('queue - close');
        negotiationQueue.startEnqueuing();
        const promise = that.peerConnectionFsm.close();
        if (promise) {
          promise.catch(onFsmError.bind(this));
        } else {
          negotiationQueue.stopEnqueuing();
          negotiationQueue.nextInQueue();
        }
      }),
    },
  };

  // Functions that are protected by the FSM.
  // The promise of one has to be resolved before another can be called.
  that.protectedCalls = {
    protectedAddStream: (stream) => {
      try {
        that.peerConnection.addStream(stream);
      } catch (e) {
        setTimeout(() => {
          negotiationQueue.stopEnqueuing();
          negotiationQueue.nextInQueue();
        }, 0);
      }
      return Promise.resolve();
    },

    protectedRemoveStream: (stream) => {
      try {
        that.peerConnection.removeStream(stream);
        setTimeout(() => {
          negotiationQueue.stopEnqueuing();
          negotiationQueue.nextInQueue();
        }, 0);
      } catch (e) {
        setTimeout(() => {
          negotiationQueue.stopEnqueuing();
          negotiationQueue.nextInQueue();
        }, 0);
      }
      return Promise.resolve();
    },

    protectedCreateOffer: (isSubscribe = false) => {
      negotiationQueue.startEnqueuing();
      logSDP('Creating offer', that.mediaConstraints);
      const rejectMessages = [];
      return that.prepareCreateOffer(isSubscribe)
        .then(() => that.peerConnection.createOffer(that.mediaConstraints))
        .then(setLocalDescForOffer.bind(null, isSubscribe))
        .catch((error) => {
          rejectMessages.push(`in protectedCreateOffer-createOffer, error: ${error}`);
        })
        .then(() => {
          if (rejectMessages.length !== 0) {
            return Promise.reject(rejectMessages);
          }
          return Promise.resolve();
        });
    },

    protectedProcessOffer: (message) => {
      log.debug(`message: Protected process Offer, message: ${message}, localDesc: ${JSON.stringify(localDesc)}`);
      const msg = message;
      remoteSdp = SemanticSdp.SDPInfo.processString(msg.sdp);

      const sessionVersion = remoteSdp && remoteSdp.origin && remoteSdp.origin.sessionVersion;
      if (latestSessionVersion >= sessionVersion) {
        log.warning('message: processOffer discarding old sdp' +
          `, sessionVersion: ${sessionVersion}, latestSessionVersion: ${latestSessionVersion}`);
        // We send an Offer-dropped message to let the other end start the negotiation again
        logSDP('processOffer - dropped');
        specBase.callback({
          type: 'offer-dropped',
        });
        setTimeout(() => {
          negotiationQueue.stopEnqueuing();
          negotiationQueue.nextInQueue();
        }, 0);
        return Promise.resolve();
      }
      latestSessionVersion = sessionVersion;

      SdpHelpers.setMaxBW(remoteSdp, specBase);

      // Hack to ensure that the offer has the right setup.
      remoteSdp.medias.forEach((media) => {
        if (media.getSetup() !== Setup.ACTPASS) {
          media.setSetup(Setup.ACTPASS);
        }
      });

      msg.sdp = remoteSdp.toString();
      that.remoteSdp = remoteSdp;
      const rejectMessage = [];
      logSDP('processOffer - Remote Description', msg.type);
      return that.peerConnection.setRemoteDescription(msg)
        .then(() => {
          specBase.remoteDescriptionSet = true;
          logSDP('processOffer - Create Answer');
        }).then(() => that.peerConnection.createAnswer(that.mediaConstraints))
        .catch((error) => {
          rejectMessage.push(`in: protectedProcessOffer-createAnswer, error: ${error}`);
        })
        .then(setLocalDescForAnswer.bind(this))
        .catch((error) => {
          rejectMessage.push(`in: protectedProcessOffer-setLocalDescForAnswer, error: ${error}`);
        })
        .then(() => {
          logSDP('processOffer - Stop enqueueing');
          setTimeout(() => {
            negotiationQueue.stopEnqueuing();
            negotiationQueue.nextInQueue();
          }, 0);
          if (rejectMessage.length !== 0) {
            return Promise.reject(rejectMessage);
          }
          return Promise.resolve();
        });
    },

    protectedProcessAnswer: (message) => {
      const msg = message;

      remoteSdp = SemanticSdp.SDPInfo.processString(msg.sdp);
      const sessionVersion = remoteSdp && remoteSdp.origin && remoteSdp.origin.sessionVersion;
      if (latestSessionVersion >= sessionVersion) {
        log.warning('message: processAnswer discarding old sdp' +
          `, sessionVersion: ${sessionVersion}, latestSessionVersion: ${latestSessionVersion}`);
        logSDP('processAnswer - dropped');
        specBase.callback({ type: 'answer-dropped' });
        setTimeout(() => {
          negotiationQueue.stopEnqueuing();
          negotiationQueue.nextInQueue();
        }, 0);
        return Promise.resolve();
      }
      latestSessionVersion = sessionVersion;
      log.debug('message: Set remote and local description');

      SdpHelpers.setMaxBW(remoteSdp, specBase);
      that.setStartVideoBW(remoteSdp);
      that.setHardMinVideoBW(remoteSdp);

      msg.sdp = remoteSdp.toString();

      configureLocalSdpAsOffer();

      logSDP('processAnswer - Local Description', localDesc.type);
      that.remoteSdp = remoteSdp;

      remoteDesc = msg;
      const rejectMessages = [];
      return that.peerConnection.setLocalDescription(localDesc)
        .then(() => {
          that.setSimulcastLayersConfig();
          logSDP('processAnswer - Remote Description', msg.type);
          that.peerConnection.setRemoteDescription(new RTCSessionDescription(msg));
        })
        .then(() => {
          specBase.remoteDescriptionSet = true;
          log.debug(`message: Candidates to be added, size: ${specBase.remoteCandidates.length}`);
          while (specBase.remoteCandidates.length > 0) {
            // IMPORTANT: preserve ordering of candidates
            that.peerConnectionFsm.addIceCandidate(specBase.remoteCandidates.shift())
              .catch(onFsmError.bind(this));
          }
          log.debug(`message: Local candidates to send, size: ${specBase.localCandidates.length}`);
          while (specBase.localCandidates.length > 0) {
            // IMPORTANT: preserve ordering of candidates
            specBase.callback({ type: 'candidate', candidate: specBase.localCandidates.shift() });
          }
        })
        .catch((error) => {
          logSDP('precessAnswer - error', error);
          rejectMessages.push(`in: protectedProcessAnswer, error: ${error}`);
        })
        .then(() => {
          logSDP('processAnswer - Stop enqueuing');
          setTimeout(() => {
            negotiationQueue.stopEnqueuing();
            negotiationQueue.nextInQueue();
          }, 0);
          if (rejectMessages.length !== 0) {
            return Promise.reject(rejectMessages);
          }
          return Promise.resolve();
        });
    },

    protectedNegotiateMaxBW: (configInput, callback) => {
      const config = configInput;
      if (config.Sdp || config.maxAudioBW) {
        negotiationQueue.startEnqueuing();
        const rejectMessages = [];

        configureLocalSdpAsOffer();
        logSDP('protectedNegotiateBW - Local Description', localDesc.type);
        that.peerConnection.setLocalDescription(localDesc)
          .then(() => {
            that.setSimulcastLayersConfig();
            remoteSdp = SemanticSdp.SDPInfo.processString(remoteDesc.sdp);
            SdpHelpers.setMaxBW(remoteSdp, specBase);
            remoteDesc.sdp = remoteSdp.toString();
            that.remoteSdp = remoteSdp;
            logSDP('protectedNegotiateBW - Remote Description', remoteDesc.type);
            return that.peerConnection.setRemoteDescription(
              new RTCSessionDescription(remoteDesc));
          }).then(() => {
            specBase.remoteDescriptionSet = true;
            specBase.callback({ type: 'offer-noanswer', sdp: localDesc.sdp, receivedSessionVersion: latestSessionVersion });
          }).catch((error) => {
            callback('error', 'updateSpec');
            rejectMessages.push(`in: protectedNegotiateMaxBW error: ${error}`);
          })
          .then(() => {
            setTimeout(() => {
              negotiationQueue.stopEnqueuing();
              negotiationQueue.nextInQueue();
            }, 0);
            if (rejectMessages.length !== 0) {
              return Promise.reject(rejectMessages);
            }
            return Promise.resolve();
          });
      }
    },

    protectedAddIceCandidate: (candidate) => {
      const rejectMessages = [];
      return that.peerConnection.addIceCandidate(candidate)
        .catch((error) => {
          rejectMessages.push(`in: protectedAddIceCandidate, error: ${error}`);
        }).then(() => {
          setTimeout(() => {
            negotiationQueue.stopEnqueuing();
            negotiationQueue.nextInQueue();
          }, 0);
          if (rejectMessages.length !== 0) {
            return Promise.reject(rejectMessages);
          }
          return Promise.resolve();
        });
    },

    protectedClose: () => {
      that.peerConnection.close();
      setTimeout(() => {
        negotiationQueue.stopEnqueuing();
        negotiationQueue.nextInQueue();
      }, 0);
      return Promise.resolve();
    },
  };


  const onIceCandidate = (event) => {
    let candidateObject = {};
    const candidate = event.candidate;
    if (!candidate) {
      log.debug('message: Gathered all candidates and sending END candidate');
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
      specBase.callback({ type: 'candidate', candidate: candidateObject, receivedSessionVersion: latestSessionVersion });
    } else {
      specBase.localCandidates.push(candidateObject);
      log.debug(`message: Storing candidates, size: ${specBase.localCandidates.length}`);
    }
  };

  // Peerconnection events
  that.peerConnection.onicecandidate = onIceCandidate;
  // public functions

  that.setStartVideoBW = (sdpInput) => {
    log.error('message: startVideoBW not implemented for this browser');
    return sdpInput;
  };

  that.setHardMinVideoBW = (sdpInput) => {
    log.error('message: hardMinVideoBw not implemented for this browser');
    return sdpInput;
  };

  that.enableSimulcast = (sdpInput) => {
    log.error('message: Simulcast not implemented');
    return sdpInput;
  };

  const setSpatialLayersConfig = (field, values, check = () => true) => {
    if (that.simulcast) {
      Object.keys(values).forEach((layerId) => {
        const value = values[layerId];
        if (!that.simulcast.spatialLayerConfigs) {
          that.simulcast.spatialLayerConfigs = {};
        }
        if (!that.simulcast.spatialLayerConfigs[layerId]) {
          that.simulcast.spatialLayerConfigs[layerId] = {};
        }
        if (check(value)) {
          that.simulcast.spatialLayerConfigs[layerId][field] = value;
        }
      });
      that.setSimulcastLayersConfig();
    }
  };

  that.updateSimulcastLayersBitrate = (bitrates) => {
    setSpatialLayersConfig('maxBitrate', bitrates);
  };

  that.updateSimulcastActiveLayers = (layersInfo) => {
    const ifIsBoolean = value => value === true || value === false;
    setSpatialLayersConfig('active', layersInfo, ifIsBoolean);
  };

  that.setSimulcastLayersConfig = () => {
    log.error('message: Simulcast not implemented');
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
    if (config.maxVideoBW) {
      log.debug(`message: Maxvideo Requested, value: ${config.maxVideoBW}, limit: ${specBase.limitMaxVideoBW}`);
      if (config.maxVideoBW > specBase.limitMaxVideoBW) {
        config.maxVideoBW = specBase.limitMaxVideoBW;
      }
      specBase.maxVideoBW = config.maxVideoBW;
      log.debug(`message: Maxvideo Result, value: ${config.maxVideoBW}, limit: ${specBase.limitMaxVideoBW}`);
    }
    if (config.maxAudioBW) {
      if (config.maxAudioBW > specBase.limitMaxAudioBW) {
        config.maxAudioBW = specBase.limitMaxAudioBW;
      }
      specBase.maxAudioBW = config.maxAudioBW;
    }
    if (shouldApplyMaxVideoBWToSdp || config.maxAudioBW) {
      that.enqueuedCalls.negotiationQueue.negotiateMaxBW(config, callback);
    }
    if (shouldSendMaxVideoBWInOptions ||
        config.minVideoBW ||
        (config.slideShowMode !== undefined) ||
        (config.muteStream !== undefined) ||
        (config.qualityLayer !== undefined) ||
        (config.slideShowBelowLayer !== undefined) ||
        (config.video !== undefined)) {
      log.debug(`message: Configuration changed, maxVideoBW: ${config.maxVideoBW}` +
        `, minVideoBW: ${config.minVideoBW}, slideShowMode: ${config.slideShowMode}` +
        `, muteStream: ${JSON.stringify(config.muteStream)}, videoConstraints: ${JSON.stringify(config.video)}` +
        `, slideShowBelowMinLayer: ${config.slideShowBelowLayer}`);
      specBase.callback({ type: 'updatestream', config }, streamId);
    }
  };


  // We need to protect it against calling multiple times to createOffer.
  // Otherwise it could change the ICE credentials before calling setLocalDescription
  // the first time in Chrome.
  that.createOffer = that.enqueuedCalls.negotiationQueue.createOffer;

  that.sendOffer = that.enqueuedCalls.negotiationQueue.createOffer.bind(null, true, true);

  that.addStream = that.enqueuedCalls.negotiationQueue.addStream;

  that.removeStream = that.enqueuedCalls.negotiationQueue.removeStream;

  that.close = that.enqueuedCalls.negotiationQueue.close;


  that.processSignalingMessage = (msgInput) => {
    logSDP('processSignalingMessage, type: ', msgInput.type);
    if (msgInput.type === 'offer') {
      that.enqueuedCalls.negotiationQueue.processOffer(msgInput);
    } else if (msgInput.type === 'answer') {
      that.peerConnectionFsm.processAnswer(msgInput);
    } else if (msgInput.type === 'candidate') {
      that.enqueuedCalls.negotiationQueue.processNewCandidate(msgInput);
    } else if (msgInput.type === 'error') {
      log.error(`message: Received error signaling message, state: ${msgInput.previousType}` +
        `, isEnqueuing: ${negotiationQueue.isEnqueueing()}`);
    }
  };

  that.peerConnectionFsm = new PeerConnectionFsm(that.protectedCalls);
  return that;
};

export default BaseStack;
