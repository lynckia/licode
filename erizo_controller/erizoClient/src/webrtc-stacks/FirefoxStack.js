/*global window, console, RTCSessionDescription, RoapConnection, webkitRTCPeerConnection*/

var Erizo = Erizo || {};

Erizo.FirefoxStack = function (spec) {
    "use strict";

    var that = {},
        WebkitRTCPeerConnection = mozRTCPeerConnection,
        RTCSessionDescription = mozRTCSessionDescription,
        RTCIceCandidate = mozRTCIceCandidate;

    var hasStream = false;

    that.pc_config = {
        "iceServers": []
    };

    if (spec.stunServerUrl !== undefined) {
        that.pc_config.iceServers.push({"url": spec.stunServerUrl});
    } 

    if ((spec.turnServer || {}).url) {
        that.pc_config.iceServers.push({"username": spec.turnServer.username, "credential": spec.turnServer.password, "url": spec.turnServer.url});
    }

    if (spec.audio === undefined) {
        spec.audio = true;
    }

    if (spec.video === undefined) {
        spec.video = true;
    }

    that.mediaConstraints = {
        offerToReceiveAudio: spec.audio,
        offerToReceiveVideo: spec.video,
        mozDontOfferDataChannel: true
    };

    that.roapSessionId = 103;

    that.peerConnection = new WebkitRTCPeerConnection(that.pc_config.iceServers);

    spec.localCandidates = [];

    that.peerConnection.onicecandidate =  function (event) {
        if (event.candidate) {
          L.Logger.info("EVENT CANDIDATE " , event.candidate);  
          if (spec.remoteDescriptionSet) {
                spec.callback({type:'candidate', candidate: event.candidate});
            } else {
                spec.localCandidates.push(event.candidate);
                console.log("Local Candidates stored: ", spec.localCandidates.length, spec.localCandidates);
            }

            

            // sendMessage({type: 'candidate',
            //        label: event.candidate.sdpMLineIndex,
            //        id: event.candidate.sdpMid,
            //        candidate: event.candidate.candidate});


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


    var setMaxBW = function (sdp) {
        if (spec.maxVideoBW) {
            var a = sdp.match(/m=video.*\r\n/);
            if (a == null){
              a = sdp.match(/m=video.*\n/);
            }
            var r = a[0] + "b=AS:" + spec.maxVideoBW + "\r\n";
            sdp = sdp.replace(a[0], r);
        }

        if (spec.maxAudioBW) {
            var a = sdp.match(/m=audio.*\r\n/);
            if (a == null){
              a = sdp.match(/m=audio.*\n/);
            }
            var r = a[0] + "b=AS:" + spec.maxAudioBW + "\r\n";
            sdp = sdp.replace(a[0], r);
        }

        return sdp;
    };

    var localDesc;

    var setLocalDesc = function (sessionDescription) {
        sessionDescription.sdp = setMaxBW(sessionDescription.sdp);
        sessionDescription.sdp = sessionDescription.sdp.replace(/a=ice-options:google-ice\r\n/g, "");
        spec.callback(sessionDescription);
        localDesc = sessionDescription;
    }

    that.createOffer = function () {
        that.peerConnection.createOffer(setLocalDesc, function(error){
          L.Logger.error("Error", error);
        
        }, that.mediaConstraints);
    };

    that.addStream = function (stream) {
        that.peerConnection.addStream(stream);
    };
    spec.remoteCandidates = [];
    spec.remoteDescriptionSet = false;

    that.processSignalingMessage = function (msg) {
//        console.log("Process Signaling Message", msg);

        // if (msg.type === 'offer') {
        //     msg.sdp = setMaxBW(msg.sdp);
        //     that.peerConnection.setRemoteDescription(new RTCSessionDescription(msg));
        //     that.peerConnection.createAnswer(setLocalAndSendMessage, null, sdpConstraints);
        // } else 

        if (msg.type === 'answer') {


            // // For compatibility with only audio in Firefox Revisar
            // if (answer.match(/a=ssrc:55543/)) {
            //     answer = answer.replace(/a=sendrecv\\r\\na=mid:video/, 'a=recvonly\\r\\na=mid:video');
            //     answer = answer.split('a=ssrc:55543')[0] + '"}';
            // }

            console.log("Set remote and local description", msg.sdp);

            msg.sdp = setMaxBW(msg.sdp);

            that.peerConnection.setLocalDescription(localDesc, function(){
              that.peerConnection.setRemoteDescription(new RTCSessionDescription(msg), function() {
                spec.remoteDescriptionSet = true;
                while (spec.remoteCandidates.length > 0) {
                  that.peerConnection.addIceCandidate(spec.remoteCandidates.pop());
                }

                while(spec.localCandidates.length > 0) {
                  spec.callback({type:'candidate', candidate: spec.localCandidates.pop()});
                }
              });
            });

        } else if (msg.type === 'candidate') {
          
            try {
                var obj = JSON.parse(msg.candidate);
                obj.candidate = obj.candidate.replace(/ generation 0/g, "");
                obj.candidate = obj.candidate.replace(/ udp /g, " UDP ");
                obj.sdpMLineIndex = parseInt(obj.sdpMLineIndex);
                var candidate = new RTCIceCandidate(obj);
                candidate.sdpMLineIndex = candidate.sdpMid=="audio"?0:1;
                console.log("Remote Candidate",candidate);

                if (spec.remoteDescriptionSet) {
                    that.peerConnection.addIceCandidate(candidate);
                } else {
                    spec.remoteCandidates.push(candidate);
                }
            } catch(e) {
                L.Logger.error("Error parsing candidate", msg.candidate, e);
            }
        }
    }
    return that;
};
