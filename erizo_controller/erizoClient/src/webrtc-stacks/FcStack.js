'use strict';
var Erizo = Erizo || {};

Erizo.FcStack = function (spec) {
/*
        spec.callback({
            type: sessionDescription.type,
            sdp: sessionDescription.sdp
        });
*/
    var that = {};

    that.pcConfig = {};

    that.peerConnection = {};
    that.desc = {};
    that.signalCallback = undefined;

    var L = L || {};

    that.close = function() {
        L.Logger.info('Close FcStack');
    };

    that.createOffer = function() {
        L.Logger.debug('FCSTACK: CreateOffer');
    };

    that.addStream = function(stream) {
        L.Logger.debug('FCSTACK: addStream', stream);
    };

    that.processSignalingMessage = function(msg) {
        L.Logger.debug('FCSTACK: processSignaling', msg);
        if(that.signalCallback !== undefined)
            that.signalCallback(msg);
    };

    that.sendSignalingMessage = function(msg) {
        L.Logger.debug('FCSTACK: Sending signaling Message', msg);
        spec.callback(msg);
    };

    that.setSignalingCallback = function(callback) {
        L.Logger.debug('FCSTACK: Setting signalling callback');
        that.signalCallback = callback;
    };
    return that;
};
