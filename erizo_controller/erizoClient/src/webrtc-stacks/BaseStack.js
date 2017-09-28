/* global RTCSessionDescription, RTCIceCandidate, RTCPeerConnection */

import SdpHelpers from '../utils/SdpHelpers';
import Logger from '../utils/Logger';

const BaseStack = (specInput) => {
  const that = {};
  const specBase = specInput;
  let localDesc;
  let remoteDesc;

  Logger.info('Starting Base stack', specBase);

  that.pcConfig = {
    iceServers: [],
  };

  that.con = {};
  if (specBase.iceServers !== undefined) {
    that.pcConfig.iceServers = specBase.iceServers;
  }
  if (specBase.forceTurn === true) {
    that.pcConfig.iceTransportPolicy = 'relay';
  }
  if (specBase.audio === undefined) {
    specBase.audio = true;
  }
  if (specBase.video === undefined) {
    specBase.video = true;
  }
  specBase.remoteCandidates = [];
  specBase.localCandidates = [];
  specBase.remoteDescriptionSet = false;

  that.mediaConstraints = {
    offerToReceiveVideo: (specBase.video !== undefined && specBase.video !== false),
    offerToReceiveAudio: (specBase.audio !== undefined && specBase.audio !== false),
  };

  that.peerConnection = new RTCPeerConnection(that.pcConfig, that.con);

  // Aux functions

  const errorCallback = (where, errorcb, message) => {
    Logger.error('message:', message, 'in baseStack at', where);
    if (errorcb !== undefined) {
      errorcb('error');
    }
  };

  const successCallback = (message) => {
    Logger.info('Success in BaseStack', message);
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
      if (!candidate.candidate.match(/a=/)) {
        candidate.candidate = `a=${candidate.candidate}`;
      }

      candidateObject = {
        sdpMLineIndex: candidate.sdpMLineIndex,
        sdpMid: candidate.sdpMid,
        candidate: candidate.candidate,
      };
    }

    if (specBase.remoteDescriptionSet) {
      specBase.callback({ type: 'candidate', candidate: candidateObject });
    } else {
      specBase.localCandidates.push(candidateObject);
      Logger.info('Storing candidate: ', specBase.localCandidates.length, candidateObject);
    }
  };

  const setLocalDescForOffer = (isSubscribe, sessionDescription) => {
    localDesc = sessionDescription;
    if (!isSubscribe) {
      localDesc.sdp = that.enableSimulcast(localDesc.sdp);
    }
    localDesc.sdp = SdpHelpers.setMaxBW(localDesc.sdp, specBase);
    specBase.callback({
      type: localDesc.type,
      sdp: localDesc.sdp,
    });
  };

  const setLocalDescForAnswerp2p = (sessionDescription) => {
    localDesc = sessionDescription;
    localDesc.sdp = SdpHelpers.setMaxBW(localDesc.sdp, specBase);
    specBase.callback({
      type: localDesc.type,
      sdp: localDesc.sdp,
    });
    Logger.info('Setting local description p2p', localDesc);
    that.peerConnection.setLocalDescription(localDesc).then(successCallback)
    .catch(errorCallback);
  };

  const processOffer = (message) => {
    // Its an offer, we assume its p2p
    const msg = message;
    msg.sdp = SdpHelpers.setMaxBW(msg.sdp, specBase);
    that.peerConnection.setRemoteDescription(msg).then(() => {
      that.peerConnection.createAnswer(that.mediaConstraints)
      .then(setLocalDescForAnswerp2p).catch(errorCallback.bind(null, 'createAnswer p2p', undefined));
      specBase.remoteDescriptionSet = true;
    }).catch(errorCallback.bind(null, 'process Offer', undefined));
  };

  const processAnswer = (message) => {
    const msg = message;
    Logger.info('Set remote and local description');
    Logger.debug('Remote Description', msg.sdp);
    Logger.debug('Local Description', localDesc.sdp);

    msg.sdp = SdpHelpers.setMaxBW(msg.sdp, specBase);

    remoteDesc = msg;
    that.peerConnection.setLocalDescription(localDesc).then(() => {
      that.peerConnection.setRemoteDescription(new RTCSessionDescription(msg)).then(() => {
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
      }).catch(errorCallback.bind(null, 'processAnswer', undefined));
    }).catch(errorCallback.bind(null, 'processAnswer', undefined));
  };

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
  that.peerConnection.onaddstream = (stream) => {
    if (that.onaddstream) {
      that.onaddstream(stream);
    }
  };
  that.peerConnection.onremovestream = (stream) => {
    if (that.onremovestream) {
      that.onremovestream(stream);
    }
  };

  that.peerConnection.oniceconnectionstatechange = (ev) => {
    if (that.oniceconnectionstatechange) {
      that.oniceconnectionstatechange(ev.target.iceConnectionState);
    }
  };

  // public functions

  that.enableSimulcast = (sdpInput) => {
    Logger.error('Simulcast not implemented');
    return sdpInput;
  };

  that.close = () => {
    that.state = 'closed';
    that.peerConnection.close();
  };

  that.updateSpec = (configInput, callback = () => {}) => {
    const config = configInput;
    if (config.maxVideoBW || config.maxAudioBW) {
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

      localDesc.sdp = SdpHelpers.setMaxBW(localDesc.sdp, specBase);
      if (config.Sdp || config.maxAudioBW) {
        Logger.debug('Updating with SDP renegotiation', specBase.maxVideoBW, specBase.maxAudioBW);
        that.peerConnection.setLocalDescription(localDesc)
          .then(() => {
            remoteDesc.sdp = SdpHelpers.setMaxBW(remoteDesc.sdp, specBase);
            return that.peerConnection.setRemoteDescription(new RTCSessionDescription(remoteDesc));
          }).then(() => {
            specBase.remoteDescriptionSet = true;
            specBase.callback({ type: 'updatestream', sdp: localDesc.sdp });
          }).catch(errorCallback.bind(null, 'updateSpec', callback));
      } else {
        Logger.debug('Updating without SDP renegotiation, ' +
                     'newVideoBW:', specBase.maxVideoBW,
                     'newAudioBW:', specBase.maxAudioBW);
        specBase.callback({ type: 'updatestream', sdp: localDesc.sdp });
      }
    }
    if (config.minVideoBW || (config.slideShowMode !== undefined) ||
            (config.muteStream !== undefined) || (config.qualityLayer !== undefined) ||
            (config.video !== undefined)) {
      Logger.debug('MinVideo Changed to ', config.minVideoBW);
      Logger.debug('SlideShowMode Changed to ', config.slideShowMode);
      Logger.debug('muteStream changed to ', config.muteStream);
      Logger.debug('Video Constraints', config.video);
      specBase.callback({ type: 'updatestream', config });
    }
  };

  that.createOffer = (isSubscribe) => {
    if (isSubscribe !== true) {
      that.mediaConstraints = {
        offerToReceiveVideo: false,
        offerToReceiveAudio: false,
      };
    }
    Logger.debug('Creating offer', that.mediaConstraints);
    that.peerConnection.createOffer(that.mediaConstraints)
    .then(setLocalDescForOffer.bind(null, isSubscribe))
    .catch(errorCallback.bind(null, 'Create Offer', undefined));
  };

  that.addStream = (stream) => {
    that.peerConnection.addStream(stream);
  };

  that.processSignalingMessage = (msgInput) => {
    if (msgInput.type === 'offer') {
      processOffer(msgInput);
    } else if (msgInput.type === 'answer') {
      processAnswer(msgInput);
    } else if (msgInput.type === 'candidate') {
      processNewCandidate(msgInput);
    }
  };

  return that;
};

export default BaseStack;
