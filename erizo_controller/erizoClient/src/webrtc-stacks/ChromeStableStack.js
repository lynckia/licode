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

  that.enableSimulcast = (sdpInput) => {
    let result;
    let sdp = sdpInput;
    if (!that.simulcast) {
      return sdp;
    }
    const hasAlreadySetSimulcast = sdp.match(new RegExp('a=ssrc-group:SIM', 'g')) !== null;
    if (hasAlreadySetSimulcast) {
      return sdp;
    }
    // TODO(javier): Improve the way we check for current video ssrcs
    const matchGroup = sdp.match(/a=ssrc-group:FID ([0-9]*) ([0-9]*)\r?\n/);
    if (!matchGroup || (matchGroup.length <= 0)) {
      return sdp;
    }
    // TODO (pedro): Consider adding these to SdpHelpers
    const numSpatialLayers = that.simulcast.numSpatialLayers || defaultSimulcastSpatialLayers;
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

    result = SdpHelpers.addSim(spatialLayers);
    let spatialLayerId;
    let spatialLayerIdRtx;
    for (let i = 0; i < spatialLayers.length; i += 1) {
      spatialLayerId = spatialLayers[i];
      spatialLayerIdRtx = spatialLayersRtx[i];
      result += SdpHelpers.addGroup(spatialLayerId, spatialLayerIdRtx);
    }

    for (let i = 0; i < spatialLayers.length; i += 1) {
      spatialLayerId = spatialLayers[i];
      spatialLayerIdRtx = spatialLayersRtx[i];
      result += SdpHelpers.addSpatialLayer(cname,
        msid, mslabel, label, spatialLayerId, spatialLayerIdRtx);
    }
    result += 'a=x-google-flag:conference\r\n';
    return sdp.replace(matchGroup[0], result);
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

  const setBitrateForVideoLayers = (sender) => {
    if (typeof sender.getParameters !== 'function' || typeof sender.setParameters !== 'function') {
      log.warning('message: Cannot set simulcast layers bitrate, reason: get/setParameters not available');
      return;
    }
    let parameters = sender.getParameters();
    Object.keys(that.simulcast.spatialLayerConfigs).forEach((layerId) => {
      if (parameters.encodings[layerId] !== undefined) {
        log.debug(`message: Configure parameters for layer, layer: ${layerId}, config: ${that.simulcast.spatialLayerConfigs[layerId]}`);
        parameters = configureParameter(parameters, that.simulcast.spatialLayerConfigs, layerId);
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

  that.setSimulcastLayersConfig = () => {
    log.debug(`message: Maybe set simulcast Layers config, simulcast: ${JSON.stringify(that.simulcast)}`);
    if (that.simulcast && that.simulcast.spatialLayerConfigs) {
      that.peerConnection.getSenders().forEach((sender) => {
        if (sender.track.kind === 'video') {
          setBitrateForVideoLayers(sender);
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
