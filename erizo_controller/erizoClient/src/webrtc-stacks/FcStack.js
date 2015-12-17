/*global window, console, RTCSessionDescription, RoapConnection, webkitRTCPeerConnection*/

var Erizo = Erizo || {};

Erizo.FcStack = function (spec) {
    "use strict";
/*
        spec.callback({
            type: sessionDescription.type,
            sdp: sessionDescription.sdp
        });
*/
    var that = {};

    that.pc_config = {};

    that.peerConnection = {};
    that.desc = {};

    that.close = function(){
        console.log("Close FcStack");
    }

    that.createOffer = function(isSubscribe){
        console.log("FCSTACK: CreateOffer");
    };

    that.addStream = function(stream){
        console.log("FCSTACK: addStream", stream);
    };
    
    that.processSignalingMessage = function(msg){
        console.log("FCSTACK: processSignaling", msg);
        desc = msg;
    }

    that.sendSignalingMessage = function(msg){
        console.log("FCSTACK: Sending signaling Message", msg);
        spec.callback(msg);
    };

    that.getDescription = function(){
        return desc;
    };
    
    return that;
};
