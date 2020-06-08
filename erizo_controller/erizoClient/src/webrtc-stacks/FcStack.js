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
    log.debug('message: Close FcStack');
  };

  that.createOffer = () => {
    log.debug('message: CreateOffer');
  };

  that.addStream = (stream) => {
    log.debug(`message: addStream, ${stream.toLog()}`);
  };

  that.processSignalingMessage = (msg) => {
    log.debug(`message: processSignaling, message: ${msg}`);
    if (that.signalCallback !== undefined) { that.signalCallback(msg); }
  };

  that.sendSignalingMessage = (msg) => {
    log.debug(`message: Sending signaling Message, message: ${msg}`);
    spec.callback(msg);
  };

  that.setSignalingCallback = (callback = () => {}) => {
    log.debug('message: Setting signalling callback');
    that.signalCallback = callback;
  };
  return that;
};

export default FcStack;
