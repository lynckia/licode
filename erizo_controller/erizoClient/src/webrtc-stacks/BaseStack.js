/* global RTCSessionDescription, RTCIceCandidate, RTCPeerConnection */
// eslint-disable-next-line
import SemanticSdp from '../../../common/semanticSdp/SemanticSdp';
import Setup from '../../../common/semanticSdp/Setup';

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

  // Aux functions

  const errorCallback = (where, errorcb, message) => {
    Logger.error('message:', message, 'in baseStack at', where);
    if (errorcb !== undefined) {
      errorcb('error');
    }
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
    that.peerConnection.setRemoteDescription(msg)
    .then(() => {
      specBase.remoteDescriptionSet = true;
    }).then(() => that.peerConnection.createAnswer(that.mediaConstraints))
    .catch(errorCallback.bind(null, 'createAnswer', undefined))
    .then(setLocalDescForAnswer.bind(this, streamIds))
    .catch(errorCallback.bind(null, 'process Offer', undefined))
    .then(() => {
      firstLocalDescriptionSet = true;
      firstLocalDescriptionQueue.stopEnqueuing();
      firstLocalDescriptionQueue.dequeueAll();
      negotiationQueue.stopEnqueuing();
      negotiationQueue.nextInQueue();
    });
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
    that.peerConnection.setLocalDescription(localDesc)
      .then(() => that.peerConnection.setRemoteDescription(new RTCSessionDescription(msg)))
      .then(() => {
        specBase.remoteDescriptionSet = true;
        Logger.info('Candidates to be added: ', specBase.remoteCandidates.length,
                      specBase.remoteCandidates);
        while (specBase.remoteCandidates.length > 0) {
          // IMPORTANT: preserve ordering of candidates
          that.peerConnection.addIceCandidate(specBase.remoteCandidates.shift());
        }
        Logger.info('Local candidates to send:', specBase.localCandidates.length);
        while (specBase.localCandidates.length > 0) {
          // IMPORTANT: preserve ordering of candidates
          specBase.callback({ type: 'candidate', candidate: specBase.localCandidates.shift() });
        }
      })
      .catch(errorCallback.bind(null, 'processAnswer', undefined))
      .then(() => {
        firstLocalDescriptionSet = true;
        firstLocalDescriptionQueue.stopEnqueuing();
        firstLocalDescriptionQueue.dequeueAll();
        negotiationQueue.stopEnqueuing();
        negotiationQueue.nextInQueue();
      });
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
        that.peerConnection.addIceCandidate(candidate);
      } else {
        specBase.remoteCandidates.push(candidate);
      }
    } catch (e) {
      Logger.error('Error parsing candidate', msg.candidate);
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

  that.close = () => {
    that.state = 'closed';
    that.peerConnection.close();
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
        that.peerConnection.setLocalDescription(localDesc)
          .then(() => {
            remoteSdp = SemanticSdp.SDPInfo.processString(remoteDesc.sdp);
            SdpHelpers.setMaxBW(remoteSdp, specBase);
            remoteDesc.sdp = remoteSdp.toString();
            that.remoteSdp = remoteSdp;
            return that.peerConnection.setRemoteDescription(new RTCSessionDescription(remoteDesc));
          }).then(() => {
            specBase.remoteDescriptionSet = true;
            specBase.callback({ type: 'updatestream', sdp: localDesc.sdp }, streamId);
          }).catch(errorCallback.bind(null, 'updateSpec', callback));
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
    negotiationQueue.startEnqueuing();
    Logger.debug('Creating offer', that.mediaConstraints, streamId);
    that.peerConnection.createOffer(that.mediaConstraints)
      .then(setLocalDescForOffer.bind(null, isSubscribe, streamId))
      .catch(errorCallback.bind(null, 'Create Offer', undefined))
      .then(() => {
        negotiationQueue.stopEnqueuing();
        negotiationQueue.nextInQueue();
      });
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
    that.peerConnection.addStream(stream);
  };

  that.removeStream = (stream) => {
    that.peerConnection.removeStream(stream);
  };

  that.processSignalingMessage = (msgInput, streamIds) => {
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
