/* global L, mozRTCIceCandidate, mozRTCSessionDescription, mozRTCPeerConnection, Erizo*/
this.Erizo = this.Erizo || {};

Erizo.FirefoxStack = (specInput) => {
  const that = {};
  const WebkitRTCPeerConnection = mozRTCPeerConnection;
  const RTCSessionDescription = mozRTCSessionDescription;
  const RTCIceCandidate = mozRTCIceCandidate;
  const spec = specInput;

  that.pcConfig = {
    iceServers: [],
  };

  if (spec.iceServers !== undefined) {
    that.pcConfig.iceServers = spec.iceServers;
  }

  if (spec.audio === undefined) {
    spec.audio = true;
  }

  if (spec.video === undefined) {
    spec.video = true;
  }

  that.mediaConstraints = {
    offerToReceiveAudio: true,
    offerToReceiveVideo: true,
    mozDontOfferDataChannel: true,
  };

  const errorCallback = (message) => {
    L.Logger.error('Error in Stack ', message);
  };

  const enableSimulcast = () => {
    if (!spec.video || !spec.simulcast) {
      return;
    }
    that.peerConnection.getSenders().forEach((sender) => {
      if (sender.track.kind === 'video') {
        sender.getParameters();
        sender.setParameters({ encodings: [{
          rid: 'spam',
          active: true,
          priority: 'high',
          maxBitrate: 40000,
          maxHeight: 640,
          maxWidth: 480 }, {
            rid: 'egg',
            active: true,
            priority: 'medium',
            maxBitrate: 10000,
            maxHeight: 320,
            maxWidth: 240 }],
        });
      }
    });
  };


  let gotCandidate = false;
  that.peerConnection = new WebkitRTCPeerConnection(that.pcConfig, that.con);
  spec.localCandidates = [];

  that.peerConnection.onicecandidate = (event) => {
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
      gotCandidate = true;
      if (!candidate.candidate.match(/a=/)) {
        candidate.candidate = `a=${candidate.candidate}`;
      }
      candidateObject = candidate;
      if (spec.remoteDescriptionSet) {
        spec.callback({ type: 'candidate', candidate: candidateObject });
      } else {
        spec.localCandidates.push(candidateObject);
        L.Logger.debug('Local Candidates stored: ',
                               spec.localCandidates.length, spec.localCandidates);
      }
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

  that.peerConnection.oniceconnectionstatechange = (ev) => {
    if (that.oniceconnectionstatechange) {
      that.oniceconnectionstatechange(ev.target.iceConnectionState);
    }
  };

  const setMaxBW = (sdpInput) => {
    let r;
    let a;
    let sdp = sdpInput;
    if (spec.video && spec.maxVideoBW) {
      sdp = sdp.replace(/b=AS:.*\r\n/g, '');
      a = sdp.match(/m=video.*\r\n/);
      if (a == null) {
        a = sdp.match(/m=video.*\n/);
      }
      if (a && (a.length > 0)) {
        r = `${a[0]}b=AS:${spec.maxVideoBW}\r\n`;
        sdp = sdp.replace(a[0], r);
      }
    }

    if (spec.audio && spec.maxAudioBW) {
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

  let localDesc;

  const setLocalDesc = (sessionDescription) => {
    localDesc = sessionDescription;
    localDesc.sdp = setMaxBW(localDesc.sdp);
    localDesc.sdp =
              localDesc.sdp.replace(/a=ice-options:google-ice\r\n/g, '');
    spec.callback(localDesc);
  };

  const setLocalDescp2p = (sessionDescription) => {
    localDesc = sessionDescription;
    localDesc.sdp = setMaxBW(localDesc.sdp);
    localDesc.sdp =
              localDesc.sdp.replace(/a=ice-options:google-ice\r\n/g, '');
    spec.callback(localDesc);
    that.peerConnection.setLocalDescription(localDesc);
  };

  that.updateSpec = (configInput) => {
    const config = configInput;
    if (config.maxVideoBW || config.maxAudioBW) {
      if (config.maxVideoBW) {
        L.Logger.debug('Maxvideo Requested', config.maxVideoBW,
                                'limit', spec.limitMaxVideoBW);
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

      localDesc.sdp = setMaxBW(localDesc.sdp);
      if (config.Sdp) {
        L.Logger.error('Cannot update with renegotiation in Firefox, ' +
                                'try without renegotiation');
      } else {
        L.Logger.debug('Updating without renegotiation, newVideoBW:', spec.maxVideoBW,
                                ', newAudioBW:', spec.maxAudioBW);
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
    if (isSubscribe === true) {
      that.peerConnection.createOffer(setLocalDesc, errorCallback, that.mediaConstraints);
    } else {
      enableSimulcast();
      that.mediaConstraints = {
        offerToReceiveAudio: false,
        offerToReceiveVideo: false,
        mozDontOfferDataChannel: true,
      };
      that.peerConnection.createOffer(setLocalDesc, errorCallback, that.mediaConstraints);
    }
  };

  that.addStream = (stream) => {
    that.peerConnection.addStream(stream);
  };
  spec.remoteCandidates = [];
  spec.remoteDescriptionSet = false;

  /**
   * Closes the connection.
   */
  that.close = () => {
    that.state = 'closed';
    that.peerConnection.close();
  };

  that.processSignalingMessage = (msgInput) => {
    const msg = msgInput;
    // L.Logger.debug("Process Signaling Message", msg);

    if (msg.type === 'offer') {
      msg.sdp = setMaxBW(msg.sdp);
      that.peerConnection.setRemoteDescription(new RTCSessionDescription(msg), () => {
        that.peerConnection.createAnswer(setLocalDescp2p, (error) => {
          L.Logger.error('Error', error);
        }, that.mediaConstraints);
        spec.remoteDescriptionSet = true;
      }, (error) => {
        L.Logger.error('Error setting Remote Description', error);
      });
    } else if (msg.type === 'answer') {
      L.Logger.info('Set remote and local description');
      L.Logger.debug('Local Description to set', localDesc.sdp);
      L.Logger.debug('Remote Description to set', msg.sdp);

      msg.sdp = setMaxBW(msg.sdp);

      that.peerConnection.setLocalDescription(localDesc, () => {
        that.peerConnection.setRemoteDescription(
                  new RTCSessionDescription(msg), () => {
                    spec.remoteDescriptionSet = true;
                    L.Logger.info('Remote Description successfully set');
                    while (spec.remoteCandidates.length > 0 && gotCandidate) {
                      L.Logger.info('Setting stored remote candidates');
                        // IMPORTANT: preserve ordering of candidates
                      that.peerConnection.addIceCandidate(spec.remoteCandidates.shift());
                    }
                    while (spec.localCandidates.length > 0) {
                      L.Logger.info('Sending Candidate from list');
                        // IMPORTANT: preserve ordering of candidates
                      spec.callback({ type: 'candidate', candidate: spec.localCandidates.shift() });
                    }
                  }, (error) => {
                    L.Logger.error('Error Setting Remote Description', error);
                  });
      }, (error) => {
        L.Logger.error('Failure setting Local Description', error);
      });
    } else if (msg.type === 'candidate') {
      try {
        let obj;
        if (typeof (msg.candidate) === 'object') {
          obj = msg.candidate;
        } else {
          obj = JSON.parse(msg.candidate);
        }
        obj.candidate = obj.candidate.replace(/ generation 0/g, '');
        obj.candidate = obj.candidate.replace(/ udp /g, ' UDP ');

        obj.sdpMLineIndex = parseInt(obj.sdpMLineIndex, 10);
        const candidate = new RTCIceCandidate(obj);
        // L.logger.debug("Remote Candidate",candidate);

        if (spec.remoteDescriptionSet && gotCandidate) {
          that.peerConnection.addIceCandidate(candidate);
          while (spec.remoteCandidates.length > 0) {
            L.Logger.info('Setting stored remote candidates');
            // IMPORTANT: preserve ordering of candidates
            that.peerConnection.addIceCandidate(spec.remoteCandidates.shift());
          }
        } else {
          spec.remoteCandidates.push(candidate);
        }
      } catch (e) {
        L.Logger.error('Error parsing candidate', msg.candidate, e);
      }
    }
  };
  return that;
};
