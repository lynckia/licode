import Logger from '../utils/Logger';
import BaseStack from './BaseStack';

const FirefoxStack = (specInput) => {
  Logger.info('Starting Firefox stack');
  const that = BaseStack(specInput);

  const enableSimulcast = () => {
    that.peerConnection.getSenders().forEach((sender) => {
      if (sender.track.kind === 'video') {
        sender.getParameters();
        sender.setParameters({ encodings: [{
          rid: 'spam',
          active: true,
          priority: 'high',
          maxBitrate: 600000,
          maxHeight: 640,
          maxWidth: 480 }, {
            rid: 'egg',
            active: true,
            priority: 'medium',
            maxBitrate: 600000,
            maxHeight: 320,
            maxWidth: 240 }],
        });
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
