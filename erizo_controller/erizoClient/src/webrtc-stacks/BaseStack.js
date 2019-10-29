/* global RTCSessionDescription, RTCIceCandidate, RTCPeerConnection */

// eslint-disable-next-line
import SemanticSdp from '../../../common/semanticSdp/SemanticSdp';
import Setup from '../../../common/semanticSdp/Setup';
import Direction from '../../../common/semanticSdp/Direction';

import PeerConnectionFsm from './PeerConnectionFsm';

import SdpHelpers from '../utils/SdpHelpers';
import Logger from '../utils/Logger';
import FunctionQueue from '../utils/FunctionQueue';

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

  that.peerConnection = new RTCPeerConnection(that.pcConfig, that.con);
  let negotiationneededCount = 0;
  that.peerConnection.onnegotiationneeded = () => {  // one per media which is added
    let medias = that.audio ? 1 : 0;
    medias += that.video ? 1 : 0;
    if (negotiationneededCount % medias === 0) {
      that.createOffer(true, true);
    }
    negotiationneededCount += 1;
  };

  const onFsmError = (message) => {
    that.peerConnectionFsm.error(message);
  };

  const configureLocalSdpAsAnswer = () => {
    localDesc.sdp = that.enableSimulcast(localDesc.sdp);
    localDesc.type = 'answer';
    localSdp = SemanticSdp.SDPInfo.processString(localDesc.sdp);
    SdpHelpers.setMaxBW(localSdp, specBase);

    const numberOfRemoteMedias = that.remoteSdp.getStreams().size;
    const numberOfLocalMedias = localSdp.getStreams().size;

    let direction = Direction.reverse('sendrecv');
    if (numberOfRemoteMedias > 0 && numberOfLocalMedias > 0) {
      direction = Direction.reverse('sendrecv');
    } else if (numberOfRemoteMedias > 0 && numberOfLocalMedias === 0) {
      direction = Direction.reverse('recvonly');
    } else if (numberOfRemoteMedias === 0 && numberOfLocalMedias > 0) {
      direction = Direction.reverse('sendonly');
    } else {
      direction = Direction.reverse('inactive');
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
      config: { maxVideoBW: specBase.maxVideoBW },
    });
  };

  const setLocalDescForAnswer = (sessionDescription) => {
    localDesc = sessionDescription;
    configureLocalSdpAsAnswer();
    specBase.callback({
      type: localDesc.type,
      sdp: localDesc.sdp,
      config: { maxVideoBW: specBase.maxVideoBW },
    });
    Logger.info('Setting local description', localDesc);
    Logger.debug('processOffer - Local Description', localDesc.type, localDesc.sdp);
    return that.peerConnection.setLocalDescription(localDesc).then(() => {
      that.setSimulcastLayersBitrate();
    });
  };

  // Functions that are protected by a functionQueue
  that.enqueuedCalls = {
    negotiationQueue: {
      createOffer: negotiationQueue.protectFunction((isSubscribe = false) => {
        const promise = that.peerConnectionFsm.createOffer(isSubscribe);
        if (promise) {
          promise.catch(onFsmError.bind(this));
        }
      }),

      processOffer: negotiationQueue.protectFunction((message) => {
        const promise = that.peerConnectionFsm.processOffer(message);
        if (promise) {
          promise.catch(onFsmError.bind(this));
        }
      }),

      processAnswer: negotiationQueue.protectFunction((message) => {
        const promise = that.peerConnectionFsm.processAnswer(message);
        if (promise) {
          promise.catch(onFsmError.bind(this));
        }
      }),

      negotiateMaxBW: negotiationQueue.protectFunction((configInput, callback) => {
        const promise = that.peerConnectionFsm.negotiateMaxBW(configInput, callback);
        if (promise) {
          promise.catch(onFsmError.bind(this));
        }
      }),

      processNewCandidate: negotiationQueue.protectFunction((message) => {
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
            negotiationQueue.startEnqueuing();
            that.peerConnectionFsm.addIceCandidate(candidate).catch(onFsmError.bind(this));
          } else {
            specBase.remoteCandidates.push(candidate);
          }
        } catch (e) {
          Logger.error('Error parsing candidate', msg.candidate);
        }
      }),

      addStream: negotiationQueue.protectFunction((stream) => {
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

    firstLocalDescriptionQueue: {
      createOffer:
      firstLocalDescriptionQueue.protectFunction((isSubscribe = false,
        forceOfferToReceive = false) => {
        if (!firstLocalDescriptionSet) {
          firstLocalDescriptionQueue.startEnqueuing();
        }
        if (!isSubscribe && !forceOfferToReceive) {
          that.mediaConstraints = {
            offerToReceiveVideo: false,
            offerToReceiveAudio: false,
          };
        }
        that.enqueuedCalls.negotiationQueue.createOffer(isSubscribe);
      }),

      sendOffer: firstLocalDescriptionQueue.protectFunction(() => {
        if (!firstLocalDescriptionSet) {
          that.createOffer(true, true);
          return;
        }
        setLocalDescForOffer(true, localDesc);
      }),

      processOffer:
      firstLocalDescriptionQueue.protectFunction((message) => {
        if (!firstLocalDescriptionSet) {
          firstLocalDescriptionQueue.startEnqueuing();
        }
        that.enqueuedCalls.negotiationQueue.processOffer(message);
      }),
    },
  };

  // Functions that are protected by the FSM.
  // The promise of one has to be resolved before another can be called.
  that.protectedCalls = {
    protectedAddStream: (stream) => {
      that.peerConnection.addStream(stream);
      setTimeout(() => {
        negotiationQueue.stopEnqueuing();
        negotiationQueue.nextInQueue();
      }, 0);
      return Promise.resolve();
    },

    protectedRemoveStream: (stream) => {
      that.peerConnection.removeStream(stream);
      setTimeout(() => {
        negotiationQueue.stopEnqueuing();
        negotiationQueue.nextInQueue();
      }, 0);
      return Promise.resolve();
    },

    protectedCreateOffer: (isSubscribe = false) => {
      negotiationQueue.startEnqueuing();
      Logger.debug('Creating offer', that.mediaConstraints);
      const rejectMessages = [];
      return that.peerConnection.createOffer(that.mediaConstraints)
        .then(setLocalDescForOffer.bind(null, isSubscribe))
        .catch((error) => {
          rejectMessages.push(`in protectedCreateOffer-createOffer, error: ${error}`);
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
    },

    protectedProcessOffer: (message) => {
      Logger.info('Protected process Offer,', message, 'localDesc', localDesc);
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
        });
        return Promise.resolve();
      }
      negotiationQueue.startEnqueuing();
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
      return that.peerConnection.setRemoteDescription(msg)
        .then(() => {
          specBase.remoteDescriptionSet = true;
        }).then(() => that.peerConnection.createAnswer(that.mediaConstraints))
        .catch((error) => {
          rejectMessage.push(`in: protectedProcessOffer-createAnswer, error: ${error}`);
        })
        .then(setLocalDescForAnswer.bind(this))
        .catch((error) => {
          rejectMessage.push(`in: protectedProcessOffer-setLocalDescForAnswer, error: ${error}`);
        })
        .then(() => {
          setTimeout(() => {
            firstLocalDescriptionSet = true;
            firstLocalDescriptionQueue.stopEnqueuing();
            firstLocalDescriptionQueue.dequeueAll();
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
        Logger.warning(`processAnswer discarding old sdp, sessionVersion: ${sessionVersion}, latestSessionVersion: ${latestSessionVersion}`);
        return Promise.resolve();
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
      const rejectMessages = [];
      return that.peerConnection.setLocalDescription(localDesc)
        .then(() => {
          that.setSimulcastLayersBitrate();
          that.peerConnection.setRemoteDescription(new RTCSessionDescription(msg));
        })
        .then(() => {
          specBase.remoteDescriptionSet = true;
          Logger.info('Candidates to be added: ', specBase.remoteCandidates.length,
            specBase.remoteCandidates);
          while (specBase.remoteCandidates.length > 0) {
            // IMPORTANT: preserve ordering of candidates
            that.peerConnectionFsm.addIceCandidate(specBase.remoteCandidates.shift())
              .catch(onFsmError.bind(this));
          }
          Logger.info('Local candidates to send:', specBase.localCandidates.length);
          while (specBase.localCandidates.length > 0) {
            // IMPORTANT: preserve ordering of candidates
            specBase.callback({ type: 'candidate', candidate: specBase.localCandidates.shift() });
          }
        })
        .catch((error) => {
          rejectMessages.push(`in: protectedProcessAnswer, error: ${error}`);
        })
        .then(() => {
          setTimeout(() => {
            firstLocalDescriptionSet = true;
            firstLocalDescriptionQueue.stopEnqueuing();
            firstLocalDescriptionQueue.dequeueAll();
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
        that.peerConnection.setLocalDescription(localDesc)
          .then(() => {
            that.setSimulcastLayersBitrate();
            remoteSdp = SemanticSdp.SDPInfo.processString(remoteDesc.sdp);
            SdpHelpers.setMaxBW(remoteSdp, specBase);
            remoteDesc.sdp = remoteSdp.toString();
            that.remoteSdp = remoteSdp;
            return that.peerConnection.setRemoteDescription(
              new RTCSessionDescription(remoteDesc));
          }).then(() => {
            specBase.remoteDescriptionSet = true;
            specBase.callback({ type: 'offer-noanswer', sdp: localDesc.sdp });
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

    protectedAddIceCandiate: (candidate) => {
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

  that.updateSimulcastLayersBitrate = (bitrates) => {
    if (that.simulcast) {
      that.simulcast.spatialLayerBitrates = bitrates;
      that.setSimulcastLayersBitrate();
    }
  };

  that.setSimulcastLayersBitrate = () => {
    Logger.error('Simulcast not implemented');
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
      Logger.debug('MaxVideoBW Changed to ', config.maxVideoBW);
      Logger.debug('MinVideo Changed to ', config.minVideoBW);
      Logger.debug('SlideShowMode Changed to ', config.slideShowMode);
      Logger.debug('muteStream changed to ', config.muteStream);
      Logger.debug('Video Constraints', config.video);
      Logger.debug('Will activate slideshow when below layer', config.slideShowBelowLayer);
      specBase.callback({ type: 'updatestream', config }, streamId);
    }
  };


  // We need to protect it against calling multiple times to createOffer.
  // Otherwise it could change the ICE credentials before calling setLocalDescription
  // the first time in Chrome.
  that.createOffer = that.enqueuedCalls.firstLocalDescriptionQueue.createOffer;

  that.sendOffer = that.enqueuedCalls.firstLocalDescriptionQueue.sendOffer;

  that.addStream = that.enqueuedCalls.negotiationQueue.addStream;

  that.removeStream = that.enqueuedCalls.negotiationQueue.removeStream;

  that.close = that.enqueuedCalls.negotiationQueue.close;


  that.processSignalingMessage = (msgInput) => {
    if (msgInput.type === 'offer') {
      that.enqueuedCalls.firstLocalDescriptionQueue.processOffer(msgInput);
    } else if (msgInput.type === 'answer') {
      that.enqueuedCalls.negotiationQueue.processAnswer(msgInput);
    } else if (msgInput.type === 'candidate') {
      that.enqueuedCalls.negotiationQueue.processNewCandidate(msgInput);
    } else if (msgInput.type === 'error') {
      Logger.error('Received error signaling message, state:', msgInput.previousType, firstLocalDescriptionQueue.isEnqueueing());
      if (msgInput.previousType === 'offer' && firstLocalDescriptionQueue.isEnqueueing()) {
        firstLocalDescriptionQueue.stopEnqueuing();
        firstLocalDescriptionQueue.nextInQueue();
      }
    }
  };

  that.peerConnectionFsm = new PeerConnectionFsm(that.protectedCalls);
  return that;
};

export default BaseStack;
