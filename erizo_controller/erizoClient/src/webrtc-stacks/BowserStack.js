/* global L, console, RTCSessionDescription, webkitRTCPeerConnection, RTCIceCandidate, Erizo*/
this.Erizo = this.Erizo || {};

Erizo.BowserStack = (specInput) => {
  const that = {};
  const WebkitRTCPeerConnection = webkitRTCPeerConnection;
  const spec = specInput;

  that.pcConfig = {
    iceServers: [],
  };

  that.con = { optional: [{ DtlsSrtpKeyAgreement: true }] };

  if (spec.stunServerUrl !== undefined) {
    that.pcConfig.iceServers.push({ url: spec.stunServerUrl });
  }

  if ((spec.turnServer || {}).url) {
    that.pcConfig.iceServers.push({ username: spec.turnServer.username,
      credential: spec.turnServer.password,
      url: spec.turnServer.url });
  }

  if (spec.audio === undefined) {
    spec.audio = true;
  }

  if (spec.video === undefined) {
    spec.video = true;
  }

  that.mediaConstraints = {
    offerToReceiveVideo: spec.video,
    offerToReceiveAudio: spec.audio,
  };

  that.peerConnection = new WebkitRTCPeerConnection(that.pcConfig, that.con);

  spec.remoteDescriptionSet = false;

  const setMaxBW = (sdpInput) => {
    let a;
    let r;
    let sdp = sdpInput;
    if (spec.maxVideoBW) {
      a = sdp.match(/m=video.*\r\n/);
      if (a == null) {
        a = sdp.match(/m=video.*\n/);
      }
      if (a && (a.length > 0)) {
        r = `${a[0]}b=AS:${spec.maxVideoBW}\r\n`;
        sdp = sdp.replace(a[0], r);
      }
    }

    if (spec.maxAudioBW) {
      a = sdp.match(/m=audio.*\r\n/);
      if (a == null) {
        a = sdp.match(/m=audio.*\n/);
      }
      if (a && (a.length > 0)) {
        r = `${a[0]}b=AS:${spec.maxAudioBW}\r\n`;
        sdp = sdp.replace(a[0], r);
      }
    }

    return sdp;
  };

    /**
     * Closes the connection.
     */
  that.close = () => {
    that.state = 'closed';
    that.peerConnection.close();
  };

  spec.localCandidates = [];

  that.peerConnection.onicecandidate = (event) => {
    const candidate = event.candidate;
    if (candidate) {
      if (!candidate.candidate.match(/a=/)) {
        candidate.candidate = `a=${candidate.candidate}`;
      }


      if (spec.remoteDescriptionSet) {
        spec.callback({ type: 'candidate', candidate });
      } else {
        spec.localCandidates.push(candidate);
//                L.Logger.debug('Local Candidates stored: ',
//                             spec.localCandidates.length, spec.localCandidates);
      }
    } else {
          //  spec.callback(that.peerConnection.localDescription);
      L.Logger.debug('End of candidates.', that.peerConnection.localDescription);
    }
  };

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

  const errorCallback = (message) => {
    L.Logger.debug('Error in Stack ', message);
  };

  let localDesc;

  const setLocalDesc = (sessionDescription) => {
    localDesc = sessionDescription;
    localDesc.sdp = setMaxBW(localDesc.sdp);
    L.Logger.debug('Set local description', localDesc);

    that.peerConnection.setLocalDescription(localDesc, () => {
      L.Logger.debug('The final LocalDesc', that.peerConnection.localDescription);
      spec.callback(that.peerConnection.localDescription);
    }, errorCallback);
  };

  const setLocalDescp2p = (sessionDescription) => {
    localDesc = sessionDescription;
    localDesc.sdp = setMaxBW(localDesc.sdp);
//        sessionDescription.sdp = sessionDescription.sdp
//                                          .replace(/a=ice-options:google-ice\r\n/g, "");
    spec.callback(localDesc);

    that.peerConnection.setLocalDescription(localDesc);
  };

  that.createOffer = (isSubscribe) => {
    if (isSubscribe === true) {
      that.peerConnection.createOffer(setLocalDesc, errorCallback, that.mediaConstraints);
    } else {
      that.peerConnection.createOffer(setLocalDesc, errorCallback);
    }
  };

  that.addStream = (stream) => {
    that.peerConnection.addStream(stream);
  };
  spec.remoteCandidates = [];


  that.processSignalingMessage = (inputMessage) => {
    const msg = inputMessage;
    L.Logger.debug('Process Signaling Message', msg);

    if (msg.type === 'offer') {
      msg.sdp = setMaxBW(msg.sdp);
      that.peerConnection.setRemoteDescription(new RTCSessionDescription(msg));
      that.peerConnection.createAnswer(setLocalDescp2p, null, that.mediaConstraints);
      spec.remoteDescriptionSet = true;
    } else if (msg.type === 'answer') {
      L.Logger.debug('Set remote description', msg.sdp);

      msg.sdp = setMaxBW(msg.sdp);

      that.peerConnection.setRemoteDescription(new RTCSessionDescription(msg), () => {
        spec.remoteDescriptionSet = true;
        L.Logger.debug('Candidates to be added: ', spec.remoteCandidates.length);
        while (spec.remoteCandidates.length > 0) {
          L.Logger.debug('Candidate :', spec.remoteCandidates[spec.remoteCandidates.length - 1]);
          that.peerConnection.addIceCandidate(spec.remoteCandidates.shift(),
                                                    () => {},
                                                    errorCallback);
        }
        while (spec.localCandidates.length > 0) {
          spec.callback({ type: 'candidate', candidate: spec.localCandidates.shift() });
        }
      }, () => { L.Logger.debug('Error Setting Remote Description'); });
    } else if (msg.type === 'candidate') {
      L.Logger.debug('Message with candidate');
      try {
        let obj;
        if (typeof (msg.candidate) === 'object') {
          obj = msg.candidate;
        } else {
          obj = JSON.parse(msg.candidate);
        }
//      obj.candidate = obj.candidate.replace(/ generation 0/g, "");
//      obj.candidate = obj.candidate.replace(/ udp /g, " UDP ");
        obj.candidate = obj.candidate.replace(/a=/g, '');
        obj.sdpMLineIndex = parseInt(obj.sdpMLineIndex, 10);
        obj.sdpMLineIndex = obj.sdpMid === 'audio' ? 0 : 1;
        const candidate = new RTCIceCandidate(obj);
        L.Logger.debug('Remote Candidate', candidate);
        if (spec.remoteDescriptionSet) {
          that.peerConnection.addIceCandidate(candidate, () => {}, errorCallback);
        } else {
          spec.remoteCandidates.push(candidate);
        }
      } catch (e) {
        L.Logger.error('Error parsing candidate', msg.candidate);
      }
    }
  };

  return that;
};
