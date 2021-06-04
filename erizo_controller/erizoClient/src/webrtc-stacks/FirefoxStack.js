import Logger from '../utils/Logger';
import BaseStack from './BaseStack';

const log = Logger.module('FirefoxStack');

const defaultSimulcastSpatialLayers = 3;

const possibleLayers = [
  { rid: '3' },
  { rid: '2', scaleResolutionDownBy: 2 },
  { rid: '1', scaleResolutionDownBy: 4 },
];

const FirefoxStack = (specInput) => {
  log.debug('message: Starting Firefox stack');
  const that = BaseStack(specInput);

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

  const getSimulcastParametersForFirefox = (sender, streamInput) => {
    const parameters = sender.getParameters() || {};
    parameters.encodings = getSimulcastParameters(streamInput);

    return sender.setParameters(parameters);
  };

  that.addStream = (streamInput) => {
    const nativeStream = streamInput.stream;
    nativeStream.transceivers = [];
    nativeStream.getTracks().forEach(async (track) => {
      let options = {};
      if (track.kind === 'video' && streamInput.simulcast) {
        options = {
          sendEncodings: [],
        };
      }
      options.streams = [nativeStream];
      const transceiver = that.peerConnection.addTransceiver(track, options);
      nativeStream.transceivers.push(transceiver);
      if (track.kind === 'video' && streamInput.simulcast) {
        getSimulcastParametersForFirefox(transceiver.sender, streamInput).catch(() => {});
      }
    });
  };

  that.prepareCreateOffer = () => Promise.resolve();

  return that;
};

export default FirefoxStack;
