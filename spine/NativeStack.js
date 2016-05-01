/*global window, console, RTCSessionDescription, RoapConnection, webkitRTCPeerConnection*/

var ErizoNativeConnection = require ("./nativeClient");
// Logger
var logger = require('./logger').logger;
var log = logger.getLogger("NativeStack");

var NativeStack = function (spec) {
    "use strict";

    var that = {};
    log.info("Creating a NativeStack", spec);

    that.pc_config = {
        "iceServers":[]
    };

    if (spec.iceServers !== undefined) {
        that.pc_config.iceServers = spec.iceServers;
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
        log.info("Close NATIVE");
        if (that.peerConnection){
            that.peerConnection.close();
        } else {
            log.error("Trying to close with no underlying PC!");
        }
    }

    that.stop = function(){
        that.close();
    }

    that.createOffer = function(isSubscribe){
        log.info("NATIVESTACK: CreateOffer");
    };

    that.addStream = function(stream){
        log.info("NATIVESTACK: addStream");
    };

    that.processSignalingMessage = function(msg){
        log.info("NATIVESTACK: processSignaling", msg.type);
        that.peerConnection.processSignallingMessage(msg);
    };

    that.sendSignalingMessage = function(msg){
        log.info("NATIVESTACK: Sending signaling Message");
    };

    that.peerConnection.onaddstream = function (stream) {
        if (that.onaddstream) {
            that.onaddstream(stream);
        }
    };

    return that;
};
exports.FakeConnection = function(spec){
    log.info("Creating Connection");
    var session_id = 0;
    spec.session_id = session_id++;
    return NativeStack(spec); 
};

exports.GetUserMedia = function(opt, callback, error){
    log.info("Fake getUserMedia to use with files", opt);
    if (that.peerConnection && opt.video.file){
        that.peerConnection.prepareVideo(opt.video.file);
    }
    callback("");
};
