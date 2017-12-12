import Logger from '../utils/Logger';
import BaseStack from './BaseStack';

const possibleLayers = [
  { rid: 'high' },
  { rid: 'med', scaleResolutionDownBy: 2 },
  { rid: 'low', scaleResolutionDownBy: 3 },
];

const FirefoxStack = (specInput) => {
  Logger.info('Starting Firefox stack');
  const that = BaseStack(specInput);
  const spec = specInput;
  const defaultSimulcastSpatialLayers = 2;

  const getSimulcastParameters = (sender) => {
    let numSpatialLayers = spec.simulcast.numSpatialLayers || defaultSimulcastSpatialLayers;
    const totalLayers = possibleLayers.length;
    numSpatialLayers = numSpatialLayers < totalLayers ?
                          numSpatialLayers : totalLayers;
    const parameters = sender.getParameters() || {};
    parameters.encodings = [];

    for (let layer = totalLayers - numSpatialLayers; layer < totalLayers; layer += 1) {
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

  return that;
};

export default FirefoxStack;
