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
        config: { maxVideoBW: specBase.maxVideoBW },
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

  const setSpatialLayersConfig = (field, values, stream, check = () => true) => {
    if (stream.hasSimulcast()) {
      Object.keys(values).forEach((layerId) => {
        const value = values[layerId];
        const spatialLayerConfigs = stream.getSimulcastConfig().spatialLayerConfigs || {};
        if (!spatialLayerConfigs[layerId]) {
          spatialLayerConfigs[layerId] = {};
        }
        if (check(value)) {
          spatialLayerConfigs[layerId][field] = value;
        }
        stream.setSpatialLayersConfigs(spatialLayerConfigs);
      });
      that.setSimulcastLayersConfig(stream);
    }
  };

  that.updateSimulcastLayersBitrate = (bitrates, licodeStream) => {
    setSpatialLayersConfig('maxBitrate', bitrates, licodeStream);
  };

  that.updateSimulcastActiveLayers = (layersInfo, licodeStream) => {
    const ifIsBoolean = value => value === true || value === false;
    setSpatialLayersConfig('active', layersInfo, licodeStream, ifIsBoolean);
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

  const defaultSimulcastSpatialLayers = 3;

  const possibleLayers = [
    { rid: '3' },
    { rid: '2', scaleResolutionDownBy: 2 },
    { rid: '1', scaleResolutionDownBy: 4 },
  ];

  const getSimulcastParameters = (streamInput) => {
    const config = streamInput.getSimulcastConfig();
    let numSpatialLayers = config.numSpatialLayers || defaultSimulcastSpatialLayers;
    const totalLayers = possibleLayers.length;
    numSpatialLayers = numSpatialLayers < totalLayers ?
      numSpatialLayers : totalLayers;
    const parameters = [];

    for (let layer = totalLayers - 1; layer >= totalLayers - numSpatialLayers; layer -= 1) {
      parameters.push(possibleLayers[layer]);
    }
    return parameters;
  };

  const configureParameterForLayer = (parameters, config, layerId) => {
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

  const setBitrateForVideoLayers = (sender, spatialLayerConfigs) => {
    if (typeof sender.getParameters !== 'function' || typeof sender.setParameters !== 'function') {
      log.warning('message: Cannot set simulcast layers bitrate, reason: get/setParameters not available');
      return;
    }
    let parameters = sender.getParameters();
    Object.keys(spatialLayerConfigs).forEach((layerId) => {
      if (parameters.encodings[layerId] !== undefined) {
        log.warning(`message: Configure parameters for layer, layer: ${layerId}, config: ${spatialLayerConfigs[layerId]}`);
        parameters = configureParameterForLayer(parameters, spatialLayerConfigs, layerId);
      }
    });
    sender.setParameters(parameters)
      .then((result) => {
        log.debug(`message: Success setting simulcast layer configs, result: ${result}`);
      })
      .catch((e) => {
        log.warning(`message: Error setting simulcast layer configs, error: ${e}`);
      });
  };

  that.setSimulcastLayersConfig = (streamInput) => {
    log.debug(`message: Maybe set simulcast Layers config, simulcast: ${JSON.stringify(that.simulcast)}`);
    if (streamInput.hasSimulcast() && streamInput.getSimulcastConfig().spatialLayerConfigs) {
      streamInput.stream.transceivers.forEach((transceiver) => {
        if (transceiver.sender.track.kind === 'video') {
          setBitrateForVideoLayers(
            transceiver.sender,
            streamInput.getSimulcastConfig().spatialLayerConfigs);
        }
      });
    }
  };

  that.addStream = (streamInput) => {
    const nativeStream = streamInput.stream;
    nativeStream.transceivers = [];
    nativeStream.getTracks().forEach(async (track) => {
      let options = {};
      if (track.kind === 'video' && streamInput.hasSimulcast()) {
        options = {
          sendEncodings: getSimulcastParameters(streamInput),
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
    await that.peerConnection.setRemoteDescription(description);
  };

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
        await that.peerConnection.setRemoteDescription(msg);
        if (msg.type === 'offer') {
          await that.peerConnection.setLocalDescription();
          specBase.callback({
            type: that.peerConnection.localDescription.type,
            sdp: that.peerConnection.localDescription.sdp,
            config: { maxVideoBW: specBase.maxVideoBW },
          });
        }
      } catch (e) {
        log.error('message: Error during negotiation, message: ', e.message);
        if (msg.type === 'offer') {
          specBase.callback({
            type: 'offer-error',
            sdp: that.peerConnection.localDescription.sdp,
            config: { maxVideoBW: specBase.maxVideoBW },
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
