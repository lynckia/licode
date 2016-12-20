'use strict';
var addon = require('./../erizoAPI/build/Release/addon');
var licodeConfig = require('./../licode_config');
var mediaConfig = require('./../rtp_media_config');
var logger = require('./logger').logger;
var log = logger.getLogger('NativeClient');

GLOBAL.config = licodeConfig || {};
GLOBAL.mediaConfig = mediaConfig || {};

exports.ErizoNativeConnection = function (spec){
    var that = {},
    wrtc,
    initWebRtcConnection,
    externalInput,
    externalOutput;

    var threadPool = new addon.ThreadPool(1);
    threadPool.start();

    var CONN_INITIAL = 101,
        // CONN_STARTED = 102,
        CONN_GATHERED = 103,
        CONN_READY = 104,
        // CONN_FINISHED = 105,
        CONN_CANDIDATE = 201,
        CONN_SDP = 202,
        CONN_FAILED = 500;

    var generatePLIs = function(){
        externalOutput.interval = setInterval ( function(){
            wrtc.generatePLIPacket();
        },1000);
    };

    log.info('NativeConnection constructor', spec);
    initWebRtcConnection = function (callback) {
        wrtc.init( function (newStatus, mess){
            log.info('webrtc Addon status ', newStatus);
            switch(newStatus) {
                case CONN_INITIAL:
                    if (spec.video && spec.video.file && !externalInput){
                        that.prepareVideo(spec.video.file);
                    } else if (spec.video && spec.video.recording && !externalOutput){
                        that.prepareRecording(spec.video.recording);
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
                    log.warn('Connection failed the ICE process');
                    //   callback('callback', {type: 'failed', sdp: mess});
                    break;

                case CONN_READY:
                    log.info('Connection ready');
                    if (externalInput !== undefined){
                        log.info('Will start External Input');
                        externalInput.init();
                    }
                    if (externalOutput !== undefined){
                        log.info('Will start External Output');
                        externalOutput.init();
                        generatePLIs();
                    }
                    break;
            }
        });
    };


    wrtc = new addon.WebRtcConnection(threadPool, 'spine',
                                      GLOBAL.config.erizo.stunserver,
                                      GLOBAL.config.erizo.stunport,
                                      GLOBAL.config.erizo.minport,
                                      GLOBAL.config.erizo.maxport,
                                      false,
                                      JSON.stringify(GLOBAL.mediaConfig),
                                      GLOBAL.config.erizo.turnserver,
                                      GLOBAL.config.erizo.turnport,
                                      GLOBAL.config.erizo.turnusername,
                                      GLOBAL.config.erizo.turnpass);

    that.createOffer = function () {

    };

    that.prepareVideo = function (url) {
        log.info('Preparing video', url);
        externalInput = new addon.ExternalInput(url);
        externalInput.setAudioReceiver(wrtc);
        externalInput.setVideoReceiver(wrtc);
    };

    that.prepareRecording = function (url) {
        log.info('Preparing Recording', url);
        externalOutput = new addon.ExternalOutput(url);
        wrtc.setVideoReceiver(externalOutput);
        wrtc.setAudioReceiver(externalOutput);
    };

    that.setRemoteDescription = function () {
        log.info('RemoteDescription');
    };

    that.processSignallingMessage = function(msg) {
        log.info('Receiving message', msg.type);
        if (msg.type === 'started'){
            initWebRtcConnection(function(mess, info){
                log.info('Message from wrtc', info.type);
                if (info.type === 'offer') {
                     spec.callback({type:info.type, sdp: info.sdp});
                }
            }, {});

            var audioEnabled = true;
            var videoEnabled = true;
            var bundle = true;
            wrtc.createOffer(audioEnabled, videoEnabled, bundle);
        } else if (msg.type === 'answer'){
            setTimeout(function(){
                log.info('Passing delayed answer');
                wrtc.setRemoteSdp(msg.sdp);
                that.onaddstream({stream:{active:true}});
            }, 10);
        }
    };

    that.close = function() {
        if (externalOutput){
            externalOutput.close();
            if(externalOutput.interval)
                clearInterval(externalOutput.interval);
        }
        if (externalInput!==undefined){
            externalInput.close();
        }
        wrtc.close();
    };

    return that;
};
