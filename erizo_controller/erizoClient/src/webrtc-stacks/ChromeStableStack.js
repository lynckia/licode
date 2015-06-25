/*global window, console, RTCSessionDescription, RoapConnection, webkitRTCPeerConnection*/

var Erizo = Erizo || {};

Erizo.ChromeStableStack = function (spec) {
    "use strict";

    var that = {},
        WebkitRTCPeerConnection = webkitRTCPeerConnection;

    that.pc_config = {
        "iceServers": []
    };


    that.con = {'optional': [{'DtlsSrtpKeyAgreement': true}]};

    if (spec.stunServerUrl !== undefined) {
        that.pc_config.iceServers.push({"url": spec.stunServerUrl});
    }

    if ((spec.turnServer || {}).url) {
        that.pc_config.iceServers.push({
            "username": spec.turnServer.username,
            "credential": spec.turnServer.password,
            "url": spec.turnServer.url
        });
    }

    if (spec.audio === undefined) {
        spec.audio = true;
    }

    if (spec.video === undefined) {
        spec.video = true;
    }

    that.mediaConstraints = {
        mandatory: {
            'OfferToReceiveVideo': spec.video,
            'OfferToReceiveAudio': spec.audio
        }
    };

    var errorCallback = function (message) {
        console.log("Error in Stack ", message);
    }

    that.peerConnection = new WebkitRTCPeerConnection(that.pc_config, that.con);

    var setMaxBW = function (sdp) {
        if (spec.video && spec.maxVideoBW) {
            sdp = sdp.replace(/b=AS:.*\r\n/g, "");
            var a = sdp.match(/m=video.*\r\n/);
            if (a == null) {
                a = sdp.match(/m=video.*\n/);
            }
            if (a && (a.length > 0)) {
                var r = a[0] + "b=AS:" + spec.maxVideoBW + "\r\n";
                sdp = sdp.replace(a[0], r);
            }
        }

        if (spec.audio && spec.maxAudioBW) {
            var a = sdp.match(/m=audio.*\r\n/);
            if (a == null) {
                a = sdp.match(/m=audio.*\n/);
            }
            if (a && (a.length > 0)) {
                var r = a[0] + "b=AS:" + spec.maxAudioBW + "\r\n";
                sdp = sdp.replace(a[0], r);
            }
        }

        return sdp;
    };

    /**
     * Closes the connection.
     */
    that.close = function () {
        that.state = 'closed';
        that.peerConnection.close();
    };

    spec.localCandidates = [];

    that.peerConnection.onicecandidate = function (event) {
        if (event.candidate) {

            if (!event.candidate.candidate.match(/a=/)) {
                event.candidate.candidate = "a=" + event.candidate.candidate;
            }
            ;

            var candidateObject = {
                sdpMLineIndex: event.candidate.sdpMLineIndex,
                sdpMid: event.candidate.sdpMid,
                candidate: event.candidate.candidate
            };

            if (spec.remoteDescriptionSet) {
                spec.callback({type: 'candidate', candidate: candidateObject});
            } else {
                spec.localCandidates.push(candidateObject);
                L.Logger.info("Storing candidate: ", spec.localCandidates.length, candidateObject);
            }

        } else {
            console.log("End of candidates.");
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

    var localDesc;
    var remoteDesc;

    var setLocalDesc = function (sessionDescription) {
        sessionDescription.sdp = setMaxBW(sessionDescription.sdp);
        sessionDescription.sdp = sessionDescription.sdp.replace(/a=ice-options:google-ice\r\n/g, "");
        spec.callback({
            type: sessionDescription.type,
            sdp: sessionDescription.sdp
        });
        localDesc = sessionDescription;
        //that.peerConnection.setLocalDescription(sessionDescription);
    }

    var setLocalDescp2p = function (sessionDescription) {
        sessionDescription.sdp = setMaxBW(sessionDescription.sdp);
        spec.callback({
            type: sessionDescription.type,
            sdp: sessionDescription.sdp
        });
        localDesc = sessionDescription;
        that.peerConnection.setLocalDescription(sessionDescription);
    }

    that.updateSpec = function (config, callback){
        if (config.maxVideoBW || config.maxAudioBW ){
            if (config.maxVideoBW){
                L.Logger.debug ("Maxvideo Requested", config.maxVideoBW, "limit", spec.limitMaxVideoBW);
                if (config.maxVideoBW > spec.limitMaxVideoBW) {
                    config.maxVideoBW = spec.limitMaxVideoBW;
                }
                spec.maxVideoBW = config.maxVideoBW; 
                L.Logger.debug ("Result", spec.maxVideoBW);
            }
            if (config.maxAudioBW) {
                if (config.maxAudioBW > spec.limitMaxAudioBW) {
                    config.maxAudioBW = spec.limitMaxAudioBW;
                }
                spec.maxAudioBW = config.maxAudioBW;
            }

            localDesc.sdp = setMaxBW(localDesc.sdp);
            that.peerConnection.setLocalDescription(localDesc, function () {
                remoteDesc.sdp = setMaxBW(remoteDesc.sdp);
                that.peerConnection.setRemoteDescription(new RTCSessionDescription(remoteDesc), function () {
                    spec.remoteDescriptionSet = true;
                    spec.callback({type:'updatestream', sdp: localDesc.sdp});
                });
            }, function (error){
                L.Logger.error("Error updating configuration", error);
                callback('error');
            });
        }
        if (config.minVideoBW){
            L.Logger.debug ("MinVideo Changed to ", config.minVideoBW);
            spec.callback({type:'updatestream', minVideoBW: config.minVideoBW});
        }       
    };

    that.createOffer = function (isSubscribe) {
        if (isSubscribe === true) {
            that.peerConnection.createOffer(setLocalDesc, errorCallback, that.mediaConstraints);
        } else {
            that.peerConnection.createOffer(setLocalDesc, errorCallback);
        }

    };

    that.addStream = function (stream) {
        that.peerConnection.addStream(stream);
    };
    spec.remoteCandidates = [];

    spec.remoteDescriptionSet = false;

    that.processSignalingMessage = function (msg) {
        //L.Logger.info("Process Signaling Message", msg);

        if (msg.type === 'offer') {
            msg.sdp = setMaxBW(msg.sdp);
            that.peerConnection.setRemoteDescription(new RTCSessionDescription(msg), function () {
                that.peerConnection.createAnswer(setLocalDescp2p, function (error) {
                    L.Logger.error("Error: ", error);
                }, that.mediaConstraints);
                spec.remoteDescriptionSet = true;
            }, function (error) {
                L.Logger.error("Error setting Remote Description", error)
            });


        } else if (msg.type === 'answer') {


            // // For compatibility with only audio in Firefox Revisar
            // if (answer.match(/a=ssrc:55543/)) {
            //     answer = answer.replace(/a=sendrecv\\r\\na=mid:video/, 'a=recvonly\\r\\na=mid:video');
            //     answer = answer.split('a=ssrc:55543')[0] + '"}';
            // }

            console.log("Set remote and local description", msg.sdp);

            msg.sdp = setMaxBW(msg.sdp);

            remoteDesc = msg;
            that.peerConnection.setLocalDescription(localDesc, function () {
                that.peerConnection.setRemoteDescription(new RTCSessionDescription(msg), function () {
                    spec.remoteDescriptionSet = true;
                    console.log("Candidates to be added: ", spec.remoteCandidates.length, spec.remoteCandidates);
                    while (spec.remoteCandidates.length > 0) {
                        // IMPORTANT: preserve ordering of candidates
                        that.peerConnection.addIceCandidate(spec.remoteCandidates.shift());
                    }
                    console.log("Local candidates to send:", spec.localCandidates.length);
                    while (spec.localCandidates.length > 0) {
                        // IMPORTANT: preserve ordering of candidates
                        spec.callback({type: 'candidate', candidate: spec.localCandidates.shift()});
                    }

                });
            });

        } else if (msg.type === 'candidate') {
            try {
                var obj;
                if (typeof(msg.candidate) === 'object') {
                    obj = msg.candidate;
                } else {
                    obj = JSON.parse(msg.candidate);
                }
                obj.candidate = obj.candidate.replace(/a=/g, "");
                obj.sdpMLineIndex = parseInt(obj.sdpMLineIndex);
                var candidate = new RTCIceCandidate(obj);
                if (spec.remoteDescriptionSet) {
                    that.peerConnection.addIceCandidate(candidate);
                } else {
                    spec.remoteCandidates.push(candidate);
//                    console.log("Candidates stored: ", spec.remoteCandidates.length, spec.remoteCandidates);
                }
            } catch (e) {
                L.Logger.error("Error parsing candidate", msg.candidate);
            }
        }
    }

    return that;
};
