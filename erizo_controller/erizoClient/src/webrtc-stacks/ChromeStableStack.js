import BaseStack from './BaseStack';
import SdpHelpers from './../utils/SdpHelpers';
import Logger from '../utils/Logger';

const ChromeStableStack = (specInput) => {
  Logger.info('Starting Chrome stable stack', specInput);
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

  const setBitrateForVideoLayers = (sender) => {
    if (typeof sender.getParameters !== 'function' || typeof sender.setParameters !== 'function') {
      Logger.warning('Cannot set simulcast layers bitrate: getParameters or setParameters is not available');
      return;
    }
    const parameters = sender.getParameters();
    Object.keys(that.simulcast.spatialLayerBitrates).forEach((key) => {
      if (parameters.encodings[key] !== undefined) {
        Logger.debug(`Setting bitrate for layer ${key}, bps: ${that.simulcast.spatialLayerBitrates[key]}`);
        parameters.encodings[key].maxBitrate = that.simulcast.spatialLayerBitrates[key];
      }
    });
    sender.setParameters(parameters)
      .then((result) => {
        Logger.debug('Success setting simulcast layer bitrates', result);
      })
      .catch((e) => {
        Logger.warning('Error setting simulcast layer bitrates', e);
      });
  };

  that.setSimulcastLayersBitrate = () => {
    Logger.debug('Maybe set simulcast Layers bitrate', that.simulcast);
    if (that.simulcast && that.simulcast.spatialLayerBitrates) {
      that.peerConnection.getSenders().forEach((sender) => {
        if (sender.track.kind === 'video') {
          setBitrateForVideoLayers(sender);
        }
      });
    }
  };

  that.setStartVideoBW = (sdpInfo) => {
    if (that.video && spec.startVideoBW) {
      Logger.debug(`startVideoBW requested: ${spec.startVideoBW}`);
      SdpHelpers.setParamForCodecs(sdpInfo, 'video', 'x-google-start-bitrate', spec.startVideoBW);
    }
  };

  that.setHardMinVideoBW = (sdpInfo) => {
    if (that.video && spec.hardMinVideoBW) {
      Logger.debug(`hardMinVideoBW requested: ${spec.hardMinVideoBW}`);
      SdpHelpers.setParamForCodecs(sdpInfo, 'video', 'x-google-min-bitrate', spec.hardMinVideoBW);
    }
  };

  return that;
};

export default ChromeStableStack;
