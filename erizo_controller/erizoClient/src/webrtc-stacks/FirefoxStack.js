import Logger from '../utils/Logger';
import BaseStack from './BaseStack';

const log = Logger.module('FirefoxStack');
const FirefoxStack = (specInput) => {
  log.debug('message: Starting Firefox stack');
  const that = BaseStack(specInput);
  const defaultSimulcastSpatialLayers = 2;

  const possibleLayers = [
    { rid: 'low', scaleResolutionDownBy: 3 },
    { rid: 'med', scaleResolutionDownBy: 2 },
    { rid: 'high' },
  ];

  const getSimulcastParameters = (sender) => {
    let numSpatialLayers = that.simulcast.numSpatialLayers || defaultSimulcastSpatialLayers;
    const totalLayers = possibleLayers.length;
    numSpatialLayers = numSpatialLayers < totalLayers ?
      numSpatialLayers : totalLayers;
    const parameters = sender.getParameters() || {};
    parameters.encodings = [];

    for (let layer = totalLayers - 1; layer >= totalLayers - numSpatialLayers; layer -= 1) {
      parameters.encodings.push(possibleLayers[layer]);
    }
    return sender.setParameters(parameters);
  };

  const enableSimulcast = () => {
    if (!that.simulcast) {
      return [];
    }
    const promises = [];
    that.peerConnection.getSenders().forEach((sender) => {
      if (sender.track.kind === 'video') {
        promises.push(getSimulcastParameters(sender));
      }
    });
    return promises;
  };

  that.enableSimulcast = sdp => sdp;

  that.prepareCreateOffer = (isSubscribe = false) => {
    let promises = [];
    if (isSubscribe !== true) {
      promises = enableSimulcast();
    }
    return Promise.all(promises);
  };

  return that;
};

export default FirefoxStack;
