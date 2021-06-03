import BaseStack from './BaseStack';
import SdpHelpers from './../utils/SdpHelpers';
import Logger from '../utils/Logger';

const log = Logger.module('ChromeStableStack');

const ChromeStableStack = (specInput) => {
  log.debug(`message: Starting Chrome stable stack, spec: ${JSON.stringify(specInput)}`);
  const spec = specInput;
  const that = BaseStack(specInput);
  const defaultSimulcastSpatialLayers = 2;
  that.mediaConstraints = {
    offerToReceiveVideo: true,
    offerToReceiveAudio: true,
  };

  const configureParameter = (parameters, config, layerId) => {
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

  const setBitrateForVideoLayers = (sender, streamInput) => {
    if (typeof sender.getParameters !== 'function' || typeof sender.setParameters !== 'function') {
      log.warning('message: Cannot set simulcast layers bitrate, reason: get/setParameters not available');
      return;
    }
    let parameters = sender.getParameters();
    const spatialLayerConfigs = streamInput.getSimulcastConfig().spatialLayerConfigs;
    Object.keys(spatialLayerConfigs).forEach((layerId) => {
      if (parameters.encodings[layerId] !== undefined) {
        log.warning(`message: Configure parameters for layer, layer: ${layerId}, config: ${spatialLayerConfigs[layerId]}`);
        parameters = configureParameter(parameters, spatialLayerConfigs, layerId);
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

  that.prepareCreateOffer = () => Promise.resolve();

  that.setSimulcastLayersConfig = (streamInput) => {
    log.debug(`message: Maybe set simulcast Layers config, simulcast: ${JSON.stringify(that.simulcast)}`);
    if (streamInput.getSimulcastConfig()) {
      streamInput.stream.transceivers.forEach((transceiver) => {
        if (transceiver.sender.track.kind === 'video') {
          setBitrateForVideoLayers(transceiver.sender, streamInput);
        }
      });
    }
  };

  that.setStartVideoBW = (sdpInfo) => {
    if (that.video && spec.startVideoBW) {
      log.debug(`message: startVideoBW, requested: ${spec.startVideoBW}`);
      SdpHelpers.setParamForCodecs(sdpInfo, 'video', 'x-google-start-bitrate', spec.startVideoBW);
    }
  };

  that.setHardMinVideoBW = (sdpInfo) => {
    if (that.video && spec.hardMinVideoBW) {
      log.debug(`message: hardMinVideoBW, requested: ${spec.hardMinVideoBW}`);
      SdpHelpers.setParamForCodecs(sdpInfo, 'video', 'x-google-min-bitrate', spec.hardMinVideoBW);
    }
  };

  return that;
};

export default ChromeStableStack;
