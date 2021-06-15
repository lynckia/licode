import Logger from '../utils/Logger';
import BaseStack from './BaseStack';

const log = Logger.module('FirefoxStack');

const defaultSimulcastSpatialLayers = 3;

const scaleResolutionDownBase = 2;
const scaleResolutionDownBaseScreenshare = 1;

const FirefoxStack = (specInput) => {
  log.debug('message: Starting Firefox stack');
  const that = BaseStack(specInput);

  const getSimulcastParameters = (streamInput) => {
    const numSpatialLayers = Object.keys(streamInput.getSimulcastConfig()).length ||
      defaultSimulcastSpatialLayers;
    const parameters = [];
    const isScreenshare = streamInput.hasScreen();
    const base = isScreenshare ? scaleResolutionDownBaseScreenshare : scaleResolutionDownBase;

    for (let layer = 1; layer <= numSpatialLayers; layer += 1) {
      parameters.push({
        rid: (layer).toString(),
        scaleResolutionDownBy: base ** (numSpatialLayers - layer),
      });
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
