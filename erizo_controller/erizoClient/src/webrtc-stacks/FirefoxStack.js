import Logger from '../utils/Logger';
import BaseStack from './BaseStack';

const FirefoxStack = (specInput) => {
  Logger.info('Starting Firefox stack');
  const that = BaseStack(specInput);
  const spec = specInput;

  that.enableSimulcast = () => {
    if (!spec.video || !spec.simulcast) {
      return;
    }
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

  that.createOffer = (isSubscribe) => {
    if (isSubscribe !== true) {
      that.enableSimulcast();
      that.mediaConstraints = {
        offerToReceiveVideo: false,
        offerToReceiveAudio: false,
      };
    }
    Logger.debug('Creating offer', that.mediaConstraints);
    that.peerConnection.createOffer(that.mediaConstraints)
    .then(that.setLocalDescForOffer.bind(null, isSubscribe))
    .catch(that.errorCallback.bind(null, 'Create Offer', undefined));
  };


  return that;
};

export default FirefoxStack;
