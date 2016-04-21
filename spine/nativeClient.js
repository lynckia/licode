var addon = require('./../erizoAPI/build/Release/addon');
var licodeConfig = require('./../licode_config');
GLOBAL.config = licodeConfig || {};

exports.ErizoNativeConnection = function (spec){
    "use strict";
    var that = {},
    wrtc, 
    initWebRtcConnection,
    externalInput = undefined;
    
    var CONN_INITIAL = 101, CONN_STARTED = 102,CONN_GATHERED = 103, CONN_READY = 104, CONN_FINISHED = 105, CONN_CANDIDATE = 201, CONN_SDP = 202, CONN_FAILED = 500;
    
    console.log("NativeConnection constructor", spec);
    initWebRtcConnection = function (callback, options) {
        wrtc.init( function (newStatus, mess){
            console.log("webrtc Addon status ", newStatus);
            switch(newStatus) {
                case CONN_INITIAL:
                    if (spec.video && spec.video.file && !externalInput){
                        that.prepareVideo(spec.video.file);
                    }
                    callback('callback', {type: 'started'});
                    break;

                case CONN_SDP:
                case CONN_GATHERED:
                    setTimeout(function(){
                        callback('callback', {type: 'offer', sdp: mess});
                    }, 100);
                    break;

                case CONN_CANDIDATE:
                    callback('callback', {type:'candidate', sdp: mess});
                    break;

                case CONN_FAILED:
                    console.warn("Connection failed the ICE process");
                    //   callback('callback', {type: 'failed', sdp: mess});
                    break;

                case CONN_READY:
                    console.log("Connection ready");
                    if (externalInput!==undefined){
                        console.log("Will start External Input");
                        externalInput.init();
                    }
                    break;
            }
        });
    };

    
    wrtc = new addon.WebRtcConnection(true, true, GLOBAL.config.erizo.stunserver, GLOBAL.config.erizo.stunport, GLOBAL.config.erizo.minport, GLOBAL.config.erizo.maxport,false,
            GLOBAL.config.erizo.turnserver, GLOBAL.config.erizo.turnport, GLOBAL.config.erizo.turnusername, GLOBAL.config.erizo.turnpass);
    
    that.createOffer = function (config) {

    };

    that.prepareVideo = function (url) {
        console.log("Preparing video", url);
        externalInput = new addon.ExternalInput(url);
        externalInput.setAudioReceiver(wrtc);
        externalInput.setVideoReceiver(wrtc);
    };



    that.setRemoteDescription = function (sdp) {
        console.log("RemoteDescription");
    };

    that.processSignallingMessage = function(msg) {
        console.log("Receiving message", msg.type);
        if (msg.type === 'started'){
            initWebRtcConnection(function(mess, info){
                console.log("Message from wrtc", info.type);
                if (info.type == 'offer') {
                     spec.callback({type:info.type, sdp: info.sdp});
                }

            }, {});

            wrtc.createOffer();
        } else if (msg.type === 'answer'){
            setTimeout(function(){
                console.log("Passing delayed answer");
                wrtc.setRemoteSdp(msg.sdp);
            }, 10);
        }
    };



    return that;
};
