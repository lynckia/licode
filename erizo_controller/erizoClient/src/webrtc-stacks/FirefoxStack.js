import Logger from '../utils/Logger';
import BaseStack from './BaseStack';

const log = Logger.module('FirefoxStack');
const FirefoxStack = (specInput) => {
  log.debug('message: Starting Firefox stack');
  const that = BaseStack(specInput);


  that.enableSimulcast = sdp => sdp;

  that.prepareCreateOffer = () => Promise.resolve();

  // private functions
  that._updateTracksToBeNegotiatedFromStream = () => {
    that.tracksToBeNegotiated += 1;
  };

  return that;
};

export default FirefoxStack;
