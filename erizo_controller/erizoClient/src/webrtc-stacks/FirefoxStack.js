import Logger from '../utils/Logger';
import BaseStack from './BaseStack';

const log = Logger.module('FirefoxStack');


const FirefoxStack = (specInput) => {
  log.debug('message: Starting Firefox stack');
  const that = BaseStack(specInput);

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
      const parameters = transceiver.sender.getParameters() || {};
      parameters.encodings = streamInput.generateEncoderParameters();
      return transceiver.sender.setParameters(parameters);
    });
  };

  that.prepareCreateOffer = () => Promise.resolve();

  return that;
};

export default FirefoxStack;
