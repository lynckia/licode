/* global RTCIceCandidate, RTCPeerConnection */

import Logger from '../utils/Logger';

const log = Logger.module('BaseStack');

const BaseStack = (specInput) => {
  const that = {};
  const specBase = specInput;

  const logs = [];
  const logSDP = (...message) => {
    logs.push(['Negotiation:', ...message].reduce((a, b) => `${a} ${b}`));
  };
  that.getNegotiationLogs = () => logs.reduce((a, b) => `${a}'\n'${b}`);

  log.debug(`message: Starting Base stack, spec: ${JSON.stringify(specBase)}`);

  that.pcConfig = {
    iceServers: [],
    sdpSemantics: 'unified-plan',
  };

  that.con = {};
  if (specBase.iceServers !== undefined) {
    that.pcConfig.iceServers = specBase.iceServers;
  }
  if (specBase.forceTurn === true) {
    that.pcConfig.iceTransportPolicy = 'relay';
  }
  that.pcConfig.bundlePolicy = 'max-bundle';
  that.audio = specBase.audio;
  that.video = specBase.video;
  if (that.audio === undefined) {
    that.audio = true;
  }
  if (that.video === undefined) {
    that.video = true;
  }

  that.peerConnection = new RTCPeerConnection(that.pcConfig, that.con);

  that.peerConnection.onnegotiationneeded = async () => {
    // This is for testing the negotiation step by step
    if (specInput.managed) {
      return;
    }
    try {
      await that.peerConnection.setLocalDescription();
      specBase.callback({
        type: that.peerConnection.localDescription.type,
        sdp: that.peerConnection.localDescription.sdp,
      });
    } catch (e) {
      logSDP('onnegotiationneeded - error', e.message);
    }
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

    specBase.callback({ type: 'candidate', candidate: candidateObject });
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

  that.setVideo = (video) => {
    that.video = video;
  };

  that.setAudio = (audio) => {
    that.audio = audio;
  };

  that.updateSpec = (configInput, streamId) => {
    const config = configInput;
    const shouldSendMaxVideoBWInOptions = !specBase.p2p && config.maxVideoBW;
    if (shouldSendMaxVideoBWInOptions ||
        config.minVideoBW ||
        (config.slideShowMode !== undefined) ||
        (config.muteStream !== undefined) ||
        (config.qualityLayer !== undefined) ||
        (config.slideShowBelowLayer !== undefined) ||
        (config.video !== undefined) ||
        (config.priorityLevel !== undefined)) {
      log.debug(`message: Configuration changed, maxVideoBW: ${config.maxVideoBW}` +
        `, minVideoBW: ${config.minVideoBW}, slideShowMode: ${config.slideShowMode}` +
        `, muteStream: ${JSON.stringify(config.muteStream)}, videoConstraints: ${JSON.stringify(config.video)}` +
        `, slideShowBelowMinLayer: ${config.slideShowBelowLayer}` +
        `, priority: ${config.priorityLevel}`);
      specBase.callback({ type: 'updatestream', config }, streamId);
    }
  };

  const supportedLayerConfiguration = [
    { rid: '3' },
    { rid: '2', scaleResolutionDownBy: 2 },
    { rid: '1', scaleResolutionDownBy: 4 },
  ];

  that.getSupportedLayerConfiguration = () => supportedLayerConfiguration;

  const translateSimulcastParameters = (streamInput) => {
    const parameters = [];
    const totalLayers = supportedLayerConfiguration.length;
    const requestedLayers = Object.keys(streamInput.getSimulcastConfig()).length;
    const layersToUse = requestedLayers < totalLayers ? requestedLayers : totalLayers;

    for (let layer = totalLayers - 1; layer >= totalLayers - layersToUse; layer -= 1) {
      const layerConfig = Object.assign({}, streamInput.getSimulcastConfig()[layer]);
      Object.assign(layerConfig, supportedLayerConfiguration[layer]);
      console.warn('LAyerConfig is', layerConfig);
      parameters.push(layerConfig);
    }
    console.warn('Parameters translated', parameters, 'simulcastConfig', streamInput.getSimulcastConfig());
    return parameters;
  };

  that.configureParameterForLayer = (parameters, config, layerId) => {
    if (parameters.encodings[layerId] === undefined ||
        config[layerId] === undefined) {
      return parameters;
    }
    const newParameters = parameters;
    newParameters.encodings[layerId].maxBitrate = config[layerId].maxBitrate;
    if (config[layerId].active !== undefined) {
      newParameters.encodings[layerId].active = config[layerId].active;
    }
    return newParameters;
  };


  that.addStream = (streamInput) => {
    const nativeStream = streamInput.stream;
    nativeStream.transceivers = [];
    nativeStream.getTracks().forEach(async (track) => {
      let options = {};
      if (track.kind === 'video' && streamInput.hasSimulcast()) {
        options = {
          sendEncodings: translateSimulcastParameters(streamInput),
        };
      }
      options.streams = [nativeStream];
      const transceiver = that.peerConnection.addTransceiver(track, options);
      nativeStream.transceivers.push(transceiver);
    });
  };

  that.removeStream = (nativeStream) => {
    nativeStream.transceivers.forEach((transceiver) => {
      log.error('Stopping transceiver', transceiver);
      // Don't remove the tagged m section, which is the first one (mid=0).
      if (transceiver.mid === '0') {
        that.peerConnection.removeTrack(transceiver.sender);
      } else {
        transceiver.stop();
      }
    });
  };

  that.close = () => {
    that.peerConnection.close();
  };

  that.setLocalDescription = async () => that.peerConnection.setLocalDescription();

  that.getLocalDescription = async () => that.peerConnection.localDescription.sdp;

  that.setRemoteDescription = async (description) => {
    console.warn('SetRemoteDescription', description);
    await that.peerConnection.setRemoteDescription(description);
  };

  // const setMediaBitrate = (sdp, media, bitrate) => {
  //   const lines = sdp.split('\n');
  //   let line = -1;
  //   for (let i = 0; i < lines.length; i += 1) {
  //     if (lines[i].indexOf(`a=mid:${media}`) === 0) {
  //       line = i;
  //       break;
  //     }
  //   }
  //   if (line === -1) {
  //     console.warn('Could not find the m line for', media);
  //     return sdp;
  //   }
  //   console.warn('Found the m line for', media, 'at line', line);

  //   // Pass the m line
  //   line += 1;

  //   // Skip i and c lines
  //   while (lines[line].indexOf('i=') === 0 || lines[line].indexOf('c=') === 0) {
  //     line += 1;
  //   }

  //   // If we're on a b line, replace it
  //   if (lines[line].indexOf('b') === 0) {
  //     console.warn('Replaced b line at line', line);
  //     lines[line] = `b=AS:${bitrate}`;
  //     return lines.join('\n');
  //   }

  //   // Add a new b line
  //   console.warn('Adding new b line before line', line);
  //   let newLines = lines.slice(0, line);
  //   newLines.push(`b=AS:${bitrate}`);
  //   console.warn('newLinesbefore', newLines);
  //   newLines = newLines.concat(lines.slice(line, lines.length));
  //   console.warn('newLines', newLines);
  //   return newLines.join('\n');
  // };


  that.processSignalingMessage = async (msgInput) => {
    const msg = msgInput;
    logSDP('processSignalingMessage, type: ', msgInput.type);
    if (msgInput.type === 'candidate') {
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
      that.peerConnection.addIceCandidate(candidate);
    } else if (msgInput.type === 'error') {
      log.error(`message: Received error signaling message, state: ${msgInput.previousType}`);
    } else if (['offer', 'answer'].indexOf(msgInput.type) !== -1) {
      try {
        if (msg.type === 'answer') {
          // msg.sdp = setMediaBitrate(msg.sdp, '1', 500);
          // msg.sdp = setMediaBitrate(msg.sdp, '2', 1500);
          // console.warn('Setting answer', msg.sdp);
        }
        await that.peerConnection.setRemoteDescription(msg);
        if (msg.type === 'offer') {
          await that.peerConnection.setLocalDescription();
          specBase.callback({
            type: that.peerConnection.localDescription.type,
            sdp: that.peerConnection.localDescription.sdp,
          });
        }
      } catch (e) {
        log.error('message: Error during negotiation, message: ', e.message);
        if (msg.type === 'offer') {
          specBase.callback({
            type: 'offer-error',
            sdp: that.peerConnection.localDescription.sdp,
          });
        }
      }
    }
  };

  that.restartIce = () => {
    that.peerConnection.restartIce();
    that.peerConnection.onnegotiationneeded();
  };

  return that;
};

export default BaseStack;
