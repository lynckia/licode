import Logger from '../utils/Logger';

const FcStack = (spec) => {
  /*
  spec.callback({
      type: sessionDescription.type,
      sdp: sessionDescription.sdp
  });
  */
  const that = {};

  that.pcConfig = {};

  that.peerConnection = {};
  that.desc = {};
  that.signalCallback = undefined;

  that.close = () => {
    Logger.info('Close FcStack');
  };

  that.createOffer = () => {
    Logger.debug('FCSTACK: CreateOffer');
  };

  that.addStream = (stream) => {
    Logger.debug('FCSTACK: addStream', stream);
  };

  that.processSignalingMessage = (msg) => {
    Logger.debug('FCSTACK: processSignaling', msg);
    if (that.signalCallback !== undefined) { that.signalCallback(msg); }
  };

  that.sendSignalingMessage = (msg) => {
    Logger.debug('FCSTACK: Sending signaling Message', msg);
    spec.callback(msg);
  };

  that.setSignalingCallback = (callback = () => {}) => {
    Logger.debug('FCSTACK: Setting signalling callback');
    that.signalCallback = callback;
  };
  return that;
};

export default FcStack;
