/* global RTCIceCandidate, RTCPeerConnection */

// eslint-disable-next-line
// import SemanticSdp from '../../../common/semanticSdp/SemanticSdp';
// import Setup from '../../../common/semanticSdp/Setup';
// import Direction from '../../../common/semanticSdp/Direction';

import PeerConnectionFsm from './PeerConnectionFsm';

// import SdpHelpers from '../utils/SdpHelpers';
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

  // private functions
  that._updateTracksToBeNegotiatedFromStream = (stream) => {
    that.tracksToBeNegotiated += stream.getTracks().length;
  };
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

  const defaultSimulcastSpatialLayers = 3;

  const possibleLayers = [
    { rid: '3', scaleResolutionDownBy: 3 },
    { rid: '2', scaleResolutionDownBy: 2 },
    { rid: '1' },
  ];

  const getSimulcastParameters = () => {
    let numSpatialLayers = that.simulcast.numSpatialLayers || defaultSimulcastSpatialLayers;
    const totalLayers = possibleLayers.length;
    numSpatialLayers = numSpatialLayers < totalLayers ?
      numSpatialLayers : totalLayers;
    const parameters = [];

    for (let layer = totalLayers - 1; layer >= totalLayers - numSpatialLayers; layer -= 1) {
      parameters.push(possibleLayers[layer]);
    }
    return parameters;
  };

  const getSimulcastParametersForFirefox = (sender) => {
    const parameters = sender.getParameters() || {};
    parameters.encodings = getSimulcastParameters();

    return sender.setParameters(parameters);
  };

  that.addStream = (stream) => {
    stream.getTracks().forEach(async (track) => {
      let options = {};
      if (track.kind === 'video' && that.simulcast) {
        options = {
          sendEncodings: getSimulcastParameters(),
        };
      }
      options.streams = [stream];
      const transceiver = that.peerConnection.addTransceiver(track, options);
      getSimulcastParametersForFirefox(transceiver.sender).catch(() => {});
    });
  };

  that.removeStream = (stream) => {
    that.peerConnection.removeStream(stream);
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

  that.peerConnectionFsm = new PeerConnectionFsm(that.protectedCalls);
  return that;
};

export default BaseStack;
