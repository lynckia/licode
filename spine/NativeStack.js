/*global window, console, RTCSessionDescription, RoapConnection, webkitRTCPeerConnection*/

var ErizoNativeConnection = require ("./nativeClient");

 var NativeStack = function (spec) {
    "use strict";
    
    var that = {};
    console.log("Creating a NativeStack", spec);

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
        console.log("Close NATIVE");
        if (that.peerConnection){
            that.peerConnection.close();
        } else {
            console.error("Trying to close with no underlying PC!");
        }
    }

    that.createOffer = function(isSubscribe){
        console.log("NATIVESTACK: CreateOffer");
    };

    that.addStream = function(stream){
        console.log("NATIVESTACK: addStream");
    };
    
    that.processSignalingMessage = function(msg){
        console.log("NATIVESTACK: processSignaling", msg.type);
        that.peerConnection.processSignallingMessage(msg);
    };

    that.sendSignalingMessage = function(msg){
        console.log("NATIVESTACK: Sending signaling Message");
    };

    return that;
};
exports.FakeConnection = function(spec){
    console.log("Creating Connection");
    var session_id = 0;
    spec.session_id = session_id++;
    return NativeStack(spec); 
};

exports.GetUserMedia = function(opt, callback, error){
    console.log("Fake getUserMedia to use with files", opt);
    if (that.peerConnection && opt.video.file){
        that.peerConnection.prepareVideo(opt.video.file);
    }
    callback("");
};
