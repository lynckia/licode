import Logger from '../utils/Logger';
import ChromeStableStack from './ChromeStableStack';

const log = Logger.module('SafariStack');
const SafariStack = (specInput) => {
  log.debug('message: Starting Safari stack');
  const that = ChromeStableStack(specInput);

  that._updateTracksToBeNegotiatedFromStream = () => {
    that.tracksToBeNegotiated += 1;
  };

  return that;
};

export default SafariStack;
