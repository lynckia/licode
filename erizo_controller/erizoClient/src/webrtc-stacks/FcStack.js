import Logger from '../utils/Logger';

const log = Logger.module('FcStack');
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
    log.info('Close FcStack');
  };

  that.createOffer = () => {
    log.debug('FCSTACK: CreateOffer');
  };

  that.addStream = (stream) => {
    log.debug('FCSTACK: addStream', stream);
  };

  that.processSignalingMessage = (msg) => {
    log.debug('FCSTACK: processSignaling', msg);
    if (that.signalCallback !== undefined) { that.signalCallback(msg); }
  };

  that.sendSignalingMessage = (msg) => {
    log.debug('FCSTACK: Sending signaling Message', msg);
    spec.callback(msg);
  };

  that.setSignalingCallback = (callback = () => {}) => {
    log.debug('FCSTACK: Setting signalling callback');
    that.signalCallback = callback;
  };
  return that;
};

export default FcStack;
