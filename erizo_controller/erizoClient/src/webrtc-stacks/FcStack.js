/*global console*/
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

    that.close = function() {
        console.log('Close FcStack');
    };

    that.createOffer = function() {
        console.log('FCSTACK: CreateOffer');
    };

    that.addStream = function(stream) {
        console.log('FCSTACK: addStream', stream);
    };

    that.processSignalingMessage = function(msg) {
        console.log('FCSTACK: processSignaling', msg);
        if(that.signalCallback !== undefined)
            that.signalCallback(msg);
    };

    that.sendSignalingMessage = function(msg) {
        console.log('FCSTACK: Sending signaling Message', msg);
        spec.callback(msg);
    };

    that.setSignalingCallback = function(callback) {
        console.log('FCSTACK: Setting signalling callback');
        that.signalCallback = callback;
    };
    return that;
};
