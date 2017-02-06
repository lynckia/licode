/*global L, console, RTCSessionDescription, webkitRTCPeerConnection, RTCIceCandidate*/
'use strict';
var Erizo = Erizo || {};

Erizo.BowserStack = function (spec) {
    var that = {},
        WebkitRTCPeerConnection = webkitRTCPeerConnection;

    that.pcConfig = {
        'iceServers': []
    };

    that.con = {'optional': [{'DtlsSrtpKeyAgreement': true}]};

    if (spec.stunServerUrl !== undefined) {
        that.pcConfig.iceServers.push({'url': spec.stunServerUrl});
    }

    if ((spec.turnServer || {}).url) {
        that.pcConfig.iceServers.push({'username': spec.turnServer.username,
                                       'credential': spec.turnServer.password,
                                       'url': spec.turnServer.url});
    }

    if (spec.audio === undefined) {
        spec.audio = true;
    }

    if (spec.video === undefined) {
        spec.video = true;
    }

    that.mediaConstraints = {
            'offerToReceiveVideo': spec.video,
            'offerToReceiveAudio': spec.audio
    };

    that.peerConnection = new WebkitRTCPeerConnection(that.pcConfig, that.con);

    spec.remoteDescriptionSet = false;

    var setMaxBW = function (sdp) {
        var a, r;
        if (spec.maxVideoBW) {
            a = sdp.match(/m=video.*\r\n/);
            if (a == null){
              a = sdp.match(/m=video.*\n/);
            }
            if (a && (a.length > 0)) {
                r = a[0] + 'b=AS:' + spec.maxVideoBW + '\r\n';
                sdp = sdp.replace(a[0], r);
            }
        }

        if (spec.maxAudioBW) {
            a = sdp.match(/m=audio.*\r\n/);
            if (a == null){
              a = sdp.match(/m=audio.*\n/);
            }
            if (a && (a.length > 0)) {
                r = a[0] + 'b=AS:' + spec.maxAudioBW + '\r\n';
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

    that.peerConnection.onicecandidate =  function (event) {
        if (event.candidate) {
            if (!event.candidate.candidate.match(/a=/)) {
                event.candidate.candidate ='a=' + event.candidate.candidate;
            }


            if (spec.remoteDescriptionSet) {
                spec.callback({type:'candidate', candidate: event.candidate});
            } else {
                spec.localCandidates.push(event.candidate);
//                console.log('Local Candidates stored: ',
//                             spec.localCandidates.length, spec.localCandidates);
            }

        } else {

          //  spec.callback(that.peerConnection.localDescription);
            console.log('End of candidates.' , that.peerConnection.localDescription);
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

    var errorCallback = function(message){
      console.log('Error in Stack ', message);
    };

    var localDesc;

    var setLocalDesc = function (sessionDescription) {
        sessionDescription.sdp = setMaxBW(sessionDescription.sdp);
//      sessionDescription.sdp = sessionDescription.sdp
//                                        .replace(/a=ice-options:google-ice\r\n/g, '');
        console.log('Set local description', sessionDescription.sdp);
        localDesc = sessionDescription;
        that.peerConnection.setLocalDescription(localDesc, function(){
          console.log('The final LocalDesc', that.peerConnection.localDescription);
          spec.callback(that.peerConnection.localDescription);
        }, errorCallback);
        //that.peerConnection.setLocalDescription(sessionDescription);
    };

    var setLocalDescp2p = function (sessionDescription) {
        sessionDescription.sdp = setMaxBW(sessionDescription.sdp);
//        sessionDescription.sdp = sessionDescription.sdp
//                                          .replace(/a=ice-options:google-ice\r\n/g, "");
        spec.callback(sessionDescription);
        localDesc = sessionDescription;
        that.peerConnection.setLocalDescription(sessionDescription);
    };

    that.createOffer = function (isSubscribe) {
      if (isSubscribe===true)
        that.peerConnection.createOffer(setLocalDesc, errorCallback, that.mediaConstraints);
      else
        that.peerConnection.createOffer(setLocalDesc, errorCallback);

    };

    that.addStream = function (stream) {
        that.peerConnection.addStream(stream);
    };
    spec.remoteCandidates = [];


    that.processSignalingMessage = function (msg) {
       console.log('Process Signaling Message', msg);

        if (msg.type === 'offer') {
            msg.sdp = setMaxBW(msg.sdp);
            that.peerConnection.setRemoteDescription(new RTCSessionDescription(msg));
            that.peerConnection.createAnswer(setLocalDescp2p, null, that.mediaConstraints);
            spec.remoteDescriptionSet = true;

        } else if (msg.type === 'answer') {

            console.log('Set remote description', msg.sdp);

            msg.sdp = setMaxBW(msg.sdp);

            that.peerConnection.setRemoteDescription(new RTCSessionDescription(msg), function() {
              spec.remoteDescriptionSet = true;
              console.log('Candidates to be added: ', spec.remoteCandidates.length);
              while (spec.remoteCandidates.length > 0) {
                console.log('Candidate :', spec.remoteCandidates[spec.remoteCandidates.length-1]);
                that.peerConnection.addIceCandidate(spec.remoteCandidates.shift(),
                                                    function(){},
                                                    errorCallback);

              }
//              console.log('Local candidates to send:' , spec.localCandidates.length);
              while(spec.localCandidates.length > 0) {
                spec.callback({type:'candidate', candidate: spec.localCandidates.shift()});
              }

            }, function(){console.log('Error Setting Remote Description');});

        } else if (msg.type === 'candidate') {
          console.log('Message with candidate');
            try {
                var obj;
                if (typeof(msg.candidate) === 'object') {
                    obj = msg.candidate;
                } else {
                    obj = JSON.parse(msg.candidate);
                }
//                obj.candidate = obj.candidate.replace(/ generation 0/g, "");
//                obj.candidate = obj.candidate.replace(/ udp /g, " UDP ");
                obj.candidate = obj.candidate.replace(/a=/g, '');
                obj.sdpMLineIndex = parseInt(obj.sdpMLineIndex);
                obj.sdpMLineIndex = obj.sdpMid === 'audio' ? 0 : 1;
                var candidate = new RTCIceCandidate(obj);
                console.log('Remote Candidate', candidate);
                if (spec.remoteDescriptionSet) {
                    that.peerConnection.addIceCandidate(candidate, function(){}, errorCallback);
                } else {
                    spec.remoteCandidates.push(candidate);
                }
            } catch(e) {
                L.Logger.error('Error parsing candidate', msg.candidate);
            }
        }
    };

    return that;
};
