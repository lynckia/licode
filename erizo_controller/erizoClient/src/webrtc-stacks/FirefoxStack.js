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

  that.enableSimulcast = sdp => sdp;

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

  that.addStream = (streamInput) => {
    const stream = streamInput;
    stream.transceivers = [];
    stream.getTracks().forEach(async (track) => {
      let options = {};
      if (track.kind === 'video' && that.simulcast) {
        options = {
          sendEncodings: [],
        };
      }
      options.streams = [stream];
      const transceiver = that.peerConnection.addTransceiver(track, options);
      stream.transceivers.push(transceiver);
      if (track.kind === 'video' && that.simulcast) {
        getSimulcastParametersForFirefox(transceiver.sender).catch(() => {});
      }
    });
  };

  that.prepareCreateOffer = () => Promise.resolve();

  return that;
};

export default FirefoxStack;
