import Logger from '../utils/Logger';
import BaseStack from './BaseStack';

const log = Logger.module('FirefoxStack');

const defaultSimulcastSpatialLayers = 3;

const scaleResolutionDownBase = 2;
const scaleResolutionDownBaseScreenshare = 1;

const FirefoxStack = (specInput) => {
  log.debug('message: Starting Firefox stack');
  const that = BaseStack(specInput);

  that.enableSimulcast = sdp => sdp;

  const getSimulcastParameters = (isScreenshare) => {
    const numSpatialLayers = that.simulcast.numSpatialLayers || defaultSimulcastSpatialLayers;
    const parameters = [];
    const base = isScreenshare ? scaleResolutionDownBaseScreenshare : scaleResolutionDownBase;

    for (let layer = 1; layer <= numSpatialLayers; layer += 1) {
      parameters.push({
        rid: (layer).toString(),
        scaleResolutionDownBy: base ** (numSpatialLayers - layer),
      });
    }
    return parameters;
  };

  const getSimulcastParametersForFirefox = (sender, isScreenshare) => {
    const parameters = sender.getParameters() || {};
    parameters.encodings = getSimulcastParameters(isScreenshare);

    return sender.setParameters(parameters);
  };

  that.addStream = (streamInput, isScreenshare) => {
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
        getSimulcastParametersForFirefox(transceiver.sender, isScreenshare).catch(() => {});
      }
    });
  };

  that.prepareCreateOffer = () => Promise.resolve();

  return that;
};

export default FirefoxStack;
