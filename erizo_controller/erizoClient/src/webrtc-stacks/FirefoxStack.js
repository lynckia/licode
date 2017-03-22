/*global L, mozRTCIceCandidate, mozRTCSessionDescription, mozRTCPeerConnection*/
'use strict';
var Erizo = Erizo || {};

Erizo.FirefoxStack = function (spec) {
    var that = {},
        WebkitRTCPeerConnection = mozRTCPeerConnection,
        RTCSessionDescription = mozRTCSessionDescription,
        RTCIceCandidate = mozRTCIceCandidate;

    that.pcConfig = {
        'iceServers': []
    };

    if (spec.iceServers !== undefined) {
        that.pcConfig.iceServers = spec.iceServers;
    }

    if (spec.audio === undefined) {
        spec.audio = true;
    }

    if (spec.video === undefined) {
        spec.video = true;
    }

    that.mediaConstraints = {
        offerToReceiveAudio: true,
        offerToReceiveVideo: true,
        mozDontOfferDataChannel: true
    };

    var errorCallback = function (message) {
        L.Logger.error('Error in Stack ', message);
    };

    var enableSimulcast = function () {
      if (!spec.video || !spec.simulcast) {
        return;
      }
      that.peerConnection.getSenders().forEach(function(sender) {
        if (sender.track.kind === 'video') {
          sender.getParameters();
          sender.setParameters({encodings: [{ 
            rid: 'spam', 
            active: true, 
            priority: 'high', 
            maxBitrate: 40000, 
            maxHeight: 640, 
            maxWidth: 480 },{ 
            rid: 'egg', 
            active: true, 
            priority: 'medium', 
            maxBitrate: 10000, 
            maxHeight: 320, 
            maxWidth: 240 }]
          });
        }
      });
    };


    var gotCandidate = false;
    that.peerConnection = new WebkitRTCPeerConnection(that.pcConfig, that.con);
    spec.localCandidates = [];

    that.peerConnection.onicecandidate =  function (event) {
        var candidateObject = {};
        if (!event.candidate) {
            L.Logger.info('Gathered all candidates. Sending END candidate');
            candidateObject = {
                sdpMLineIndex: -1 ,
                sdpMid: 'end',
                candidate: 'end'
            };
        }else{
            gotCandidate = true;
            if (!event.candidate.candidate.match(/a=/)) {
                event.candidate.candidate ='a=' + event.candidate.candidate;
            }
            candidateObject = event.candidate;
            if (spec.remoteDescriptionSet) {
                spec.callback({type:'candidate', candidate: candidateObject});
            } else {
                spec.localCandidates.push(candidateObject);
                L.Logger.debug('Local Candidates stored: ',
                               spec.localCandidates.length, spec.localCandidates);
            }

        }
    };


    that.peerConnection.onaddstream = function (stream) {
        if (that.onaddstream) {
            that.onaddstream(stream);
        }
    };

    that.peerConnection.onremovestream = function (stream) {
        if (that.onremovestream) {
            that.onremovestream(stream);
        }
    };

    that.peerConnection.oniceconnectionstatechange = function (ev) {
        if (that.oniceconnectionstatechange){
            that.oniceconnectionstatechange(ev.target.iceConnectionState);
        }
    };

    var setMaxBW = function (sdp) {
        var r, a;
        if (spec.video && spec.maxVideoBW) {
            sdp = sdp.replace(/b=AS:.*\r\n/g, '');
            a = sdp.match(/m=video.*\r\n/);
            if (a == null) {
                a = sdp.match(/m=video.*\n/);
            }
            if (a && (a.length > 0)) {
                r = a[0] + 'b=AS:' + spec.maxVideoBW + '\r\n';
                sdp = sdp.replace(a[0], r);
            }
        }

        if (spec.audio && spec.maxAudioBW) {
            a = sdp.match(/m=audio.*\r\n/);
            if (a == null) {
                a = sdp.match(/m=audio.*\n/);
            }
            if (a && (a.length > 0)) {
                r = a[0] + 'b=AS:' + spec.maxAudioBW + '\r\n';
                sdp = sdp.replace(a[0], r);
            }
        }

        return sdp;
    };

    var localDesc;

    var setLocalDesc = function (sessionDescription) {
        sessionDescription.sdp = setMaxBW(sessionDescription.sdp);
        sessionDescription.sdp =
              sessionDescription.sdp.replace(/a=ice-options:google-ice\r\n/g, '');
        spec.callback(sessionDescription);
        localDesc = sessionDescription;
    };

    var setLocalDescp2p = function (sessionDescription) {
        sessionDescription.sdp = setMaxBW(sessionDescription.sdp);
        sessionDescription.sdp =
              sessionDescription.sdp.replace(/a=ice-options:google-ice\r\n/g, '');
        spec.callback(sessionDescription);
        localDesc = sessionDescription;
        that.peerConnection.setLocalDescription(localDesc);
    };

    that.updateSpec = function (config){
        if (config.maxVideoBW || config.maxAudioBW ){
            if (config.maxVideoBW){
                L.Logger.debug ('Maxvideo Requested', config.maxVideoBW,
                                'limit', spec.limitMaxVideoBW);
                if (config.maxVideoBW > spec.limitMaxVideoBW) {
                    config.maxVideoBW = spec.limitMaxVideoBW;
                }
                spec.maxVideoBW = config.maxVideoBW;
                L.Logger.debug ('Result', spec.maxVideoBW);
            }
            if (config.maxAudioBW) {
                if (config.maxAudioBW > spec.limitMaxAudioBW) {
                    config.maxAudioBW = spec.limitMaxAudioBW;
                }
                spec.maxAudioBW = config.maxAudioBW;
            }

            localDesc.sdp = setMaxBW(localDesc.sdp);
            if (config.Sdp){
                L.Logger.error ('Cannot update with renegotiation in Firefox, ' +
                                'try without renegotiation');
            } else {
                L.Logger.debug ('Updating without renegotiation, newVideoBW:', spec.maxVideoBW,
                                ', newAudioBW:', spec.maxAudioBW);
                spec.callback({type:'updatestream', sdp: localDesc.sdp});
            }
        }
        if (config.minVideoBW || (config.slideShowMode!==undefined) ||
            (config.muteStream !== undefined) || (config.qualityLayer !== undefined)){
            L.Logger.debug ('MinVideo Changed to ', config.minVideoBW);
            L.Logger.debug ('SlideShowMode Changed to ', config.slideShowMode);
            L.Logger.debug ('muteStream changed to ', config.muteStream);
            spec.callback({type: 'updatestream', config:config});
        }
    };

    that.createOffer = function (isSubscribe) {
        if (isSubscribe === true) {
            that.peerConnection.createOffer(setLocalDesc, errorCallback, that.mediaConstraints);
        } else {
            enableSimulcast();
            that.mediaConstraints = {
                offerToReceiveAudio: false,
                offerToReceiveVideo: false,
                mozDontOfferDataChannel: true
            };
            that.peerConnection.createOffer(setLocalDesc, errorCallback, that.mediaConstraints);
        }
    };

    that.addStream = function (stream) {
        that.peerConnection.addStream(stream);
    };
    spec.remoteCandidates = [];
    spec.remoteDescriptionSet = false;

    /**
     * Closes the connection.
     */
    that.close = function () {
        that.state = 'closed';
        that.peerConnection.close();
    };

    that.processSignalingMessage = function (msg) {

//        L.Logger.debug("Process Signaling Message", msg);

        if (msg.type === 'offer') {
            msg.sdp = setMaxBW(msg.sdp);
            that.peerConnection.setRemoteDescription(new RTCSessionDescription(msg), function(){
                that.peerConnection.createAnswer(setLocalDescp2p, function(error){
                L.Logger.error('Error', error);
            }, that.mediaConstraints);
                spec.remoteDescriptionSet = true;
            }, function(error){
              L.Logger.error('Error setting Remote Description', error);
            });
        } else if (msg.type === 'answer') {

            L.Logger.info('Set remote and local description');
            L.Logger.debug('Local Description to set', localDesc.sdp);
            L.Logger.debug('Remote Description to set', msg.sdp);

            msg.sdp = setMaxBW(msg.sdp);

            that.peerConnection.setLocalDescription(localDesc, function(){
                that.peerConnection.setRemoteDescription(
                  new RTCSessionDescription(msg), function() {
                    spec.remoteDescriptionSet = true;
                    L.Logger.info('Remote Description successfully set');
                    while (spec.remoteCandidates.length > 0 && gotCandidate) {
                        L.Logger.info('Setting stored remote candidates');
                        // IMPORTANT: preserve ordering of candidates
                        that.peerConnection.addIceCandidate(spec.remoteCandidates.shift());
                    }
                    while(spec.localCandidates.length > 0) {
                        L.Logger.info('Sending Candidate from list');
                        // IMPORTANT: preserve ordering of candidates
                        spec.callback({type:'candidate', candidate: spec.localCandidates.shift()});
                    }
                }, function (error){
                    L.Logger.error('Error Setting Remote Description', error);
                });
            },function(error){
               L.Logger.error('Failure setting Local Description', error);
            });

        } else if (msg.type === 'candidate') {

            try {
                var obj;
                if (typeof(msg.candidate) === 'object') {
                    obj = msg.candidate;
                } else {
                    obj = JSON.parse(msg.candidate);
                }
                obj.candidate = obj.candidate.replace(/ generation 0/g, '');
                obj.candidate = obj.candidate.replace(/ udp /g, ' UDP ');

                obj.sdpMLineIndex = parseInt(obj.sdpMLineIndex);
                var candidate = new RTCIceCandidate(obj);
//                L.logger.debug("Remote Candidate",candidate);

                if (spec.remoteDescriptionSet && gotCandidate) {
                    that.peerConnection.addIceCandidate(candidate);
                    while (spec.remoteCandidates.length > 0) {
                        L.Logger.info('Setting stored remote candidates');
                        // IMPORTANT: preserve ordering of candidates
                        that.peerConnection.addIceCandidate(spec.remoteCandidates.shift());
                    }
                } else {
                    spec.remoteCandidates.push(candidate);
                }
            } catch(e) {
                L.Logger.error('Error parsing candidate', msg.candidate, e);
            }
        }
    };
    return that;
};
