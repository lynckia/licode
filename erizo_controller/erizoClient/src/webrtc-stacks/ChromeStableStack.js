/* global L, RTCSessionDescription, webkitRTCPeerConnection, RTCIceCandidate, Erizo*/
this.Erizo = this.Erizo || {};

Erizo.ChromeStableStack = (specInput) => {
  const that = {};
  const WebkitRTCPeerConnection = webkitRTCPeerConnection;
  const defaultSimulcastSpatialLayers = 2;
  const spec = specInput;

  that.pcConfig = {
    iceServers: [],
  };


  that.con = { optional: [{ DtlsSrtpKeyAgreement: true }] };

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
    mandatory: {
      OfferToReceiveVideo: spec.video,
      OfferToReceiveAudio: spec.audio,
    },
  };

  const errorCallback = (message) => {
    L.Logger.error('Error in Stack ', message);
  };

  that.peerConnection = new WebkitRTCPeerConnection(that.pcConfig, that.con);

  const addSim = (spatialLayers) => {
    let line = 'a=ssrc-group:SIM';
    spatialLayers.forEach((spatialLayerId) => {
      line += ` ${spatialLayerId}`;
    });
    return `${line}\r\n`;
  };

  const addGroup = (spatialLayerId, spatialLayerIdRtx) =>
    `a=ssrc-group:FID ${spatialLayerId} ${spatialLayerIdRtx}\r\n`;

  const addSpatialLayer = (cname, msid, mslabel, label, spatialLayerId, spatialLayerIdRtx) =>
    `a=ssrc:${spatialLayerId} cname:${cname}\r\n` +
    `a=ssrc:${spatialLayerId} msid:${msid}\r\n` +
    `a=ssrc:${spatialLayerId} mslabel:${mslabel}\r\n` +
    `a=ssrc:${spatialLayerId} label:${label}\r\n` +
    `a=ssrc:${spatialLayerIdRtx} cname:${cname}\r\n` +
    `a=ssrc:${spatialLayerIdRtx} msid:${msid}\r\n` +
    `a=ssrc:${spatialLayerIdRtx} mslabel:${mslabel}\r\n` +
    `a=ssrc:${spatialLayerIdRtx} label:${label}\r\n`;

  const enableSimulcast = (sdpInput) => {
    let result;
    let sdp = sdpInput;
    if (!spec.video) {
      return sdp;
    }
    if (!spec.simulcast) {
      return sdp;
    }

    // TODO(javier): Improve the way we check for current video ssrcs
    const matchGroup = sdp.match(/a=ssrc-group:FID ([0-9]*) ([0-9]*)\r?\n/);
    if (!matchGroup || (matchGroup.length <= 0)) {
      return sdp;
    }

    const numSpatialLayers = spec.simulcast.numSpatialLayers || defaultSimulcastSpatialLayers;
    const baseSsrc = parseInt(matchGroup[1], 10);
    const baseSsrcRtx = parseInt(matchGroup[2], 10);
    const cname = sdp.match(new RegExp(`a=ssrc:${matchGroup[1]} cname:(.*)\r?\n`))[1];
    const msid = sdp.match(new RegExp(`a=ssrc:${matchGroup[1]} msid:(.*)\r?\n`))[1];
    const mslabel = sdp.match(new RegExp(`a=ssrc:${matchGroup[1]} mslabel:(.*)\r?\n`))[1];
    const label = sdp.match(new RegExp(`a=ssrc:${matchGroup[1]} label:(.*)\r?\n`))[1];

    sdp.match(new RegExp(`a=ssrc:${matchGroup[1]}.*\r?\n`, 'g')).forEach((line) => {
      sdp = sdp.replace(line, '');
    });
    sdp.match(new RegExp(`a=ssrc:${matchGroup[2]}.*\r?\n`, 'g')).forEach((line) => {
      sdp = sdp.replace(line, '');
    });

    const spatialLayers = [baseSsrc];
    const spatialLayersRtx = [baseSsrcRtx];

    for (let i = 1; i < numSpatialLayers; i += 1) {
      spatialLayers.push(baseSsrc + (i * 1000));
      spatialLayersRtx.push(baseSsrcRtx + (i * 1000));
    }

    result = addSim(spatialLayers);
    let spatialLayerId;
    let spatialLayerIdRtx;
    for (let i = 0; i < spatialLayers.length; i += 1) {
      spatialLayerId = spatialLayers[i];
      spatialLayerIdRtx = spatialLayersRtx[i];
      result += addGroup(spatialLayerId, spatialLayerIdRtx);
    }

    for (let i = 0; i < spatialLayers.length; i += 1) {
      spatialLayerId = spatialLayers[i];
      spatialLayerIdRtx = spatialLayersRtx[i];
      result += addSpatialLayer(cname, msid, mslabel, label, spatialLayerId, spatialLayerIdRtx);
    }
    result += 'a=x-google-flag:conference\r\n';
    return sdp.replace(matchGroup[0], result);
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

  const enableOpusNacks = (sdpInput) => {
    let sdp = sdpInput;
    const sdpMatch = sdp.match(/a=rtpmap:(.*)opus.*\r\n/);
    if (sdpMatch !== null) {
      const theLine = `${sdpMatch[0]}a=rtcp-fb:${sdpMatch[1]}nack\r\n`;
      sdp = sdp.replace(sdpMatch[0], theLine);
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

  let localDesc;
  let remoteDesc;

  const setLocalDesc = (isSubscribe, sessionDescription) => {
    localDesc = sessionDescription;
    if (!isSubscribe) {
      localDesc.sdp = enableSimulcast(localDesc.sdp);
    }
    localDesc.sdp = setMaxBW(localDesc.sdp);
    localDesc.sdp = enableOpusNacks(localDesc.sdp);
    localDesc.sdp = localDesc.sdp.replace(/a=ice-options:google-ice\r\n/g, '');
    spec.callback({
      type: localDesc.type,
      sdp: localDesc.sdp,
    });
    // that.peerConnection.setLocalDescription(localDesc);
  };

  const setLocalDescp2p = (sessionDescription) => {
    localDesc = sessionDescription;
    localDesc.sdp = setMaxBW(localDesc.sdp);
    spec.callback({
      type: localDesc.type,
      sdp: localDesc.sdp,
    });
    that.peerConnection.setLocalDescription(localDesc);
  };

  that.updateSpec = (configInput, callback) => {
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

      localDesc.sdp = setMaxBW(localDesc.sdp);
      if (config.Sdp || config.maxAudioBW) {
        L.Logger.debug('Updating with SDP renegotiation', spec.maxVideoBW, spec.maxAudioBW);
        that.peerConnection.setLocalDescription(localDesc, () => {
          remoteDesc.sdp = setMaxBW(remoteDesc.sdp);
          that.peerConnection.setRemoteDescription(
                      new RTCSessionDescription(remoteDesc), () => {
                        spec.remoteDescriptionSet = true;
                        spec.callback({ type: 'updatestream', sdp: localDesc.sdp });
                      });
        }, (error) => {
          L.Logger.error('Error updating configuration', error);
          callback('error');
        });
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
    if (isSubscribe === true) {
      that.peerConnection.createOffer(setLocalDesc.bind(null, isSubscribe), errorCallback,
                that.mediaConstraints);
    } else {
      that.mediaConstraints = {
        mandatory: {
          OfferToReceiveVideo: false,
          OfferToReceiveAudio: false,
        },
      };
      that.peerConnection.createOffer(setLocalDesc.bind(null, isSubscribe), errorCallback,
                that.mediaConstraints);
    }
  };

  that.addStream = (stream) => {
    that.peerConnection.addStream(stream);
  };
  spec.remoteCandidates = [];

  spec.remoteDescriptionSet = false;

  that.processSignalingMessage = (msgInput) => {
    const msg = msgInput;
    // L.Logger.info("Process Signaling Message", msg);

    if (msg.type === 'offer') {
      msg.sdp = setMaxBW(msg.sdp);
      that.peerConnection.setRemoteDescription(new RTCSessionDescription(msg), () => {
        that.peerConnection.createAnswer(setLocalDescp2p, (error) => {
          L.Logger.error('Error: ', error);
        }, that.mediaConstraints);
        spec.remoteDescriptionSet = true;
      }, (error) => {
        L.Logger.error('Error setting Remote Description', error);
      });
    } else if (msg.type === 'answer') {
      L.Logger.info('Set remote and local description');
      L.Logger.debug('Remote Description', msg.sdp);
      L.Logger.debug('Local Description', localDesc.sdp);

      msg.sdp = setMaxBW(msg.sdp);

      remoteDesc = msg;
      that.peerConnection.setLocalDescription(localDesc, () => {
        that.peerConnection.setRemoteDescription(
                  new RTCSessionDescription(msg), () => {
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
                  });
      });
    } else if (msg.type === 'candidate') {
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
    }
  };

  return that;
};
