'use strict';
var ErizoNativeConnection = require ('./nativeClient');
// Logger
var logger = require('./logger').logger;
var log = logger.getLogger('NativeStack');

var NativeStack = function (spec) {
    var that = {};
    log.info('Creating a NativeStack', spec);

    that.pcConfig = {
        'iceServers': []
    };

    if (spec.iceServers !== undefined) {
        that.pcConfig.iceServers = spec.iceServers;
    }

    if (spec.audio === undefined) {
        spec.audio = false;
    }

    if (spec.video === undefined) {
        spec.video = false;
    }

    that.peerConnection = ErizoNativeConnection.ErizoNativeConnection(spec) ;
    that.desc = {};
    that.callback = undefined;

    that.close = function(){
        log.info('Close NATIVE');
        if (that.peerConnection){
            that.peerConnection.close();
        } else {
            log.error('Trying to close with no underlying PC!');
        }
    };

    that.stop = function(){
        that.close();
    };

    that.createOffer = function(){
        log.info('NATIVESTACK: CreateOffer');
    };

    that.addStream = function(){
        log.info('NATIVESTACK: addStream');
    };

    that.processSignalingMessage = function(msg){
        log.info('NATIVESTACK: processSignaling', msg.type);
        that.peerConnection.processSignallingMessage(msg);
    };

    that.sendSignalingMessage = function(){
        log.info('NATIVESTACK: Sending signaling Message');
    };

    that.peerConnection.onaddstream = function (stream) {
        if (that.onaddstream) {
            that.onaddstream(stream);
        }
    };

    return that;
};
exports.FakeConnection = function(spec){
    log.info('Creating Connection');
    var sessionId = 0;
    spec.sessionId = sessionId++;
    return NativeStack(spec); // jshint ignore:line
};

exports.GetUserMedia = function(opt, callback){
    log.info('Fake getUserMedia to use with files', opt);
    // if (that.peerConnection && opt.video.file){
    //     that.peerConnection.prepareVideo(opt.video.file);
    // }
    callback('');
};
