import Logger from '../utils/Logger';
import BaseStack from './BaseStack';
import SemanticSdp from '../../../common/semanticSdp/SemanticSdp';
import SdpHelpers from '../utils/SdpHelpers';

const FirefoxStack = (specInput) => {
  Logger.info('Starting Firefox stack');
  const that = BaseStack(specInput);
  const spec = specInput;
  const defaultSimulcastSpatialLayers = 2;

  const possibleLayers = [
    { rid: 'low', scaleResolutionDownBy: 3 },
    { rid: 'med', scaleResolutionDownBy: 2 },
    { rid: 'high' },
  ];

  const getSimulcastParameters = (sender) => {
    let numSpatialLayers = spec.simulcast.numSpatialLayers || defaultSimulcastSpatialLayers;
    const totalLayers = possibleLayers.length;
    numSpatialLayers = numSpatialLayers < totalLayers ?
                          numSpatialLayers : totalLayers;
    const parameters = sender.getParameters() || {};
    parameters.encodings = [];

    for (let layer = totalLayers - 1; layer >= totalLayers - numSpatialLayers; layer -= 1) {
      parameters.encodings.push(possibleLayers[layer]);
    }
    sender.setParameters(parameters);
  };

  const enableSimulcast = () => {
    if (!spec.simulcast) {
      return;
    }
    that.peerConnection.getSenders().forEach((sender) => {
      if (sender.track.kind === 'video') {
        getSimulcastParameters(sender);
      }
    });
  };

  that.enableSimulcast = sdp => sdp;

  const baseCreateOffer = that.createOffer;

  that.createOffer = (isSubscribe) => {
    if (isSubscribe !== true) {
      enableSimulcast();
    }
    baseCreateOffer(isSubscribe);
  };

  const baseSetLocalDescForOffer = that.setLocalDescForOffer;

  that.setLocalDescForOffer = (isSubscribe, sessionDescription) => {
    const sdp = SemanticSdp.SDPInfo.processString(sessionDescription.sdp);
    if (isSubscribe) {
      SdpHelpers.forceDirection(sdp, 'recvonly');
    } else {
      SdpHelpers.forceDirection(sdp, 'sendonly');
    }
    baseSetLocalDescForOffer(isSubscribe, sdp.toString());
  };

  const baseSetLocalDescForAnswerp2p = that.setLocalDescForAnswerp2p;

  that.setLocalDescForAnswerp2p = (sessionDescription) => {
    const sdp = SemanticSdp.SDPInfo.processString(sessionDescription.sdp);
    SdpHelpers.forceDirection(sdp, 'recvonly');
    baseSetLocalDescForAnswerp2p(sdp.toString());
  };

  return that;
};

export default FirefoxStack;
