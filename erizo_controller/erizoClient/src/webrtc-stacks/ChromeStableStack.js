import BaseStack from './BaseStack';
import SdpHelpers from './../utils/SdpHelpers';
import Logger from '../utils/Logger';

const log = Logger.module('ChromeStableStack');

const ChromeStableStack = (specInput) => {
  log.debug(`message: Starting Chrome stable stack, spec: ${JSON.stringify(specInput)}`);
  const spec = specInput;
  const that = BaseStack(specInput);
  that.mediaConstraints = {
    offerToReceiveVideo: true,
    offerToReceiveAudio: true,
  };

  that.prepareCreateOffer = () => Promise.resolve();

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
