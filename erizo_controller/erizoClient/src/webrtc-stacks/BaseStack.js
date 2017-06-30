/* global L, RTCSessionDescription, RTCIceCandidate, RTCPeerConnection Erizo*/
this.Erizo = this.Erizo || {};

Erizo.BaseStack = (specInput) => {
  const that = {};
  const spec = specInput;
  let localDesc;
  let remoteDesc;

  L.Logger.info('Starting Chrome stable stack');

  that.pcConfig = {
    iceServers: [],
  };

  that.con = {};
  if (spec.iceServers !== undefined) {
    that.pcConfig.iceServers = spec.iceServers;
  }
  if (spec.audio === undefined) {
    spec.audio = true;
  }
  if (spec.video === undefined) {
    spec.video = true;
  }
  spec.remoteCandidates = [];
  spec.localCandidates = [];
  spec.remoteDescriptionSet = false;

  that.mediaConstraints = {
    offerToReceiveVideo: spec.video,
    offerToReceiveAudio: spec.audio,
  };

  that.peerConnection = new RTCPeerConnection(that.pcConfig, that.con);

  // Aux functions

  const errorCallback = (where, errorcb, message) => {
    L.Logger.error('message:', message, 'in baseStack at', where);
    if (errorcb !== undefined) {
      errorcb('error');
    }
  };

  const successCallback = (message) => {
    L.Logger.info('Success in BaseStack', message);
  };

  const onIceCandidate = (event) => {
    let candidateObject = {};
    const candidate = event.candidate;
    if (!candidate) {
      L.Logger.info('Gathered all candidates. Sending END candidate');
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

    if (spec.remoteDescriptionSet) {
      spec.callback({ type: 'candidate', candidate: candidateObject });
    } else {
      spec.localCandidates.push(candidateObject);
      L.Logger.info('Storing candidate: ', spec.localCandidates.length, candidateObject);
    }
  };

  const setLocalDescForOfferp2p = (isSubscribe, sessionDescription) => {
    localDesc = sessionDescription;
    if (!isSubscribe) {
      localDesc.sdp = that.enableSimulcast(localDesc.sdp);
    }
    localDesc.sdp = Erizo.SdpHelpers.setMaxBW(localDesc.sdp, spec);
    spec.callback({
      type: localDesc.type,
      sdp: localDesc.sdp,
    });
  };

  const setLocalDescForAnswerp2p = (sessionDescription) => {
    localDesc = sessionDescription;
    localDesc.sdp = Erizo.SdpHelpers.setMaxBW(localDesc.sdp, spec);
    spec.callback({
      type: localDesc.type,
      sdp: localDesc.sdp,
    });
    L.Logger.info('Setting local description p2p', localDesc);
    that.peerConnection.setLocalDescription(localDesc).then(successCallback)
    .catch(errorCallback);
  };

  const processOffer = (message) => {
    // Its an offer, we assume its p2p
    const msg = message;
    msg.sdp = Erizo.SdpHelpers.setMaxBW(msg.sdp, spec);
    that.peerConnection.setRemoteDescription(msg).then(() => {
      that.peerConnection.createAnswer(that.mediaConstraints)
      .then(setLocalDescForAnswerp2p).catch(errorCallback.bind(null, 'createAnswer p2p', undefined));
      spec.remoteDescriptionSet = true;
    }).catch(errorCallback.bind(null, 'process Offer', undefined));
  };

  const processAnswer = (message) => {
    const msg = message;
    L.Logger.info('Set remote and local description');
    L.Logger.debug('Remote Description', msg.sdp);
    L.Logger.debug('Local Description', localDesc.sdp);

    msg.sdp = Erizo.SdpHelpers.setMaxBW(msg.sdp, spec);

    remoteDesc = msg;
    that.peerConnection.setLocalDescription(localDesc).then(() => {
      that.peerConnection.setRemoteDescription(new RTCSessionDescription(msg)).then(() => {
        spec.remoteDescriptionSet = true;
        L.Logger.info('Candidates to be added: ', spec.remoteCandidates.length,
                      spec.remoteCandidates);
        while (spec.remoteCandidates.length > 0) {
          // IMPORTANT: preserve ordering of candidates
          that.peerConnection.addIceCandidate(spec.remoteCandidates.shift());
        }
        L.Logger.info('Local candidates to send:', spec.localCandidates.length);
        while (spec.localCandidates.length > 0) {
          // IMPORTANT: preserve ordering of candidates
          spec.callback({ type: 'candidate', candidate: spec.localCandidates.shift() });
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
      if (spec.remoteDescriptionSet) {
        that.peerConnection.addIceCandidate(candidate);
      } else {
        spec.remoteCandidates.push(candidate);
      }
    } catch (e) {
      L.Logger.error('Error parsing candidate', msg.candidate);
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
    L.Logger.error('Simulcast not implemented');
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
        L.Logger.debug('Maxvideo Requested:', config.maxVideoBW,
                                'limit:', spec.limitMaxVideoBW);
        if (config.maxVideoBW > spec.limitMaxVideoBW) {
          config.maxVideoBW = spec.limitMaxVideoBW;
        }
        spec.maxVideoBW = config.maxVideoBW;
        L.Logger.debug('Result', spec.maxVideoBW);
      }
      if (config.maxAudioBW) {
        if (config.maxAudioBW > spec.limitMaxAudioBW) {
          config.maxAudioBW = spec.limitMaxAudioBW;
        }
        spec.maxAudioBW = config.maxAudioBW;
      }

      localDesc.sdp = Erizo.SdpHelpers.setMaxBW(localDesc.sdp, spec);
      if (config.Sdp || config.maxAudioBW) {
        L.Logger.debug('Updating with SDP renegotiation', spec.maxVideoBW, spec.maxAudioBW);
        that.peerConnection.setLocalDescription(localDesc).then(() => {
          remoteDesc.sdp = Erizo.SdpHelpers.setMaxBW(remoteDesc.sdp, spec);
          that.peerConnection.setRemoteDescription(new RTCSessionDescription(remoteDesc))
          .then(() => {
            spec.remoteDescriptionSet = true;
            spec.callback({ type: 'updatestream', sdp: localDesc.sdp });
          }).catch(errorCallback.bind(null, 'updateSpec', undefined));
        }).catch(errorCallback.bind(null, 'updateSpec', callback));
      } else {
        L.Logger.debug('Updating without SDP renegotiation, ' +
                                'newVideoBW:', spec.maxVideoBW,
                                'newAudioBW:', spec.maxAudioBW);
        spec.callback({ type: 'updatestream', sdp: localDesc.sdp });
      }
    }
    if (config.minVideoBW || (config.slideShowMode !== undefined) ||
            (config.muteStream !== undefined) || (config.qualityLayer !== undefined)) {
      L.Logger.debug('MinVideo Changed to ', config.minVideoBW);
      L.Logger.debug('SlideShowMode Changed to ', config.slideShowMode);
      L.Logger.debug('muteStream changed to ', config.muteStream);
      spec.callback({ type: 'updatestream', config });
    }
  };

  that.createOffer = (isSubscribe) => {
    if (isSubscribe !== true) {
      that.mediaConstraints = {
        offerToReceiveVideo: false,
        offerToReceiveAudio: false,
      };
    }
    L.Logger.debug('Creating offer', that.mediaConstraints);
    that.peerConnection.createOffer(that.mediaConstraints)
    .then(setLocalDescForOfferp2p.bind(null, isSubscribe))
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
