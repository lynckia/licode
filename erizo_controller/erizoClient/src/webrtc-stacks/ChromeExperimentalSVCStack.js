import BaseStack from './BaseStack';
import SdpHelpers from './../utils/SdpHelpers';
import Logger from '../utils/Logger';
import { addLegacySimulcast } from '../utils/unifiedPlanUtils';
const sdpTransform = require('sdp-transform');

const log = Logger.module('ChromeStableStack');

const ChromeExperitmentalSVCStack = (specInput) => {
  log.debug(`message: Starting Chrome experimental stack, spec: ${JSON.stringify(specInput)}`);
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

  that.peerConnection.onnegotiationneeded = async () => {
    // This is for testing the negotiation step by step
    if (specInput.managed) {
      return;
    }
    try {
      let  offer = await that.peerConnection.createOffer();

      let localOffer = sdpTransform.parse(offer.sdp);
      let offerMediaObject = localOffer.media[localOffer.media.length - 1];
      addLegacySimulcast({offerMediaObject, numStreams:3});
      offer = { type: 'offer', sdp: sdpTransform.write(localOffer) };
      await that.peerConnection.setLocalDescription(offer);
      spec.callback({
        type: that.peerConnection.localDescription.type,
        sdp: that.peerConnection.localDescription.sdp,
      });
    } catch (e) {
      log.error('onnegotiationneeded - error', e.message);
    }
  };

  return that;
};

export default ChromeExperitmentalSVCStack;
