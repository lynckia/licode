/* global L, Erizo*/
this.Erizo = this.Erizo || {};

Erizo.FcStack = (spec) => {
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
    L.Logger.info('Close FcStack');
  };

  that.createOffer = () => {
    L.Logger.debug('FCSTACK: CreateOffer');
  };

  that.addStream = (stream) => {
    L.Logger.debug('FCSTACK: addStream', stream);
  };

  that.processSignalingMessage = (msg) => {
    L.Logger.debug('FCSTACK: processSignaling', msg);
    if (that.signalCallback !== undefined) { that.signalCallback(msg); }
  };

  that.sendSignalingMessage = (msg) => {
    L.Logger.debug('FCSTACK: Sending signaling Message', msg);
    spec.callback(msg);
  };

  that.setSignalingCallback = (callback) => {
    L.Logger.debug('FCSTACK: Setting signalling callback');
    that.signalCallback = callback;
  };
  return that;
};
