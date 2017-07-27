import Logger from '../utils/Logger';
import BaseStack from './BaseStack';

const FirefoxStack = (specInput) => {
  Logger.info('Starting Firefox stack');
  const that = BaseStack(specInput);
  const spec = specInput;

  that.enableSimulcast = (sdp) => {
    if (!spec.video || !spec.simulcast) {
      return sdp;
    }
    that.peerConnection.getSenders().forEach((sender) => {
      if (sender.track.kind === 'video') {
        sender.getParameters();
        sender.setParameters({ encodings: [{
          rid: '10001',
          active: true,
          priority: 'medium',
          maxBitrate: 250000 },
        {
          rid: '10000',
          active: true,
          priority: 'high',
          maxBitrate: 200000 }],
        });
      }
    });
    return sdp;
  };
  return that;
};

export default FirefoxStack;
